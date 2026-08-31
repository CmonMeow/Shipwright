#include "RemoteProjectilePresentationRegistry.h"
#include "../SequenceNumber.h"

#include <limits>

namespace Game::Client {
RemoteProjectilePresentationApplyResult RemoteProjectilePresentationRegistry::Apply(
    const RemoteProjectilePresentationState& state) {
    RemoteProjectilePresentationApplyResult result{};
    if (!IsSane(state)) return result;
    result.entity = state.entity;
    const uint64_t key = Key(state.entity);
    const auto logical = mEntitiesByLogicalId.find(state.logicalId);

    if (!state.active) {
        if (logical == mEntitiesByLogicalId.end() || logical->second != key) return result;
        const auto current = mEntries.find(key);
        if (current == mEntries.end()) return result;
        result.update = RemoteProjectilePresentationUpdate::Retired;
        result.actorHandle = current->second.actorHandle;
        mEntitiesByActorHandle.erase(current->second.actorHandle);
        mEntitiesByLogicalId.erase(logical);
        mEntries.erase(current);
        return result;
    }

    const auto exact = mEntries.find(key);
    if (exact != mEntries.end()) {
        if (exact->second.state.logicalId != state.logicalId) return result;
        exact->second.state = state;
        result.update = RemoteProjectilePresentationUpdate::Updated;
        result.actorHandle = exact->second.actorHandle;
        return result;
    }

    int16_t reusedHandle = 0;
    if (logical != mEntitiesByLogicalId.end()) {
        const auto current = mEntries.find(logical->second);
        if (current == mEntries.end()) return result;
        if (current->second.state.entity.index == state.entity.index &&
            !Sequence::IsNewer(state.entity.generation,
                               current->second.state.entity.generation)) {
            return result;
        }
        result.previousEntity = current->second.state.entity;
        reusedHandle = current->second.actorHandle;
        mEntitiesByActorHandle.erase(reusedHandle);
        mEntries.erase(current);
        mEntitiesByLogicalId.erase(logical);
    }

    const int16_t actorHandle = reusedHandle != 0 ? reusedHandle : AllocateActorHandle();
    if (actorHandle == 0) return {};
    mEntries.emplace(key, Entry{ state, actorHandle });
    mEntitiesByLogicalId[state.logicalId] = key;
    mEntitiesByActorHandle[actorHandle] = key;
    result.update = result.previousEntity ? RemoteProjectilePresentationUpdate::Replaced
                                          : RemoteProjectilePresentationUpdate::Established;
    result.actorHandle = actorHandle;
    return result;
}

const RemoteProjectilePresentationState* RemoteProjectilePresentationRegistry::Find(
    Simulation::EntityId entity) const {
    const auto found = mEntries.find(Key(entity));
    return found == mEntries.end() ? nullptr : &found->second.state;
}

const RemoteProjectilePresentationState* RemoteProjectilePresentationRegistry::FindLogical(
    Replication::ProjectileLogicalId logicalId) const {
    const auto entity = mEntitiesByLogicalId.find(logicalId);
    if (entity == mEntitiesByLogicalId.end()) return nullptr;
    const auto found = mEntries.find(entity->second);
    return found == mEntries.end() ? nullptr : &found->second.state;
}

const RemoteProjectilePresentationState* RemoteProjectilePresentationRegistry::FindByActorHandle(
    int16_t actorHandle) const {
    const auto entity = mEntitiesByActorHandle.find(actorHandle);
    if (entity == mEntitiesByActorHandle.end()) return nullptr;
    const auto found = mEntries.find(entity->second);
    return found == mEntries.end() ? nullptr : &found->second.state;
}

std::optional<int16_t> RemoteProjectilePresentationRegistry::ActorHandleFor(
    Simulation::EntityId entity) const {
    const auto found = mEntries.find(Key(entity));
    return found == mEntries.end() ? std::nullopt
                                   : std::optional<int16_t>(found->second.actorHandle);
}

std::optional<Simulation::EntityId> RemoteProjectilePresentationRegistry::EntityForActorHandle(
    int16_t actorHandle) const {
    const auto* state = FindByActorHandle(actorHandle);
    return state ? std::optional<Simulation::EntityId>(state->entity) : std::nullopt;
}

std::vector<Simulation::EntityId> RemoteProjectilePresentationRegistry::RetireOwner(
    int32_t ownerPlayerId) {
    std::vector<Simulation::EntityId> retired;
    if (ownerPlayerId < 0) return retired;
    for (auto entry = mEntries.begin(); entry != mEntries.end();) {
        if (entry->second.state.logicalId.ownerPlayerId != ownerPlayerId) {
            ++entry;
            continue;
        }
        retired.push_back(entry->second.state.entity);
        mEntitiesByLogicalId.erase(entry->second.state.logicalId);
        mEntitiesByActorHandle.erase(entry->second.actorHandle);
        entry = mEntries.erase(entry);
    }
    return retired;
}

void RemoteProjectilePresentationRegistry::Reset() {
    mEntries.clear();
    mEntitiesByLogicalId.clear();
    mEntitiesByActorHandle.clear();
    mNextActorHandle = 1;
}

uint64_t RemoteProjectilePresentationRegistry::Key(Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

bool RemoteProjectilePresentationRegistry::IsSane(
    const RemoteProjectilePresentationState& state) {
    return state.entity.Valid() && state.logicalId.Valid() && state.sceneId >= 0 &&
           state.sceneId < 4096;
}

int16_t RemoteProjectilePresentationRegistry::AllocateActorHandle() {
    constexpr int32_t handleCount = std::numeric_limits<int16_t>::max();
    for (int32_t attempt = 0; attempt < handleCount; ++attempt) {
        const int16_t candidate = static_cast<int16_t>(mNextActorHandle++);
        if (mNextActorHandle > std::numeric_limits<int16_t>::max()) mNextActorHandle = 1;
        if (mEntitiesByActorHandle.count(candidate) == 0) return candidate;
    }
    return 0;
}

} // namespace Game::Client
