// MIT. Diagnostic only: observes native transactions, never requests one.
#include <vector>
#include <shroudtopia.h>
#include <MinHook.h>
#include <wincrypt.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <mutex>
#include "client_layout.h"
#include "events.h"

namespace Probe {
using Callback = void(__fastcall*)(void*);
using Init = void(__fastcall*)(void*, void*, std::uint32_t);
using Next = bool(__fastcall*)(void*, void*, std::uint32_t);
using CurrentId = std::uint32_t*(__fastcall*)(void*, void*);
using World = void*(__fastcall*)(void*);
using Lookup = void*(__fastcall*)(void*, void*, void*, std::uint32_t);
using SelectTarget = void*(__fastcall*)(void*, const void*, std::uint8_t);
using FindEntry = void*(__fastcall*)(void*, std::uint32_t);
using ReadEntry = void*(__fastcall*)(void*, const void*, const void*);
using ReadTemplate = void*(__fastcall*)(void*, void*, const void*);

std::array<Callback, Systems.size()> originals{};
std::array<std::atomic<unsigned long long>, Systems.size()> calls{};
std::atomic<bool> observing{false}, faulted{false};
std::atomic<unsigned long long> dropped{0}, nextOfferTick{0}, offerScans{0}, offerRows{0}, truncatedScans{0}, inputRows{0};
std::atomic<unsigned long long> actorRows{0}, truncatedActorScans{0};
std::atomic<unsigned long long> actionRows{0}, truncatedActionScans{0};
// Publish local identity and its observation time together, without retaining
// a game-owned pointer across callbacks or world changes.
std::atomic<std::uint64_t> localPlayerObservation{0};
std::mutex queueMutex;
std::array<Row, 512> queue{};
std::size_t queued = 0;
Init iterInit = nullptr;
Next iterNext = nullptr;
CurrentId currentId = nullptr;
World getWorld = nullptr;
Lookup lookupRead = nullptr;
SelectTarget selectTarget = nullptr;
FindEntry findEntry = nullptr;
ReadEntry readEntry = nullptr;
ReadTemplate readTemplate = nullptr;
unsigned char* image = nullptr;
bool prepared = false;

void Enqueue(const Row& row) {
    std::unique_lock<std::mutex> lock(queueMutex, std::try_to_lock);
    if (!lock.owns_lock() || queued == queue.size()) { ++dropped; return; }
    queue[queued++] = row;
}

bool Foreground() {
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId();
}
using FocusCheck = bool(*)();
FocusCheck isForeground = Foreground;

// Each observer is entered by one named ECS system. A nonblocking guard also
// protects its change cache if that system is scheduled concurrently.
std::atomic_flag offersBusy = ATOMIC_FLAG_INIT, inputBusy = ATOMIC_FLAG_INIT, actorBusy = ATOMIC_FLAG_INIT;
std::atomic_flag actionBusy = ATOMIC_FLAG_INIT;
struct OfferState { std::uint32_t target{}, player{}, acceptedBy{}, verb{}, offered{}; ULONGLONG seen{}; };
std::array<OfferState, 256> lastOffers{};
InputSnapshot lastInput{};
std::uint32_t lastInputPlayer = 0;
ActorSnapshot lastActor{};
std::uint32_t lastActorPlayer = 0;
ActionTargetSnapshot lastAction{};
std::uint32_t lastActionPlayer = 0;
SelectedOfferSnapshot lastSelected{};
std::uint32_t lastSelectedPlayer = 0;
ULONGLONG nextSelectedTick = 0;

bool ChangedOffer(const Row& row, ULONGLONG now) {
    auto& old = lastOffers[row.target % lastOffers.size()];
    const bool changed = old.target != row.target || old.player != row.player || old.acceptedBy != row.offer ||
        old.verb != row.verb || old.offered != row.flags || now - old.seen >= 5000;
    if (changed) old = {row.target, row.player, row.offer, row.verb, row.flags, now};
    return changed;
}

// These functions contain only POD locals so the SEH guard does not bypass C++ cleanup.
void ReadEvents(const void* list, std::uint32_t stride, Kind kind) {
    if (!list) return;
    const auto count = Read<std::uint32_t>(list, 0x28);
    if (count > 65536) { faulted = true; return; }
    const auto* data = Read<const unsigned char*>(list, 0x18);
    if (count && !data) { faulted = true; return; }
    const auto limit = (std::min)(count, MaxEventsPerCallback);
    dropped += count - limit;
    for (std::uint32_t index = 0; index < limit; ++index) {
        std::array<unsigned char, 232> copy{};
        if (stride > copy.size()) { faulted = true; return; }
        std::memcpy(copy.data(), data + static_cast<std::size_t>(index) * stride, stride);
        Row row{};
        if (DecodeEvent(kind, copy.data(), stride, row)) Enqueue(row);
    }
}

void ReadClientOffers(void* context, std::uintptr_t* record) {
    const auto now = GetTickCount64();
    auto next = nextOfferTick.load();
    if (now < next || !nextOfferTick.compare_exchange_strong(next, now + 100) || !isForeground()) return;
    if (!record[6] || !record[8]) return;
    const auto player = Read<std::uint32_t>(reinterpret_cast<void*>(record[6]), 4);
    if (!player || player > 1023) return;
    std::uintptr_t transform[2]{};
    lookupRead(transform, getWorld(*static_cast<void**>(context)), reinterpret_cast<void*>(record[8]), player);
    if (!transform[0]) return;
    std::int64_t playerPosition[3]{};
    std::memcpy(playerPosition, reinterpret_cast<void*>(transform[0]), sizeof(playerPosition));
    ++offerScans;
    // Diagnostic scans only the system's declared client offer rows. Never
    // retain components, write counters, or invoke an interaction.
    unsigned scanned = 0, emitted = 0;
    for (; scanned < 4096 && iterNext(context, record, 0x48); ++scanned) {
        ++offerRows;
        if (!record[1] || !record[2]) continue;
        std::uint32_t idBuffer[2]{};
        const auto* target = currentId(context, idBuffer);
        if (!target || !*target) continue;
        std::int64_t targetPosition[3]{};
        std::memcpy(targetPosition, reinterpret_cast<void*>(record[2]), sizeof(targetPosition));
        const auto distance = Distance(playerPosition, targetPosition);
        if (!std::isfinite(distance) || distance > 3.0) continue;
        Row row{};
        row.kind = Kind::ClientOffer;
        row.player = player;
        row.target = *target;
        const auto* offer = reinterpret_cast<void*>(record[1]);
        row.verb = Read<std::uint32_t>(offer, 0);
        row.offer = Read<std::uint32_t>(offer, 4); // lastAcceptionId; NOT an offer ID or killer ID.
        row.flags = Read<std::uint8_t>(offer, 8);
        row.distance = distance;
        if (emitted < MaxEventsPerCallback && ChangedOffer(row, now)) { Enqueue(row); ++emitted; }
    }
    if (scanned == 4096) ++truncatedScans;
}

void ReadClientInput(void* context, std::uintptr_t* record) {
    if (!isForeground() || !record[0xe0 / 8]) { localPlayerObservation = 0; return; }
    const auto player = Read<std::uint32_t>(reinterpret_cast<void*>(record[0xe0 / 8]), 4);
    if (!player || player > 1023) { lastInputPlayer = 0; localPlayerObservation = 0; return; }
    localPlayerObservation = (static_cast<std::uint64_t>(GetTickCount()) << 32) | player;
    for (unsigned count = 0; count < 32 && iterNext(context, record, 0x158); ++count) {
        std::uint32_t idBuffer[2]{};
        const auto* id = currentId(context, idBuffer);
        if (!id || *id != player || !record[3]) continue;
        ++inputRows;
        const auto* input = reinterpret_cast<void*>(record[3]);
        Row row{};
        row.kind = Kind::ClientInput;
        row.player = player;
        auto& snapshot = row.input;
        snapshot.digital = Read<std::uint64_t>(input, 792);
        snapshot.transferVersion = Read<std::uint32_t>(input, 0);
        snapshot.source = Read<std::uint32_t>(input, 4);
        snapshot.destination = Read<std::uint32_t>(input, 8);
        snapshot.sourceSlot[0] = Read<std::uint32_t>(input, 12);
        snapshot.sourceSlot[1] = Read<std::uint32_t>(input, 16);
        snapshot.destinationSlot[0] = Read<std::uint32_t>(input, 20);
        snapshot.destinationSlot[1] = Read<std::uint32_t>(input, 24);
        snapshot.type = Read<std::uint8_t>(input, 28);
        snapshot.flags = Read<std::uint8_t>(input, 29);
        snapshot.amount = Read<std::uint16_t>(input, 30);
        // Observed UI entity field used by client_interaction_toggle_new.
        // Its precise prompt/transaction semantics remain to be verified.
        if (record[0xd0 / 8]) snapshot.hudHost = Read<std::uint32_t>(reinterpret_cast<void*>(record[0xd0 / 8]), 0x16308);
        if (lastInputPlayer != player || !SameInput(lastInput, snapshot)) {
            lastInput = snapshot;
            lastInputPlayer = player;
            Enqueue(row);
        }
        return;
    }
}

void ReadClientActor(void* context, std::uintptr_t* record) {
    if (!isForeground()) return;
    const auto local = localPlayerObservation.load();
    const auto player = static_cast<std::uint32_t>(local);
    if (!player || static_cast<DWORD>(GetTickCount() - static_cast<DWORD>(local >> 32)) > 1000) {
        lastActorPlayer = 0;
        return;
    }
    unsigned scanned = 0;
    for (; scanned < 4096 && iterNext(context, record, 0x30); ++scanned) {
        ++actorRows;
        std::uint32_t idBuffer[2]{};
        const auto* id = currentId(context, idBuffer);
        if (!id || *id != player || !record[2]) continue;
        const auto snapshot = ReadActor(reinterpret_cast<void*>(record[2]));
        const bool changed = lastActorPlayer != player || !SameActor(lastActor, snapshot);
        // Log interaction targets and their clearing, not unrelated locomotion.
        const bool relevant = snapshot.host || (lastActorPlayer == player && lastActor.host);
        lastActor = snapshot;
        lastActorPlayer = player;
        if (changed && relevant) {
            Row row{};
            row.kind = Kind::ClientActor;
            row.player = player;
            row.actor = snapshot;
            Enqueue(row);
        }
        return;
    }
    if (scanned == 4096) ++truncatedActorScans;
}

// The native prediction helper at 0x270760 reads SelectedTargets (record +
// 0x148) and InteractionOffer through the declared lookup at record + 0x250.
// This observes the offered action before a button press, not only the later
// ActorInput request. The smaller ClientInteractionOffer does not carry it.
void ReadSelectedOffer(void* context, const std::uintptr_t* record, std::uint32_t player) {
    const auto now = GetTickCount64();
    if (now < nextSelectedTick) return;
    nextSelectedTick = now + 100;
    if (!record[0x148 / 8] || !selectTarget || !findEntry || !readEntry) return;
    const auto* targets = reinterpret_cast<const void*>(record[0x148 / 8]);
    std::uint64_t object[2]{};
    selectTarget(object, targets, 2); // Reflected TargetType::Interaction.
    SelectedOfferSnapshot snapshot{};
    snapshot.host = object[0];
    snapshot.hostType = static_cast<std::uint8_t>(object[1]);
    if (snapshot.host) snapshot.sequence = Read<std::uint32_t>(targets, 0x880);
    snapshot.lookupPresent = record[0x250 / 8] != 0;
    if (snapshot.host && snapshot.hostType == 0 && snapshot.host <= UINT32_MAX) {
        if (const auto* entry = findEntry(context, static_cast<std::uint32_t>(snapshot.host))) {
            snapshot.entryFound = 1;
            // The native query compares this GUID with the player tombstone
            // template at 0x2580aa. A template identifies kind, not ownership.
            if (readTemplate) readTemplate(context, snapshot.templateGuid, entry);
            std::uintptr_t component[2]{};
            if (snapshot.lookupPresent)
                readEntry(component, entry, reinterpret_cast<const void*>(record[0x250 / 8]));
            snapshot.componentPresent = component[0] != 0;
            snapshot.componentStride = static_cast<std::uint32_t>(component[1]);
            // Native read helper returns the component pointer and byte stride.
            if (component[0] && component[1] >= 128) {
                const auto* offer = reinterpret_cast<const void*>(component[0]);
                snapshot.found = 1;
                snapshot.actionGuid[0] = Read<std::uint64_t>(offer, 0);
                snapshot.actionGuid[1] = Read<std::uint64_t>(offer, 8);
                snapshot.offer = Read<std::uint32_t>(offer, 112);
                snapshot.verb = Read<std::uint32_t>(offer, 116);
                snapshot.offered = Read<std::uint8_t>(offer, 121);
            }
        }
    }
    const bool changed = lastSelectedPlayer != player || !SameSelectedOffer(lastSelected, snapshot);
    const bool relevant = snapshot.host || (lastSelectedPlayer == player && lastSelected.host);
    lastSelected = snapshot;
    lastSelectedPlayer = player;
    if (changed && relevant) {
        Row row{};
        row.kind = Kind::SelectedOffer;
        row.player = player;
        row.selected = snapshot;
        Enqueue(row);
    }
}

void ReadActionTargetRows(void* context, std::uintptr_t* record) {
    if (!isForeground()) { lastActionPlayer = lastSelectedPlayer = 0; return; }
    const auto local = localPlayerObservation.load();
    const auto player = static_cast<std::uint32_t>(local);
    if (!player || static_cast<DWORD>(GetTickCount() - static_cast<DWORD>(local >> 32)) > 1000) {
        lastActionPlayer = lastSelectedPlayer = 0;
        return;
    }
    unsigned scanned = 0;
    for (; scanned < 4096 && iterNext(context, record, 0x358); ++scanned) {
        ++actionRows;
        std::uint32_t idBuffer[2]{};
        const auto* id = currentId(context, idBuffer);
        // Native player_control_action_sequence uses record + 0xa8 as
        // ActorInput (0x28459a), resetting its host/offer at 0x2845cd.
        if (!id || *id != player) continue;
        ReadSelectedOffer(context, record, player);
        if (!record[0xa8 / 8]) return;
        const auto snapshot = ReadActionTarget(reinterpret_cast<void*>(record[0xa8 / 8]));
        const bool changed = lastActionPlayer != player || !SameActionTarget(lastAction, snapshot);
        const bool relevant = snapshot.host || (lastActionPlayer == player && lastAction.host);
        lastAction = snapshot;
        lastActionPlayer = player;
        if (changed && relevant) {
            Row row{};
            row.kind = Kind::ActionTarget;
            row.player = player;
            row.action = snapshot;
            Enqueue(row);
        }
        return;
    }
    if (scanned == 4096) ++truncatedActionScans;
}

void Capture(unsigned system, void* context) {
    __try {
        std::uintptr_t copy[2]{}, record[0x358 / 8]{};
        std::memcpy(copy, context, sizeof(copy));
        iterInit(copy, record, Systems[system].recordSize);
        switch (system) {
        case 0: ReadClientOffers(copy, record); break;
        case 1: ReadEvents(reinterpret_cast<void*>(record[3]), 24, Kind::Attempt); break;
        case 2: ReadClientInput(copy, record); break;
        case 3:
            ReadEvents(reinterpret_cast<void*>(record[0x50 / 8]), 16, Kind::Death);
            ReadEvents(reinterpret_cast<void*>(record[0x58 / 8]), 232, Kind::Hit);
            break;
        case 4: ReadEvents(reinterpret_cast<void*>(record[3]), 80, Kind::Transformed); break;
        case 5: ReadClientActor(copy, record); break;
        case 6: ReadActionTargetRows(copy, record); break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { faulted = true; }
}

// Capture AFTER native target selection, before its caller consumes ActorInput.
// The original function owns all writes and runs exactly once. Copy its iterator
// before the call so observation does not start at the consumed end position.
bool CopyContext(void* context, std::uintptr_t* copy) {
    __try { std::memcpy(copy, context, sizeof(std::uintptr_t) * 2); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { faulted = true; return false; }
}
void __fastcall ActionTargetHook(void* context) {
    ++calls[6];
    std::uintptr_t copy[2]{};
    bool capture = observing && !faulted && context;
    if (capture) capture = CopyContext(context, copy);
    originals[6](context);
    if (capture && observing && !faulted && !actionBusy.test_and_set()) {
        try { Capture(6, copy); }
        catch (...) { faulted = true; }
        actionBusy.clear();
    }
}

template<unsigned Index> void __fastcall Hook(void* context) {
    ++calls[Index];
    if (observing && !faulted && context) {
        auto* guard = Index == 0 ? &offersBusy : (Index == 2 ? &inputBusy : (Index == 5 ? &actorBusy : nullptr));
        if (!guard || !guard->test_and_set()) {
            try { Capture(Index, context); }
            catch (...) { faulted = true; }
            if (guard) guard->clear();
        }
    }
    // Always preserve the original callback, including when diagnostics fail.
    originals[Index](context);
}

bool SupportedExecutable() {
    wchar_t path[32768]{};
    const auto pathLength = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (!pathLength || pathLength >= std::size(path)) return false;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool valid = CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
    std::array<unsigned char, 65536> buffer{};
    while (valid) {
        DWORD count = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
            valid = false; break;
        }
        if (!count) break;
        valid = CryptHashData(hash, buffer.data(), count, 0) != FALSE;
    }
    std::array<unsigned char, 32> digest{};
    DWORD digestSize = static_cast<DWORD>(digest.size());
    valid = valid && CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digestSize, 0) && digestSize == digest.size();
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    constexpr char hex[] = "0123456789abcdef";
    for (std::size_t index = 0; valid && index < digest.size(); ++index)
        valid = ExecutableSha256[index * 2] == hex[digest[index] >> 4] &&
            ExecutableSha256[index * 2 + 1] == hex[digest[index] & 15];
    return valid;
}

bool ValidateImage() {
    __try {
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 4096) return false;
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp != TimeDateStamp || nt->OptionalHeader.SizeOfImage != ImageSize) return false;
        for (const auto& system : Systems) {
            if (std::memcmp(image + system.callback, system.prefix.data(), system.prefix.size())) return false;
            const auto* descriptor = reinterpret_cast<const std::uintptr_t*>(image + system.descriptor);
            if (descriptor[2] != reinterpret_cast<std::uintptr_t>(image + system.callback) ||
                descriptor[1] != std::strlen(system.name) ||
                std::memcmp(reinterpret_cast<void*>(descriptor[0]), system.name, descriptor[1])) return false;
        }
        for (const auto& helper : SelectionHelpers)
            if (std::memcmp(image + helper.rva, helper.prefix.data(), helper.prefix.size())) return false;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool Prepare() {
    if (prepared) return true;
    image = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!SupportedExecutable() || !ValidateImage()) return false;
    // Keep code and trampolines alive until process exit if the loader unloads the mod.
    // This prevents an in-flight callback returning into a freed DLL.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Prepare), &pinned)) return false;
    if (MH_Initialize() != MH_OK) return false;
    const std::array<Callback, Systems.size()> hooks{Hook<0>, Hook<1>, Hook<2>, Hook<3>, Hook<4>, Hook<5>, ActionTargetHook};
    unsigned created = 0;
    for (; created < Systems.size(); ++created) {
        if (MH_CreateHook(image + Systems[created].callback, reinterpret_cast<void*>(hooks[created]),
            reinterpret_cast<void**>(&originals[created])) != MH_OK) break;
    }
    if (created != Systems.size()) {
        for (unsigned index = 0; index < created; ++index) MH_RemoveHook(image + Systems[index].callback);
        MH_Uninitialize();
        return false;
    }
    iterInit = reinterpret_cast<Init>(image + 0x8da7c0);
    iterNext = reinterpret_cast<Next>(image + 0x8d5ca0);
    currentId = reinterpret_cast<CurrentId>(image + 0x8d3490);
    getWorld = reinterpret_cast<World>(image + 0x8d3630);
    lookupRead = reinterpret_cast<Lookup>(image + 0x8c72a0);
    selectTarget = reinterpret_cast<SelectTarget>(image + SelectionHelpers[0].rva);
    findEntry = reinterpret_cast<FindEntry>(image + SelectionHelpers[1].rva);
    readEntry = reinterpret_cast<ReadEntry>(image + SelectionHelpers[2].rva);
    readTemplate = reinterpret_cast<ReadTemplate>(image + SelectionHelpers[3].rva);
    prepared = true;
    return true;
}

void Stop() {
    observing = false;
    localPlayerObservation = 0;
    if (prepared)
        for (const auto& system : Systems) MH_DisableHook(image + system.callback);
}
}

class InteractionProbe final : public Mod {
    ULONGLONG nextSummary = 0;
public:
    ModMetaData GetMetaData() override {
        return {"InteractionProbe", "Observes manual harvest and loot transactions; no automatic actions.",
            "0.6.0", "Aerox912", "0.1.1", true, false};
    }
    void Load(ModContext* context) override {
        loaded = context && context->game.isClient && Probe::Prepare();
        if (context) context->Log(loaded ? "[InteractionProbe] verified client 1076226; diagnostic 0.6.0 prepared" :
            "[InteractionProbe] unsupported or conflicting layout; no hooks enabled");
    }
    void Activate(ModContext* context) override {
        // The current loader sets Mod::loaded after Load even if preparation failed.
        if (!loaded || !Probe::prepared || active) return;
        for (const auto& system : Probe::Systems) {
            if (MH_EnableHook(Probe::image + system.callback) != MH_OK) {
                Probe::Stop();
                context->Log("[InteractionProbe] hook activation failed; diagnostics stopped");
                return;
            }
        }
        Probe::observing = true;
        active = true;
        context->Log("[InteractionProbe] observing only; harvest and loot remain manual");
    }
    void Deactivate(ModContext*) override { Probe::Stop(); active = false; }
    void Unload(ModContext* context) override { Deactivate(context); loaded = false; }
    ~InteractionProbe() override { Probe::Stop(); }
    void Update(ModContext* context) override {
        if (!active || !context) return;
        if (Probe::faulted) {
            Deactivate(context);
            context->Log("[InteractionProbe] unexpected data layout; diagnostics disabled, native actions retained");
            return;
        }
        std::array<Probe::Row, 512> rows{};
        std::size_t count = 0;
        {
            std::lock_guard<std::mutex> lock(Probe::queueMutex);
            count = Probe::queued;
            std::copy_n(Probe::queue.begin(), count, rows.begin());
            Probe::queued = 0;
        }
        constexpr const char* names[]{"candidate", "attempt", "loot", "hit", "death", "transformed", "clientOffer", "clientInput", "clientActor"};
        char text[768]{};
        for (std::size_t index = 0; index < count; ++index) {
            const auto& row = rows[index];
              if (row.kind == Probe::Kind::SelectedOffer) {
                  const auto& selected = row.selected;
                  sprintf_s(text, "[InteractionProbe] selectedOffer player=%u host=0x%llx hostType=%u sequence=%u found=%u offer=%u verb=%u offered=%u actionGuid=%016llx:%016llx lookup=%u entry=%u component=%u stride=%u templateGuid=%016llx:%016llx",
                      row.player, static_cast<unsigned long long>(selected.host), selected.hostType, selected.sequence,
                      selected.found, selected.offer, selected.verb, selected.offered,
                      static_cast<unsigned long long>(selected.actionGuid[0]), static_cast<unsigned long long>(selected.actionGuid[1]),
                      selected.lookupPresent, selected.entryFound, selected.componentPresent, selected.componentStride,
                      static_cast<unsigned long long>(selected.templateGuid[0]), static_cast<unsigned long long>(selected.templateGuid[1]));
                  context->Log(text);
                  continue;
              }
            if (row.kind == Probe::Kind::ActionTarget) {
                const auto& action = row.action;
                sprintf_s(text, "[InteractionProbe] actionTarget player=%u host=0x%llx hostType=%u offer=%u sequence=%u triggerTime=%llu trigger=%u callIndex=%u",
                    row.player, static_cast<unsigned long long>(action.host), action.hostType, action.offer,
                    action.sequence, static_cast<unsigned long long>(action.triggerTime), action.trigger, action.callIndex);
                context->Log(text);
                continue;
            }
            if (row.kind == Probe::Kind::ClientActor) {
                const auto& actor = row.actor;
                sprintf_s(text, "[InteractionProbe] clientActor player=%u host=0x%llx hostType=%u sequence=%u sequenceActor=%u triggerTime=%llu callIndex=%u trigger=%u state=%u",
                    row.player, static_cast<unsigned long long>(actor.host), actor.hostType, actor.sequence,
                    actor.sequenceActor, static_cast<unsigned long long>(actor.triggerTime), actor.callIndex, actor.trigger, actor.state);
                context->Log(text);
                continue;
            }
            if (row.kind == Probe::Kind::ClientInput) {
                const auto& input = row.input;
                sprintf_s(text, "[InteractionProbe] clientInput player=%u digital=0x%llx hudHost=%u transferVersion=%u source=%u destination=%u sourceSlot=%u:%u destinationSlot=%u:%u type=%u flags=%u amount=%u",
                    row.player, static_cast<unsigned long long>(input.digital), input.hudHost, input.transferVersion,
                    input.source, input.destination, input.sourceSlot[0], input.sourceSlot[1],
                    input.destinationSlot[0], input.destinationSlot[1], input.type, input.flags, input.amount);
                context->Log(text);
                continue;
            }
            if (row.kind == Probe::Kind::ClientOffer) {
                sprintf_s(text, "[InteractionProbe] clientOffer player=%u target=%u lastAcceptionId=%u offered=%u verb=%u distance=%.3f",
                    row.player, row.target, row.offer, row.flags, row.verb, row.distance);
                context->Log(text);
                continue;
            }
            sprintf_s(text, "[InteractionProbe] %s player=%u target=%u offer=%u flags=%u verb=%u distance=%.3f healthChange=%d",
                names[static_cast<unsigned>(row.kind)], row.player, row.target, row.offer, row.flags, row.verb,
                row.distance, row.healthChange);
            context->Log(text);
        }
        const auto now = GetTickCount64();
        if (now >= nextSummary) {
            nextSummary = now + 10000;
            sprintf_s(text, "[InteractionProbe] calls uiOffers=%llu attempt=%llu clientInput=%llu experience=%llu transform=%llu dropped=%llu offerScans=%llu offerRows=%llu truncatedScans=%llu inputRows=%llu clientActor=%llu actorRows=%llu truncatedActorScans=%llu",
                Probe::calls[0].load(), Probe::calls[1].load(), Probe::calls[2].load(),
                Probe::calls[3].load(), Probe::calls[4].load(), Probe::dropped.load(),
                Probe::offerScans.load(), Probe::offerRows.load(), Probe::truncatedScans.load(), Probe::inputRows.load(),
                Probe::calls[5].load(), Probe::actorRows.load(), Probe::truncatedActorScans.load());
            context->Log(text);
            sprintf_s(text, "[InteractionProbe] actionCalls=%llu actionRows=%llu truncatedActionScans=%llu",
                Probe::calls[6].load(), Probe::actionRows.load(), Probe::truncatedActionScans.load());
            context->Log(text);
        }
    }
};

extern "C" __declspec(dllexport) Mod* CreateModInstance() { return new InteractionProbe(); }
