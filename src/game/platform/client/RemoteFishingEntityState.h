#pragma once

#include "../simulation/FishingSimulation.h"

#include <cstddef>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>

namespace Game::Client {

struct RemoteFishIdentity {
    int32_t sceneId = -1;
    uint32_t spawnKey = 0;

    constexpr auto operator<=>(const RemoteFishIdentity&) const = default;
};

struct RemoteFishEntity {
    int32_t ownerPlayerId = -1;
    Simulation::EntityId entity{};
    RemoteFishIdentity identity{};
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float length = 0.0f;
    Simulation::FishSpecies species = Simulation::FishSpecies::HylianBass;
    bool active = false;
};

struct RemoteLureEntity {
    int32_t ownerPlayerId = -1;
    Simulation::EntityId entity{};
    int32_t sceneId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint8_t phase = 0;
    uint8_t lureType = 0;
    bool active = false;
};

enum class RemoteFishingEntityUpdate : uint8_t {
    Ignored,
    Established,
    Updated,
    Replaced,
    Retired,
};

// Stores already-admitted semantic fish/lure state consumed by native rendering.
// ClientReplicationInbox is the sole wire ordering and exact-lifetime gate;
// this presentation store contains no transport sequence policy or delayed queue.
class RemoteFishingEntityState final {
  public:
    RemoteFishingEntityUpdate ApplyFish(const RemoteFishEntity& state);
    RemoteFishingEntityUpdate ApplyLure(const RemoteLureEntity& state);

    const RemoteFishEntity* FindFish(Simulation::EntityId entity) const;
    const RemoteLureEntity* FindLure(Simulation::EntityId entity) const;
    const RemoteFishEntity* FishForOwner(int32_t ownerPlayerId) const;
    const RemoteLureEntity* LureForOwner(int32_t ownerPlayerId) const;
    std::optional<Simulation::EntityId> FishEntityForOwner(
        int32_t ownerPlayerId) const;
    std::optional<Simulation::EntityId> LureEntityForOwner(
        int32_t ownerPlayerId) const;
    std::optional<Simulation::EntityId> EntityForFish(
        const RemoteFishIdentity& identity) const;
    std::optional<int32_t> OwnerForFish(const RemoteFishIdentity& identity) const;

    void RemoveOwner(int32_t ownerPlayerId);
    void Reset();

    size_t FishCount() const { return mFish.size(); }
    size_t LureCount() const { return mLures.size(); }

  private:
    using EntityKey = uint64_t;

    static EntityKey Key(Simulation::EntityId entity);
    static bool IsSane(const RemoteFishEntity& state);
    static bool IsSane(const RemoteLureEntity& state);

    std::map<EntityKey, RemoteFishEntity> mFish;
    std::map<EntityKey, RemoteLureEntity> mLures;
    std::map<int32_t, EntityKey> mFishByOwner;
    std::map<int32_t, EntityKey> mLureByOwner;
    std::map<RemoteFishIdentity, EntityKey> mFishByIdentity;
};

} // namespace Game::Client
