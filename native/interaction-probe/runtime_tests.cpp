// Compile the exact callback implementation with synthetic game functions.
// No hooks or game process are needed for these boundary tests.
#include "probe.cpp"
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
std::array<unsigned char, 0x30> eventList{};
std::array<unsigned char, 24 * 40> events{};
unsigned checks = 0, originalCalls = 0;
void* originalContext = nullptr;

void Require(bool value, const char* label) {
    ++checks;
    if (!value) { std::cerr << label << '\n'; std::exit(1); }
}
template<class T> void Put(void* data, std::size_t offset, T value) {
    std::memcpy(static_cast<unsigned char*>(data) + offset, &value, sizeof(value));
}
void __fastcall FakeInit(void* context, void* record, std::uint32_t size) {
    Require(size == 0x20, "exact attempt record size");
    static_cast<std::uintptr_t*>(context)[1] = 99; // Must affect only the private iterator.
    static_cast<std::uintptr_t*>(record)[3] = reinterpret_cast<std::uintptr_t>(eventList.data());
}
void __fastcall Original(void* context) { ++originalCalls; originalContext = context; }
void Reset(std::uint32_t count) {
    Probe::queued = 0;
    Probe::dropped = 0;
    Probe::faulted = false;
    Probe::observing = true;
    originalCalls = 0;
    Put(eventList.data(), 0x18, events.data());
    Put(eventList.data(), 0x28, count);
}
std::array<unsigned char, 808> clientInput{};
std::array<unsigned char, 208> networkActor{};
std::array<unsigned char, 440> actorInput{};
std::array<unsigned char, 2184> selectedTargets{};
std::array<unsigned char, 128> fullOffer{};
bool selectionPresent = false, entryPresent = true, offerPresent = true;
bool lookupPresent = true;
std::array<std::uint64_t, 2> templateGuid{0x46c0817c3a0df3efull, 0x5f1fbe866bbd5f97ull};
std::uint64_t selectedHost = 0;
std::uint8_t selectedType = 0;
std::uintptr_t offerStride = 128;
unsigned selectionCalls = 0, entryCalls = 0;
void* __fastcall FakeSelectedTarget(void* output, const void* targets, std::uint8_t type) {
    Require(targets == selectedTargets.data() && type == 2, "native selector reads only the Interaction target");
    ++selectionCalls;
    Put(output, 0, selectedHost);
    Put(output, 8, std::uint64_t{selectedType});
    return output;
}
void* __fastcall FakeEntry(void* context, std::uint32_t id) {
    Require(static_cast<std::uintptr_t*>(context)[0] == 11 && id == selectedHost, "selected entity resolved through the current private iterator");
    ++entryCalls;
    return entryPresent ? selectedTargets.data() : nullptr;
}
void* __fastcall FakeReadEntry(void* output, const void* entry, const void* descriptor) {
    Require(entry == selectedTargets.data() && descriptor == reinterpret_cast<void*>(0x222), "only the system's declared InteractionOffer lookup is used");
    Put(output, 0, offerPresent ? reinterpret_cast<std::uintptr_t>(fullOffer.data()) : std::uintptr_t{0});
    Put(output, 8, offerStride);
    return output;
}
void* __fastcall FakeTemplate(void* context, void* output, const void* entry) {
    Require(static_cast<std::uintptr_t*>(context)[0] == 11 && entry == selectedTargets.data(),
        "template identity read from the same resolved entity");
    std::memcpy(output, templateGuid.data(), sizeof(templateGuid));
    return output;
}
std::array<unsigned char, 0x1630c> hud{};
std::uint32_t localPlayer[2]{0, 1}, currentEntity = 0;
std::int64_t playerPosition[3]{}, targetPosition[3]{};
std::uint32_t clientOffer[3]{42, 0, 1};
unsigned entityCount = 0, nextCalls = 0, entityIndex = 0;
bool foreground = true;
bool actorOnlyRemote = false, actorMissingData = false;
bool actionMissingData = false, actionChangesTarget = false;
void __fastcall ActionOriginal(void* context) {
    Original(context);
    if (actionChangesTarget) Put(actorInput.data(), 0x90, std::uint32_t{99});
}
bool Focus() { return foreground; }
void* __fastcall FakeWorld(void* descriptor) { return descriptor; }
void* __fastcall FakeLookup(void* output, void*, void*, std::uint32_t player) {
    Require(player == 1, "client offer lookup uses the local player");
    static_cast<std::uintptr_t*>(output)[0] = reinterpret_cast<std::uintptr_t>(playerPosition);
    return output;
}
std::uint32_t* __fastcall FakeId(void*, void*) { return &currentEntity; }
void __fastcall ClientInit(void* context, void* output, std::uint32_t size) {
    Require(size == 0x48 || size == 0x158 || size == 0x30 || size == 0x358, "client observer uses verified record size");
    auto* record = static_cast<std::uintptr_t*>(output);
    static_cast<std::uintptr_t*>(context)[1] = 99;
    entityIndex = nextCalls = 0;
    if (size == 0x48) {
        record[6] = reinterpret_cast<std::uintptr_t>(localPlayer);
        record[8] = 11; // Read-only lookup descriptor, never dereferenced by fixture.
    } else if (size == 0x158) {
        record[0xe0 / 8] = reinterpret_cast<std::uintptr_t>(localPlayer);
        record[0xd0 / 8] = reinterpret_cast<std::uintptr_t>(hud.data());
    }
}
bool __fastcall ClientNext(void*, void* output, std::uint32_t size) {
    ++nextCalls;
    if (entityIndex >= entityCount) return false;
    auto* record = static_cast<std::uintptr_t*>(output);
    if (size == 0x48) {
        currentEntity = 2000 + entityIndex;
        record[1] = reinterpret_cast<std::uintptr_t>(clientOffer);
        record[2] = reinterpret_cast<std::uintptr_t>(targetPosition);
    } else if (size == 0x158) {
        currentEntity = entityIndex == 0 ? 2u : 1u; // Remote player precedes local player.
        record[3] = reinterpret_cast<std::uintptr_t>(clientInput.data());
    } else if (size == 0x30) {
        currentEntity = actorOnlyRemote || entityIndex == 0 ? 2u : 1u;
        record[2] = actorMissingData ? 0 : reinterpret_cast<std::uintptr_t>(networkActor.data());
    } else {
        currentEntity = actorOnlyRemote || entityIndex == 0 ? 2u : 1u;
        record[0xa8 / 8] = actionMissingData ? 0 : reinterpret_cast<std::uintptr_t>(actorInput.data());
        record[0x148 / 8] = selectionPresent ? reinterpret_cast<std::uintptr_t>(selectedTargets.data()) : 0;
        record[0x250 / 8] = lookupPresent ? 0x222 : 0;
    }
    ++entityIndex;
    return true;
}
}

int main() {
    Probe::iterInit = FakeInit;
    Probe::originals[1] = Original;
    for (unsigned index = 0; index < 40; ++index) {
        Put(events.data(), index * 24 + 8, std::uint32_t{7});
        Put(events.data(), index * 24 + 12, std::uint32_t{1000 + index});
        Put(events.data(), index * 24 + 16, std::uint32_t{18});
    }
    std::uintptr_t context[2]{11, 22};
    Reset(1);
    Probe::Hook<1>(context);
    Require(originalCalls == 1 && originalContext == context, "original called once with original context");
    Require(context[0] == 11 && context[1] == 22, "game iterator remains unchanged");
    Require(!Probe::faulted && Probe::queued == 1 && Probe::queue[0].player == 7 && Probe::queue[0].target == 1000,
        "manual attempt captured");

    Reset(40);
    Probe::Hook<1>(context);
    Require(Probe::queued == 32 && Probe::dropped == 8 && originalCalls == 1, "per-callback observation work bounded");
    Reset(65537);
    Probe::Hook<1>(context);
    Require(Probe::faulted && Probe::queued == 0 && originalCalls == 1, "implausible count stops observation and preserves game callback");

    Reset(1);
    Put(eventList.data(), 0x18, std::uintptr_t{0});
    Probe::Hook<1>(context);
    Require(Probe::faulted && Probe::queued == 0 && originalCalls == 1, "missing event data fails closed");
    Reset(1);
    Put(eventList.data(), 0x18, std::uintptr_t{1});
    Probe::Hook<1>(context);
    Require(Probe::faulted && originalCalls == 1, "access fault does not suppress original callback");

    Reset(1);
    Probe::observing = false;
    Probe::Hook<1>(context);
    Require(Probe::queued == 0 && originalCalls == 1, "disabled observer leaves native callback alone");
    Reset(1);
    Probe::Hook<1>(nullptr);
    Require(!Probe::faulted && originalCalls == 1 && !originalContext, "null context passed through unchanged");

    Reset(0);
    for (unsigned index = 0; index < 513; ++index) Probe::Enqueue({});
    Require(Probe::queued == 512 && Probe::dropped == 1, "output queue bounded");
    Reset(0);
    {
        std::lock_guard<std::mutex> lock(Probe::queueMutex);
        std::thread competing([] { Probe::Enqueue({}); });
        competing.join();
    }
    Require(Probe::queued == 0 && Probe::dropped == 1, "game callback never waits on logging lock");

    Probe::iterInit = ClientInit;
    Probe::iterNext = ClientNext;
    Probe::currentId = FakeId;
    Probe::getWorld = FakeWorld;
    Probe::lookupRead = FakeLookup;
    Probe::isForeground = Focus;
    Probe::originals[0] = Probe::originals[2] = Original;
    entityCount = 2;
    Put(clientInput.data(), 0, std::uint32_t{9});
    Put(clientInput.data(), 4, std::uint32_t{3456});
    Put(clientInput.data(), 8, std::uint32_t{1});
    Put(clientInput.data(), 28, std::uint8_t{2});
    Put(clientInput.data(), 30, std::uint16_t{7});
    Put(clientInput.data(), 792, std::uint64_t{0x100000000ull});
    Put(hud.data(), 0x16308, std::uint32_t{3456});
    const auto untouchedInput = clientInput;
    const auto untouchedHud = hud;
    Reset(0);
    Probe::lastInputPlayer = 0;
    Probe::Hook<2>(context);
    Require(!Probe::faulted && originalCalls == 1 && Probe::queued == 1, "native client input observed without replacing camera control");
    Require(Probe::queue[0].player == 1 && Probe::queue[0].input.source == 3456 &&
        Probe::queue[0].input.destination == 1 && Probe::queue[0].input.hudHost == 3456 &&
        Probe::queue[0].input.digital == 0x100000000ull && Probe::queue[0].input.amount == 7 &&
        Probe::queue[0].input.type == 2 && Probe::queue[0].input.transferVersion == 9,
        "only local player transfer fields decoded");
    Require(clientInput == untouchedInput && hud == untouchedHud && context[1] == 22,
        "input HUD and game iterator remain unchanged");
    Reset(0);
    Probe::Hook<2>(context);
    Require(Probe::queued == 0 && originalCalls == 1, "unchanged input is not logged again");
    Put(clientInput.data(), 0, std::uint32_t{10});
    Probe::Hook<2>(context);
    Require(Probe::queued == 1, "new version of otherwise identical transfer is observed");
    Reset(0);
    foreground = false;
    Put(clientInput.data(), 792, std::uint64_t{2});
    Probe::Hook<2>(context);
    Require(Probe::queued == 0 && originalCalls == 1 && nextCalls == 0, "unfocused client input observer pauses");
    foreground = true;
    Reset(0);
    localPlayer[1] = 0;
    Probe::Hook<2>(context);
    Require(Probe::queued == 0 && nextCalls == 0, "no active local player leaves input untouched");
    localPlayer[1] = 1;
    Reset(0);
    Probe::inputBusy.test_and_set();
    Probe::Hook<2>(context);
    Probe::inputBusy.clear();
    Require(Probe::queued == 0 && originalCalls == 1, "concurrent input capture skips without delaying native callback");

    Reset(0);
    entityCount = 1;
    targetPosition[0] = std::int64_t{2} << 32;
    Probe::nextOfferTick = 0;
    Probe::Hook<0>(context);
    Require(!Probe::faulted && Probe::queued == 1 && originalCalls == 1 &&
        Probe::queue[0].kind == Probe::Kind::ClientOffer && Probe::queue[0].target == 2000 &&
        Probe::queue[0].distance == 2.0 && Probe::queue[0].flags == 1 && Probe::queue[0].verb == 42,
        "nearby client prompt observed using native read lookup");
    Require(clientOffer[0] == 42 && clientOffer[1] == 0 && clientOffer[2] == 1 && context[1] == 22,
        "client interaction offer and iterator remain unchanged");
    Reset(0);
    Probe::nextOfferTick = 0;
    Probe::Hook<0>(context);
    Require(Probe::queued == 0, "unchanged nearby prompt is suppressed");
    clientOffer[1] = 1;
    Probe::nextOfferTick = 0;
    Probe::Hook<0>(context);
    Require(Probe::queued == 1 && Probe::queue[0].offer == 1, "acceptance changes observed without calling an interaction");
    Reset(0);
    targetPosition[0] = std::int64_t{4} << 32;
    Probe::nextOfferTick = 0;
    Probe::Hook<0>(context);
    Require(Probe::queued == 0, "offers beyond diagnostic radius excluded");
    Reset(0);
    entityCount = 4097;
    Probe::truncatedScans = 0;
    Probe::nextOfferTick = 0;
    Probe::Hook<0>(context);
    Require(nextCalls == 4096 && Probe::truncatedScans == 1 && originalCalls == 1,
        "large offer lists are bounded and truncation is reported");
    Reset(0);
    entityCount = 100;
    targetPosition[0] = 0;
    Probe::lastOffers = {};
    Probe::nextOfferTick = 0;
    Probe::Hook<0>(context);
    Require(Probe::queued == 32, "nearby offer logging is bounded per scan");
    Reset(0);
    foreground = false;
    Probe::nextOfferTick = 0;
    Probe::Hook<0>(context);
    Require(nextCalls == 0 && Probe::queued == 0 && originalCalls == 1, "unfocused offer scan pauses");

    foreground = true;
    entityCount = 2;
    Probe::originals[5] = Original;
    Probe::lastActorPlayer = 0;
    Probe::localPlayerObservation = (static_cast<std::uint64_t>(GetTickCount()) << 32) | 1u;
    Put(networkActor.data(), 0, std::uint32_t{81});
    Put(networkActor.data(), 8, std::uint64_t{123456});
    Put(networkActor.data(), 16, std::uint32_t{1});
    Put(networkActor.data(), 20, std::uint8_t{4});
    Put(networkActor.data(), 56, std::uint64_t{0xfedcba9876543210ull});
    Put(networkActor.data(), 64, std::uint8_t{3});
    Put(networkActor.data(), 204, std::uint16_t{63});
    Put(networkActor.data(), 206, std::uint8_t{1});
    const auto untouchedActor = networkActor;
    Reset(0);
    Probe::Hook<5>(context);
    const auto& actor = Probe::queue[0].actor;
    Require(!Probe::faulted && originalCalls == 1 && originalContext == context && Probe::queued == 1 &&
        Probe::queue[0].kind == Probe::Kind::ClientActor && Probe::queue[0].player == 1,
        "only local actor captured and original replication callback preserved");
    Require(actor.host == 0xfedcba9876543210ull && actor.hostType == 3 && actor.sequence == 81 &&
        actor.sequenceActor == 1 && actor.triggerTime == 123456 && actor.callIndex == 4 && actor.trigger == 63 && actor.state == 1,
        "full world-object target and reflected action fields retained without assuming entity type");
    Require(networkActor == untouchedActor && context[1] == 22, "actor and game iterator remain unchanged");
    Reset(0);
    Probe::Hook<5>(context);
    Require(Probe::queued == 0 && originalCalls == 1, "unchanged actor target suppressed");
    Put(networkActor.data(), 8, std::uint64_t{123457});
    Probe::Hook<5>(context);
    Require(Probe::queued == 1, "new trigger time for same target is observable");
    Reset(0);
    Put(networkActor.data(), 56, std::uint64_t{0});
    Probe::Hook<5>(context);
    Require(Probe::queued == 1 && Probe::queue[0].actor.host == 0, "target clearing is observed once");
    Reset(0);
    Put(networkActor.data(), 0, std::uint32_t{82});
    Probe::Hook<5>(context);
    Require(Probe::queued == 0, "unrelated action without interaction target is not logged");

    networkActor = untouchedActor;
    Reset(0);
    foreground = false;
    Probe::Hook<5>(context);
    Require(Probe::queued == 0 && nextCalls == 0 && originalCalls == 1, "actor observation pauses when unfocused");
    foreground = true;
    Reset(0);
    Probe::localPlayerObservation = 0;
    Probe::Hook<5>(context);
    Require(Probe::queued == 0 && nextCalls == 0 && originalCalls == 1, "actor observation requires active local identity");
    Reset(0);
    Probe::localPlayerObservation = (static_cast<std::uint64_t>(static_cast<DWORD>(GetTickCount() - 2000)) << 32) | 1u;
    Probe::Hook<5>(context);
    Require(Probe::queued == 0 && nextCalls == 0 && Probe::lastActorPlayer == 0, "stale local identity is not reused across sessions");
    Probe::localPlayerObservation = (static_cast<std::uint64_t>(GetTickCount()) << 32) | 1u;
    Reset(0);
    actorOnlyRemote = true;
    entityCount = 4097;
    Probe::truncatedActorScans = 0;
    Probe::Hook<5>(context);
    Require(Probe::queued == 0 && nextCalls == 4096 && Probe::truncatedActorScans == 1 && originalCalls == 1,
        "actor scan remains bounded when local player is absent");
    actorOnlyRemote = false;
    entityCount = 2;
    actorMissingData = true;
    Reset(0);
    Probe::Hook<5>(context);
    Require(Probe::queued == 0 && !Probe::faulted && originalCalls == 1, "missing actor component skipped");
    actorMissingData = false;
    Reset(0);
    Probe::actorBusy.test_and_set();
    Probe::Hook<5>(context);
    Probe::actorBusy.clear();
    Require(Probe::queued == 0 && originalCalls == 1, "concurrent actor observation never blocks native replication");

    Reset(0);
    localPlayer[1] = 0;
    Probe::Hook<2>(context);
    Require(Probe::localPlayerObservation == 0, "leaving world clears actor identity");
    localPlayer[1] = 1;

    Probe::originals[6] = ActionOriginal;
    Probe::localPlayerObservation = (static_cast<std::uint64_t>(GetTickCount()) << 32) | 1u;
    Put(actorInput.data(), 8, std::uint32_t{987});
    Put(actorInput.data(), 16, std::uint64_t{654});
    Put(actorInput.data(), 0x20, std::uint16_t{6});
    Put(actorInput.data(), 0x80, std::uint64_t{0xfedcba9876543210ull});
    Put(actorInput.data(), 0x88, std::uint8_t{1});
    Put(actorInput.data(), 0x90, std::uint32_t{12});
    Put(actorInput.data(), 0x178, std::uint8_t{4});
    actionChangesTarget = true;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::faulted && originalCalls == 1 && originalContext == context && Probe::queued == 1,
        "target observer preserves native call and selects local player");
    const auto action = Probe::queue[0].action;
    Require(action.host == 0xfedcba9876543210ull && action.hostType == 1 && action.offer == 99 &&
        action.sequence == 987 && action.triggerTime == 654 && action.trigger == 6 && action.callIndex == 4,
        "target captures post-native offer ID and full-width object identity");
    actionChangesTarget = false;
    const auto untouchedAction = actorInput;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::queued && originalCalls == 1 && actorInput == untouchedAction && context[1] == 22,
        "unchanged target is suppressed without changing native data or iterator");
    Put(actorInput.data(), 0x90, std::uint32_t{100});
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && Probe::queue[0].action.offer == 100,
        "new offer on same object is not deduplicated away");
    Reset(0);
    Put(actorInput.data(), 0x80, std::uint64_t{0});
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && Probe::queue[0].action.host == 0, "target clearing is logged");
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::queued, "idle selection is not repeatedly logged");
    actorInput = untouchedAction;
    foreground = false;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::queued && nextCalls == 0 && originalCalls == 1, "target observer pauses without focus");
    foreground = true;
    Probe::localPlayerObservation = 0;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::queued && nextCalls == 0 && originalCalls == 1, "target requires local player observation");
    Probe::localPlayerObservation = (static_cast<std::uint64_t>(static_cast<DWORD>(GetTickCount() - 2000)) << 32) | 1u;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::queued && nextCalls == 0, "target rejects stale identity");
    Probe::localPlayerObservation = (static_cast<std::uint64_t>(GetTickCount()) << 32) | 1u;
    actionMissingData = true;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::faulted && !Probe::queued && originalCalls == 1, "missing target input skipped");
    actionMissingData = false;
    actorOnlyRemote = true;
    entityCount = 4097;
    Probe::truncatedActionScans = 0;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(nextCalls == 4096 && Probe::truncatedActionScans == 1 && !Probe::queued && originalCalls == 1,
        "target scan is bounded when local player is absent");
    actorOnlyRemote = false;
    entityCount = 2;
    Reset(0);
    Probe::actionBusy.test_and_set();
    Probe::ActionTargetHook(context);
    Probe::actionBusy.clear();
    Require(!Probe::queued && originalCalls == 1, "busy target observer still calls native selection");
    Reset(0);
    Probe::observing = false;
    Probe::ActionTargetHook(context);
    Require(!Probe::queued && originalCalls == 1, "disabled target observer passes through");
    Reset(0);
    Probe::ActionTargetHook(nullptr);
    Require(!Probe::queued && !Probe::faulted && originalCalls == 1 && !originalContext,
        "null target context passed to original unchanged");

    Probe::selectTarget = FakeSelectedTarget;
    Probe::findEntry = FakeEntry;
    Probe::readEntry = FakeReadEntry;
    Probe::readTemplate = FakeTemplate;
    selectionPresent = true;
    selectedHost = 0x8fb4065a;
    selectedType = 0;
    Put(selectedTargets.data(), 0x880, std::uint32_t{428699201});
    Put(fullOffer.data(), 0, std::uint64_t{0x123456789abcdef0ull});
    Put(fullOffer.data(), 8, std::uint64_t{0xfedcba9876543210ull});
    Put(fullOffer.data(), 112, std::uint32_t{73});
    Put(fullOffer.data(), 116, std::uint32_t{81});
    Put(fullOffer.data(), 121, std::uint8_t{1});
    const auto untouchedOffer = fullOffer;
    const auto untouchedSelected = selectedTargets;
    const auto untouchedTemplate = templateGuid;
    Probe::localPlayerObservation = (static_cast<std::uint64_t>(GetTickCount()) << 32) | 1u;
    Probe::lastAction = Probe::ReadActionTarget(actorInput.data());
    Probe::lastActionPlayer = 1;
    Probe::nextSelectedTick = 0;
    Reset(0);
    Probe::ActionTargetHook(context);
    Require(!Probe::faulted && originalCalls == 1 && originalContext == context && Probe::queued == 1 && Probe::queue[0].kind == Probe::Kind::SelectedOffer,
        "selected offer captured without suppressing the native callback");
    const auto selected = Probe::queue[0].selected;
    Require(selected.host == selectedHost && selected.hostType == 0 && selected.sequence == 428699201 && selected.found && selected.offered && selected.offer == 73 && selected.verb == 81 &&
        selected.actionGuid[0] == 0x123456789abcdef0ull && selected.actionGuid[1] == 0xfedcba9876543210ull,
        "full offered action retained separately from an action request or ownership claim");
    Require(selected.lookupPresent && selected.entryFound && selected.componentPresent && selected.componentStride == 128 &&
        selected.templateGuid[0] == templateGuid[0] && selected.templateGuid[1] == templateGuid[1],
        "template identity and every lookup stage retained separately from eligibility");
    Require(fullOffer == untouchedOffer && selectedTargets == untouchedSelected && templateGuid == untouchedTemplate && actorInput == untouchedAction && context[1] == 22,
        "selected offer observation leaves all native data and iterator unchanged");
    Reset(0);
    const auto selectionsBefore = selectionCalls;
    Probe::nextSelectedTick = GetTickCount64() + 60000;
    Probe::ActionTargetHook(context);
    Require(!Probe::queued && selectionCalls == selectionsBefore, "offer lookup work is throttled");
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(!Probe::queued, "unchanged selected offer is deduplicated");
    Put(fullOffer.data(), 121, std::uint8_t{0});
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && !Probe::queue[0].selected.offered, "withdrawn offer remains distinguishable from an eligible offer");
    Reset(0);
    selectedHost = 0;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && !Probe::queue[0].selected.host && !Probe::queue[0].selected.found, "cleared interaction target is logged once");
    Reset(0);
    selectedHost = 0xfedcba9876543210ull; selectedType = 1;
    const auto entriesBefore = entryCalls;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && Probe::queue[0].selected.host == selectedHost && !Probe::queue[0].selected.found && entryCalls == entriesBefore,
        "SnappingBox IDs are retained at full width and never passed to entity lookup");
    Reset(0);
    selectedType = 0;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && entryCalls == entriesBefore, "out-of-range Entity ID is not truncated for lookup");
    Reset(0);
    selectedHost = 32; entryPresent = false;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(!Probe::faulted && Probe::queued == 1 && !Probe::queue[0].selected.found, "missing entity leaves offer unverified");
    Require(!Probe::queue[0].selected.entryFound && !Probe::queue[0].selected.templateGuid[0], "missing entity cannot inherit the previous template");
    Reset(0);
    selectedHost = 33; entryPresent = true; offerPresent = false;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(!Probe::faulted && Probe::queued == 1 && !Probe::queue[0].selected.found, "missing offer component leaves target unverified");
    Require(Probe::queue[0].selected.entryFound && !Probe::queue[0].selected.componentPresent,
        "absent component is distinguishable from absent entity");
    Reset(0);
    selectedHost = 34; offerPresent = true; offerStride = 12;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(!Probe::faulted && Probe::queued == 1 && !Probe::queue[0].selected.found, "short component is never interpreted as a full interaction offer");
    Require(Probe::queue[0].selected.componentPresent && Probe::queue[0].selected.componentStride == 12,
        "rejected component reports its actual stride without reading its fields");
    Reset(0);
    lookupPresent = false;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && !Probe::queue[0].selected.lookupPresent && Probe::queue[0].selected.entryFound &&
        !Probe::queue[0].selected.componentPresent && Probe::queue[0].selected.templateGuid[0] == templateGuid[0],
        "missing lookup does not hide template identity or reuse old offer fields");
    Reset(0);
    lookupPresent = true;
    templateGuid[0] ^= 1;
    Probe::nextSelectedTick = 0;
    Probe::ActionTargetHook(context);
    Require(Probe::queued == 1 && Probe::queue[0].selected.templateGuid[0] == templateGuid[0],
        "changed template is copied even when the entity ID is unchanged");
    Reset(0);
    foreground = false;
    Probe::nextSelectedTick = 0;
    const auto unfocusedCalls = selectionCalls;
    Probe::ActionTargetHook(context);
    Require(!Probe::queued && selectionCalls == unfocusedCalls && originalCalls == 1, "full offer lookup pauses outside foreground gameplay");
    foreground = true;

    ModContext modContext{};
    modContext.game.isClient = true;
    std::string message;
    modContext.Log = [&message](const char* text) { message = text; };
    InteractionProbe mod;
    mod.Load(&modContext);
    Require(!mod.loaded && !Probe::prepared && message.find("unsupported") != std::string::npos,
        "unsupported executable installs no hooks");
    mod.loaded = true; // Match the loader's unconditional assignment after Load.
    mod.Activate(&modContext);
    Require(!mod.active, "unsupported executable cannot activate");
    std::cout << checks << " callback boundary checks passed\n";
}
