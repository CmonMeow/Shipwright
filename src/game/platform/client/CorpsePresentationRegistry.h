#pragma once

#include "../simulation/EntityId.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>

namespace Game::Client {

struct CorpsePresentationState {
    Simulation::EntityId entity{};
    int32_t sourcePlayerId = -1;
    Simulation::EntityId sourcePlayerEntity{};
    uint32_t sourceLifeEpoch = 0;
    int32_t sceneId = -1;
    int32_t roomId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int16_t rotation[3]{};
    uint8_t selectedWeapon = 0;
    bool active = false;
};

enum class CorpsePresentationUpdate : uint8_t {
    Ignored,
    Established,
    Updated,
    Replaced,
    Retired,
};

struct CorpsePresentationApplyResult {
    CorpsePresentationUpdate update = CorpsePresentationUpdate::Ignored;
    Simulation::EntityId entity{};
    std::optional<Simulation::EntityId> previousEntity;
    int16_t actorHandle = 0;

    bool Applied() const { return update != CorpsePresentationUpdate::Ignored; }
};

// Owns authoritative corpse presentation lifetimes independently from player
// identity. The negative int16 handle is private native Actor plumbing only;
// network state and registry lookup always use EntityId.
class CorpsePresentationRegistry final {
  public:
    CorpsePresentationApplyResult Apply(const CorpsePresentationState& state);

    const CorpsePresentationState* Find(Simulation::EntityId entity) const;
    const CorpsePresentationState* FindForSource(
        Simulation::EntityId sourcePlayerEntity,
        uint32_t sourceLifeEpoch) const;
    bool OwnsSource(Simulation::EntityId sourcePlayerEntity,
                    uint32_t sourceLifeEpoch) const;
    const CorpsePresentationState* FindByActorHandle(int16_t actorHandle) const;
    std::optional<int16_t> ActorHandleFor(Simulation::EntityId entity) const;
    std::optional<Simulation::EntityId> EntityForActorHandle(int16_t actorHandle) const;

    void Reset();
    size_t Size() const { return mEntries.size(); }

  private:
    struct Entry {
        CorpsePresentationState state{};
        int16_t actorHandle = 0;
    };

    static uint64_t Key(Simulation::EntityId entity);
    using SourceKey = std::tuple<uint32_t, uint32_t, uint32_t>;
    static SourceKey Source(const CorpsePresentationState& state);
    static SourceKey Source(Simulation::EntityId entity, uint32_t lifeEpoch);
    static bool SameSource(const CorpsePresentationState& first,
                           const CorpsePresentationState& second);
    static bool IsSane(const CorpsePresentationState& state);
    int16_t AllocateActorHandle();

    std::map<uint64_t, Entry> mEntries;
    std::map<SourceKey, uint64_t> mEntitiesBySource;
    std::map<int16_t, uint64_t> mEntitiesByActorHandle;
    int32_t mNextActorHandle = -1000;
};

} // namespace Game::Client
