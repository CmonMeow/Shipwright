#include "RemotePlayerPresentationRegistry.h"
#include "../SequenceNumber.h"

#include <limits>

namespace Game::Client {
RemotePlayerPresentationApplyResult RemotePlayerPresentationRegistry::Apply(
    const RemotePlayerPresentationState& state) {
    RemotePlayerPresentationApplyResult result{};
    if (!IsSane(state)) return result;
    result.entity = state.entity;
    const uint64_t key = Key(state.entity);
    const auto player = mEntitiesByPlayer.find(state.playerId);

    if (!state.active) {
        if (player == mEntitiesByPlayer.end() || player->second != key) return result;
        const auto current = mEntries.find(key);
        if (current == mEntries.end()) return result;
        result.update = RemotePlayerPresentationUpdate::Retired;
        result.actorHandle = current->second.actorHandle;
        mEntitiesByActorHandle.erase(current->second.actorHandle);
        mEntitiesByPlayer.erase(player);
        mEntries.erase(current);
        return result;
    }

    const auto exact = mEntries.find(key);
    if (exact != mEntries.end()) {
        if (exact->second.state.playerId != state.playerId) return result;
        exact->second.state = state;
        result.update = RemotePlayerPresentationUpdate::Updated;
        result.actorHandle = exact->second.actorHandle;
        return result;
    }

    int16_t reusedHandle = 0;
    if (player != mEntitiesByPlayer.end()) {
        const auto current = mEntries.find(player->second);
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
        mEntitiesByPlayer.erase(player);
    }

    const int16_t actorHandle = reusedHandle != 0 ? reusedHandle : AllocateActorHandle();
    if (actorHandle == 0) return {};
    mEntries.emplace(key, Entry{ state, actorHandle });
    mEntitiesByPlayer[state.playerId] = key;
    mEntitiesByActorHandle[actorHandle] = key;
    result.update = result.previousEntity ? RemotePlayerPresentationUpdate::Replaced
                                          : RemotePlayerPresentationUpdate::Established;
    result.actorHandle = actorHandle;
    return result;
}

const RemotePlayerPresentationState* RemotePlayerPresentationRegistry::Find(
    Simulation::EntityId entity) const {
    const auto found = mEntries.find(Key(entity));
    return found == mEntries.end() ? nullptr : &found->second.state;
}

const RemotePlayerPresentationState* RemotePlayerPresentationRegistry::FindPlayer(
    int32_t playerId) const {
    const auto entity = mEntitiesByPlayer.find(playerId);
    if (entity == mEntitiesByPlayer.end()) return nullptr;
    const auto found = mEntries.find(entity->second);
    return found == mEntries.end() ? nullptr : &found->second.state;
}

const RemotePlayerPresentationState* RemotePlayerPresentationRegistry::FindByActorHandle(
    int16_t actorHandle) const {
    const auto entity = mEntitiesByActorHandle.find(actorHandle);
    if (entity == mEntitiesByActorHandle.end()) return nullptr;
    const auto found = mEntries.find(entity->second);
    return found == mEntries.end() ? nullptr : &found->second.state;
}

std::optional<int16_t> RemotePlayerPresentationRegistry::ActorHandleFor(
    Simulation::EntityId entity) const {
    const auto found = mEntries.find(Key(entity));
    return found == mEntries.end() ? std::nullopt
                                   : std::optional<int16_t>(found->second.actorHandle);
}

std::optional<Simulation::EntityId> RemotePlayerPresentationRegistry::EntityForActorHandle(
    int16_t actorHandle) const {
    const auto* state = FindByActorHandle(actorHandle);
    return state ? std::optional<Simulation::EntityId>(state->entity) : std::nullopt;
}

void RemotePlayerPresentationRegistry::Reset() {
    mEntries.clear();
    mEntitiesByPlayer.clear();
    mEntitiesByActorHandle.clear();
    mNextActorHandle = 1;
}

uint64_t RemotePlayerPresentationRegistry::Key(Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

bool RemotePlayerPresentationRegistry::IsSane(
    const RemotePlayerPresentationState& state) {
    return state.entity.Valid() && state.playerId >= 0 && state.sceneId >= 0 &&
           state.sceneId < 4096;
}

int16_t RemotePlayerPresentationRegistry::AllocateActorHandle() {
    constexpr int32_t handleCount = std::numeric_limits<int16_t>::max();
    for (int32_t attempt = 0; attempt < handleCount; ++attempt) {
        const int16_t candidate = static_cast<int16_t>(mNextActorHandle++);
        if (mNextActorHandle > std::numeric_limits<int16_t>::max()) mNextActorHandle = 1;
        if (mEntitiesByActorHandle.count(candidate) == 0) return candidate;
    }
    return 0;
}

} // namespace Game::Client
