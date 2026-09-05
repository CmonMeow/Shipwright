#pragma once

#include "../simulation/EntityId.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace Game::Client {

struct RemotePlayerPresentationState {
    Simulation::EntityId entity{};
    int32_t playerId = -1;
    uint32_t lifeEpoch = 0;
    int32_t sceneId = -1;
    bool active = false;
};

enum class RemotePlayerPresentationUpdate : uint8_t {
    Ignored,
    Established,
    Updated,
    Replaced,
    Retired,
};

struct RemotePlayerPresentationApplyResult {
    RemotePlayerPresentationUpdate update = RemotePlayerPresentationUpdate::Ignored;
    Simulation::EntityId entity{};
    std::optional<Simulation::EntityId> previousEntity;
    int16_t actorHandle = 0;

    bool Applied() const { return update != RemotePlayerPresentationUpdate::Ignored; }
};

// Maps exact server player lifetimes to private native Actor handles. Player
// IDs are ownership metadata and never pass through Actor::params, removing
// the legacy int16 player-count/identity coupling from the render boundary.
class RemotePlayerPresentationRegistry final {
  public:
    RemotePlayerPresentationApplyResult Apply(
        const RemotePlayerPresentationState& state);

    const RemotePlayerPresentationState* Find(Simulation::EntityId entity) const;
    const RemotePlayerPresentationState* FindPlayer(int32_t playerId) const;
    const RemotePlayerPresentationState* FindByActorHandle(int16_t actorHandle) const;
    std::optional<int16_t> ActorHandleFor(Simulation::EntityId entity) const;
    std::optional<Simulation::EntityId> EntityForActorHandle(int16_t actorHandle) const;

    void Reset();
    size_t Size() const { return mEntries.size(); }

  private:
    struct Entry {
        RemotePlayerPresentationState state{};
        int16_t actorHandle = 0;
    };

    static uint64_t Key(Simulation::EntityId entity);
    static bool IsSane(const RemotePlayerPresentationState& state);
    int16_t AllocateActorHandle();

    std::map<uint64_t, Entry> mEntries;
    std::map<int32_t, uint64_t> mEntitiesByPlayer;
    std::map<int16_t, uint64_t> mEntitiesByActorHandle;
    int32_t mNextActorHandle = 1;
};

} // namespace Game::Client
