#include "CorpsePresentationRegistry.h"
#include "../SequenceNumber.h"

#include <cmath>
#include <limits>

namespace Game::Client {
CorpsePresentationApplyResult CorpsePresentationRegistry::Apply(
    const CorpsePresentationState& state) {
    CorpsePresentationApplyResult result{};
    if (!IsSane(state)) return result;
    result.entity = state.entity;

    const uint64_t key = Key(state.entity);
    auto current = mEntries.find(key);
    if (!state.active) {
        if (current == mEntries.end() ||
            !SameSource(current->second.state, state)) return result;
        result.update = CorpsePresentationUpdate::Retired;
        result.actorHandle = current->second.actorHandle;
        mEntitiesBySource.erase(Source(current->second.state));
        mEntitiesByActorHandle.erase(current->second.actorHandle);
        mEntries.erase(current);
        return result;
    }

    if (current != mEntries.end()) {
        if (!SameSource(current->second.state, state)) return result;
        current->second.state = state;
        result.update = CorpsePresentationUpdate::Updated;
        result.actorHandle = current->second.actorHandle;
        return result;
    }

    int16_t reusedHandle = 0;
    for (auto existing = mEntries.begin(); existing != mEntries.end(); ++existing) {
        if (existing->second.state.entity.index != state.entity.index) continue;
        if (!Sequence::IsNewer(state.entity.generation,
                               existing->second.state.entity.generation)) {
            return {};
        }
        result.previousEntity = existing->second.state.entity;
        reusedHandle = existing->second.actorHandle;
        mEntitiesBySource.erase(Source(existing->second.state));
        mEntitiesByActorHandle.erase(reusedHandle);
        mEntries.erase(existing);
        break;
    }

    const SourceKey source = Source(state);
    const auto sourceOwner = mEntitiesBySource.find(source);
    if (sourceOwner != mEntitiesBySource.end() && sourceOwner->second != key) {
        const auto previous = mEntries.find(sourceOwner->second);
        if (previous != mEntries.end()) {
            result.previousEntity = previous->second.state.entity;
            reusedHandle = previous->second.actorHandle;
            mEntitiesByActorHandle.erase(reusedHandle);
            mEntries.erase(previous);
        }
        mEntitiesBySource.erase(sourceOwner);
    }

    const int16_t actorHandle = reusedHandle != 0 ? reusedHandle : AllocateActorHandle();
    if (actorHandle == 0) return {};
    mEntries.emplace(key, Entry{ state, actorHandle });
    mEntitiesBySource[source] = key;
    mEntitiesByActorHandle[actorHandle] = key;
    result.update = result.previousEntity ? CorpsePresentationUpdate::Replaced
                                          : CorpsePresentationUpdate::Established;
    result.actorHandle = actorHandle;
    return result;
}

const CorpsePresentationState* CorpsePresentationRegistry::Find(
    Simulation::EntityId entity) const {
    const auto found = mEntries.find(Key(entity));
    return found == mEntries.end() ? nullptr : &found->second.state;
}

const CorpsePresentationState* CorpsePresentationRegistry::FindForSource(
    Simulation::EntityId sourcePlayerEntity,
    uint32_t sourceLifeEpoch) const {
    const auto source = mEntitiesBySource.find(
        Source(sourcePlayerEntity, sourceLifeEpoch));
    if (source == mEntitiesBySource.end()) return nullptr;
    const auto corpse = mEntries.find(source->second);
    return corpse == mEntries.end() ? nullptr : &corpse->second.state;
}

bool CorpsePresentationRegistry::OwnsSource(
    Simulation::EntityId sourcePlayerEntity,
    uint32_t sourceLifeEpoch) const {
    return FindForSource(sourcePlayerEntity, sourceLifeEpoch) != nullptr;
}

const CorpsePresentationState* CorpsePresentationRegistry::FindByActorHandle(
    int16_t actorHandle) const {
    const auto entity = mEntitiesByActorHandle.find(actorHandle);
    if (entity == mEntitiesByActorHandle.end()) return nullptr;
    const auto found = mEntries.find(entity->second);
    return found == mEntries.end() ? nullptr : &found->second.state;
}

std::optional<int16_t> CorpsePresentationRegistry::ActorHandleFor(
    Simulation::EntityId entity) const {
    const auto found = mEntries.find(Key(entity));
    return found == mEntries.end() ? std::nullopt
                                   : std::optional<int16_t>(found->second.actorHandle);
}

std::optional<Simulation::EntityId> CorpsePresentationRegistry::EntityForActorHandle(
    int16_t actorHandle) const {
    const CorpsePresentationState* state = FindByActorHandle(actorHandle);
    return state ? std::optional<Simulation::EntityId>(state->entity) : std::nullopt;
}

void CorpsePresentationRegistry::Reset() {
    mEntries.clear();
    mEntitiesBySource.clear();
    mEntitiesByActorHandle.clear();
    mNextActorHandle = -1000;
}

uint64_t CorpsePresentationRegistry::Key(Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

CorpsePresentationRegistry::SourceKey CorpsePresentationRegistry::Source(
    const CorpsePresentationState& state) {
    return Source(state.sourcePlayerEntity, state.sourceLifeEpoch);
}

CorpsePresentationRegistry::SourceKey CorpsePresentationRegistry::Source(
    Simulation::EntityId entity, uint32_t lifeEpoch) {
    return { entity.index, entity.generation, lifeEpoch };
}

bool CorpsePresentationRegistry::SameSource(
    const CorpsePresentationState& first,
    const CorpsePresentationState& second) {
    return first.sourcePlayerId == second.sourcePlayerId &&
           first.sourcePlayerEntity == second.sourcePlayerEntity &&
           first.sourceLifeEpoch == second.sourceLifeEpoch;
}

bool CorpsePresentationRegistry::IsSane(const CorpsePresentationState& state) {
    const auto saneCoordinate = [](float value) {
        return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
    };
    return state.entity.Valid() && state.sourcePlayerId >= 0 &&
           state.sourcePlayerEntity.Valid() && state.sourceLifeEpoch != 0 &&
           state.sceneId >= 0 &&
           state.sceneId < 4096 && state.roomId >= -1 && state.roomId < 256 &&
           saneCoordinate(state.x) && saneCoordinate(state.y) && saneCoordinate(state.z) &&
           state.selectedWeapon <= 4;
}

int16_t CorpsePresentationRegistry::AllocateActorHandle() {
    constexpr int32_t handleCount = -static_cast<int32_t>(std::numeric_limits<int16_t>::min()) - 999;
    for (int32_t attempt = 0; attempt < handleCount; ++attempt) {
        const int16_t candidate = static_cast<int16_t>(mNextActorHandle);
        --mNextActorHandle;
        if (mNextActorHandle < std::numeric_limits<int16_t>::min()) mNextActorHandle = -1000;
        if (mEntitiesByActorHandle.count(candidate) == 0) return candidate;
    }
    return 0;
}

} // namespace Game::Client
