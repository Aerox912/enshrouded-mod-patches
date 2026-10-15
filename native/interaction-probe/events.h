#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Probe {
inline constexpr double HarvestRadiusMetres = 2.0;
inline constexpr double CorpseRadiusMetres = 2.0;
inline constexpr std::uint32_t MaxEventsPerCallback = 32;

enum class Kind : unsigned { Candidate, Attempt, Loot, Hit, Death, Transformed, ClientOffer, ClientInput, ClientActor, ActionTarget, SelectedOffer };
struct SelectedOfferSnapshot {
    std::uint64_t host{}, actionGuid[2]{}, templateGuid[2]{};
    std::uint32_t sequence{}, offer{}, verb{}, componentStride{};
    std::uint8_t hostType{}, found{}, offered{}, lookupPresent{}, entryFound{}, componentPresent{};
};
inline bool SameSelectedOffer(const SelectedOfferSnapshot& a, const SelectedOfferSnapshot& b) {
    return a.host == b.host && a.hostType == b.hostType && a.sequence == b.sequence &&
        a.offer == b.offer && a.verb == b.verb && a.found == b.found && a.offered == b.offered &&
        a.actionGuid[0] == b.actionGuid[0] && a.actionGuid[1] == b.actionGuid[1] &&
        a.templateGuid[0] == b.templateGuid[0] && a.templateGuid[1] == b.templateGuid[1] &&
        a.componentStride == b.componentStride && a.lookupPresent == b.lookupPresent &&
        a.entryFound == b.entryFound && a.componentPresent == b.componentPresent;
}
struct ActionTargetSnapshot {
    std::uint64_t host{}, triggerTime{};
    std::uint32_t sequence{}, offer{};
    std::uint16_t trigger{};
    std::uint8_t hostType{}, callIndex{};
};
struct ActorSnapshot {
    std::uint64_t host{}, triggerTime{};
    std::uint32_t sequence{}, sequenceActor{};
    std::uint16_t trigger{};
    std::uint8_t hostType{}, callIndex{}, state{};
};
inline ActorSnapshot ReadActor(const void* data);
struct InputSnapshot {
    std::uint64_t digital{};
    std::uint32_t transferVersion{}, source{}, destination{}, sourceSlot[2]{}, destinationSlot[2]{};
    std::uint16_t amount{};
    std::uint8_t type{}, flags{};
    std::uint32_t hudHost{};
};
struct Row {
    Kind kind{};
    std::uint32_t player{}, target{}, offer{}, flags{}, verb{};
    std::int32_t healthChange{};
    double distance{};
    InputSnapshot input{};
    ActorSnapshot actor{};
    ActionTargetSnapshot action{};
    SelectedOfferSnapshot selected{};
};

inline bool SameActionTarget(const ActionTargetSnapshot& a, const ActionTargetSnapshot& b) {
    return a.host == b.host && a.hostType == b.hostType && a.triggerTime == b.triggerTime &&
        a.sequence == b.sequence && a.offer == b.offer && a.trigger == b.trigger && a.callIndex == b.callIndex;
}

inline bool SameActor(const ActorSnapshot& a, const ActorSnapshot& b) {
    return a.host == b.host && a.hostType == b.hostType && a.triggerTime == b.triggerTime &&
        a.sequence == b.sequence && a.sequenceActor == b.sequenceActor &&
        a.trigger == b.trigger && a.callIndex == b.callIndex && a.state == b.state;
}

inline bool SameInput(const InputSnapshot& a, const InputSnapshot& b) {
    return a.digital == b.digital && a.transferVersion == b.transferVersion &&
        a.source == b.source && a.destination == b.destination &&
        a.sourceSlot[0] == b.sourceSlot[0] && a.sourceSlot[1] == b.sourceSlot[1] &&
        a.destinationSlot[0] == b.destinationSlot[0] && a.destinationSlot[1] == b.destinationSlot[1] &&
        a.amount == b.amount && a.type == b.type && a.flags == b.flags && a.hudHost == b.hudHost;
}

template<class T> T Read(const void* data, std::size_t offset) {
    T value{};
    std::memcpy(&value, static_cast<const unsigned char*>(data) + offset, sizeof(value));
    return value;
}

// ActorInput.triggerContext starts at 8; unlike NetworkActor it retains the
// interaction offer ID. This is a request observation, not a success or owner.
inline ActionTargetSnapshot ReadActionTarget(const void* data) {
    ActionTargetSnapshot action{};
    action.sequence = Read<std::uint32_t>(data, 8);
    action.triggerTime = Read<std::uint64_t>(data, 16);
    action.trigger = Read<std::uint16_t>(data, 0x20);
    action.host = Read<std::uint64_t>(data, 0x80);
    action.hostType = Read<std::uint8_t>(data, 0x88);
    action.offer = Read<std::uint32_t>(data, 0x90);
    action.callIndex = Read<std::uint8_t>(data, 0x178);
    return action;
}

// NetworkActor and its nested SequenceRuntimeId/GameObjectId are reflected
// in client 1076226. A target is not necessarily an EntityId; keep all 64 bits.
inline ActorSnapshot ReadActor(const void* data) {
    ActorSnapshot actor{};
    actor.sequence = Read<std::uint32_t>(data, 0);
    actor.triggerTime = Read<std::uint64_t>(data, 8);
    actor.sequenceActor = Read<std::uint32_t>(data, 16);
    actor.callIndex = Read<std::uint8_t>(data, 20);
    actor.host = Read<std::uint64_t>(data, 56);
    actor.hostType = Read<std::uint8_t>(data, 64);
    actor.trigger = Read<std::uint16_t>(data, 204);
    actor.state = Read<std::uint8_t>(data, 206);
    return actor;
}

// Input is a copied event, not a retained pointer into the game.
inline bool DecodeEvent(Kind kind, const void* data, std::size_t size, Row& row) {
    if (!data) return false;
    Row parsed{};
    parsed.kind = kind;
    switch (kind) {
    case Kind::Attempt:
    case Kind::Loot:
        if (size < (kind == Kind::Loot ? 120u : 24u)) return false;
        parsed.player = Read<std::uint32_t>(data, 8);
        parsed.target = Read<std::uint32_t>(data, 12);
        parsed.offer = Read<std::uint32_t>(data, 16);
        if (kind == Kind::Loot)
            parsed.flags = Read<std::uint8_t>(data, 20) |
                (static_cast<std::uint32_t>(Read<std::uint8_t>(data, 21)) << 8);
        break;
    case Kind::Hit:
        if (size < 232) return false;
        parsed.player = Read<std::uint32_t>(data, 144); // rootSourceId, not proof of credited killer
        parsed.target = Read<std::uint32_t>(data, 160); // rootTargetId
        parsed.healthChange = Read<std::int32_t>(data, 188);
        break;
    case Kind::Death:
    case Kind::Transformed:
        if (size < (kind == Kind::Death ? 16u : 80u)) return false;
        parsed.target = Read<std::uint32_t>(data, 8);
        break;
    default: return false;
    }
    if (!parsed.target) return false;
    row = parsed;
    return true;
}

inline double Distance(const std::int64_t* first, const std::int64_t* second) {
    double squared = 0;
    for (unsigned axis = 0; axis < 3; ++axis) {
        // Convert before subtracting, avoiding signed fixed-point overflow.
        const double delta = (static_cast<double>(first[axis]) -
            static_cast<double>(second[axis])) / 4294967296.0;
        squared += delta * delta;
    }
    return std::sqrt(squared);
}
}
