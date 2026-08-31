#include "RemoteFishingEntityState.h"

#include <cmath>

namespace Game::Client {
namespace {

bool SaneCoordinate(float value) {
    return std::isfinite(value) && value > -1000000.0f && value < 1000000.0f;
}

} // namespace

RemoteFishingEntityUpdate RemoteFishingEntityState::ApplyFish(
    const RemoteFishEntity& state) {
    if (!IsSane(state)) return RemoteFishingEntityUpdate::Ignored;

    const EntityKey key = Key(state.entity);
    const auto owner = mFishByOwner.find(state.ownerPlayerId);
    if (!state.active) {
        if (owner == mFishByOwner.end() || owner->second != key) {
            return RemoteFishingEntityUpdate::Ignored;
        }
        const auto current = mFish.find(key);
        if (current == mFish.end()) return RemoteFishingEntityUpdate::Ignored;
        mFishByIdentity.erase(current->second.identity);
        mFishByOwner.erase(owner);
        mFish.erase(current);
        return RemoteFishingEntityUpdate::Retired;
    }

    const auto identity = mFishByIdentity.find(state.identity);
    if (identity != mFishByIdentity.end() && identity->second != key &&
        (owner == mFishByOwner.end() || owner->second != identity->second)) {
        return RemoteFishingEntityUpdate::Ignored;
    }

    const auto exact = mFish.find(key);
    if (exact != mFish.end()) {
        if (exact->second.ownerPlayerId != state.ownerPlayerId) {
            return RemoteFishingEntityUpdate::Ignored;
        }
        if (exact->second.identity != state.identity) {
            mFishByIdentity.erase(exact->second.identity);
        }
        exact->second = state;
        mFishByOwner[state.ownerPlayerId] = key;
        mFishByIdentity[state.identity] = key;
        return RemoteFishingEntityUpdate::Updated;
    }

    RemoteFishingEntityUpdate result = RemoteFishingEntityUpdate::Established;
    if (owner != mFishByOwner.end()) {
        const auto previous = mFish.find(owner->second);
        if (previous == mFish.end()) return RemoteFishingEntityUpdate::Ignored;
        mFishByIdentity.erase(previous->second.identity);
        mFish.erase(previous);
        result = RemoteFishingEntityUpdate::Replaced;
    }
    mFish.emplace(key, state);
    mFishByOwner[state.ownerPlayerId] = key;
    mFishByIdentity[state.identity] = key;
    return result;
}

RemoteFishingEntityUpdate RemoteFishingEntityState::ApplyLure(
    const RemoteLureEntity& state) {
    if (!IsSane(state)) return RemoteFishingEntityUpdate::Ignored;

    const EntityKey key = Key(state.entity);
    const auto owner = mLureByOwner.find(state.ownerPlayerId);
    if (!state.active) {
        if (owner == mLureByOwner.end() || owner->second != key) {
            return RemoteFishingEntityUpdate::Ignored;
        }
        if (mLures.erase(key) == 0) return RemoteFishingEntityUpdate::Ignored;
        mLureByOwner.erase(owner);
        return RemoteFishingEntityUpdate::Retired;
    }

    const auto exact = mLures.find(key);
    if (exact != mLures.end()) {
        if (exact->second.ownerPlayerId != state.ownerPlayerId) {
            return RemoteFishingEntityUpdate::Ignored;
        }
        exact->second = state;
        mLureByOwner[state.ownerPlayerId] = key;
        return RemoteFishingEntityUpdate::Updated;
    }

    RemoteFishingEntityUpdate result = RemoteFishingEntityUpdate::Established;
    if (owner != mLureByOwner.end()) {
        if (mLures.erase(owner->second) == 0) {
            return RemoteFishingEntityUpdate::Ignored;
        }
        result = RemoteFishingEntityUpdate::Replaced;
    }
    mLures.emplace(key, state);
    mLureByOwner[state.ownerPlayerId] = key;
    return result;
}

const RemoteFishEntity* RemoteFishingEntityState::FindFish(
    Simulation::EntityId entity) const {
    const auto found = mFish.find(Key(entity));
    return found == mFish.end() ? nullptr : &found->second;
}

const RemoteLureEntity* RemoteFishingEntityState::FindLure(
    Simulation::EntityId entity) const {
    const auto found = mLures.find(Key(entity));
    return found == mLures.end() ? nullptr : &found->second;
}

const RemoteFishEntity* RemoteFishingEntityState::FishForOwner(
    int32_t ownerPlayerId) const {
    const auto found = FishEntityForOwner(ownerPlayerId);
    return found ? FindFish(*found) : nullptr;
}

const RemoteLureEntity* RemoteFishingEntityState::LureForOwner(
    int32_t ownerPlayerId) const {
    const auto found = LureEntityForOwner(ownerPlayerId);
    return found ? FindLure(*found) : nullptr;
}

std::optional<Simulation::EntityId> RemoteFishingEntityState::FishEntityForOwner(
    int32_t ownerPlayerId) const {
    const auto found = mFishByOwner.find(ownerPlayerId);
    if (found == mFishByOwner.end()) return std::nullopt;
    const auto fish = mFish.find(found->second);
    return fish == mFish.end() ? std::nullopt
                               : std::optional<Simulation::EntityId>(fish->second.entity);
}

std::optional<Simulation::EntityId> RemoteFishingEntityState::LureEntityForOwner(
    int32_t ownerPlayerId) const {
    const auto found = mLureByOwner.find(ownerPlayerId);
    if (found == mLureByOwner.end()) return std::nullopt;
    const auto lure = mLures.find(found->second);
    return lure == mLures.end() ? std::nullopt
                                : std::optional<Simulation::EntityId>(lure->second.entity);
}

std::optional<Simulation::EntityId> RemoteFishingEntityState::EntityForFish(
    const RemoteFishIdentity& identity) const {
    const auto found = mFishByIdentity.find(identity);
    if (found == mFishByIdentity.end()) return std::nullopt;
    const auto fish = mFish.find(found->second);
    return fish == mFish.end() ? std::nullopt
                               : std::optional<Simulation::EntityId>(fish->second.entity);
}

std::optional<int32_t> RemoteFishingEntityState::OwnerForFish(
    const RemoteFishIdentity& identity) const {
    const auto entity = EntityForFish(identity);
    const auto* fish = entity ? FindFish(*entity) : nullptr;
    return fish ? std::optional<int32_t>(fish->ownerPlayerId) : std::nullopt;
}

void RemoteFishingEntityState::RemoveOwner(int32_t ownerPlayerId) {
    const auto fishOwner = mFishByOwner.find(ownerPlayerId);
    if (fishOwner != mFishByOwner.end()) {
        const auto fish = mFish.find(fishOwner->second);
        if (fish != mFish.end()) {
            mFishByIdentity.erase(fish->second.identity);
            mFish.erase(fish);
        }
        mFishByOwner.erase(fishOwner);
    }
    const auto lureOwner = mLureByOwner.find(ownerPlayerId);
    if (lureOwner != mLureByOwner.end()) {
        mLures.erase(lureOwner->second);
        mLureByOwner.erase(lureOwner);
    }
}

void RemoteFishingEntityState::Reset() {
    mFish.clear();
    mLures.clear();
    mFishByOwner.clear();
    mLureByOwner.clear();
    mFishByIdentity.clear();
}

RemoteFishingEntityState::EntityKey RemoteFishingEntityState::Key(
    Simulation::EntityId entity) {
    return (static_cast<uint64_t>(entity.generation) << 32) | entity.index;
}

bool RemoteFishingEntityState::IsSane(const RemoteFishEntity& state) {
    const RemoteFishIdentity& identity = state.identity;
    return state.ownerPlayerId >= 0 && state.entity.Valid() &&
           identity.sceneId >= 0 && identity.sceneId < 4096 &&
           identity.spawnKey != 0 &&
           state.species <= Simulation::FishSpecies::HylianLoach &&
           SaneCoordinate(state.x) && SaneCoordinate(state.y) && SaneCoordinate(state.z) &&
           std::isfinite(state.length) && state.length >= 0.0f && state.length <= 100.0f;
}

bool RemoteFishingEntityState::IsSane(const RemoteLureEntity& state) {
    return state.ownerPlayerId >= 0 && state.entity.Valid() &&
           state.sceneId >= 0 && state.sceneId < 4096 && state.phase <= 2 &&
           state.lureType <= 2 && SaneCoordinate(state.x) &&
           SaneCoordinate(state.y) && SaneCoordinate(state.z);
}

} // namespace Game::Client
