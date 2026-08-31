#pragma once

#include "../simulation/EntityId.h"

#include <cstdint>
#include <optional>

namespace Game::Client {

struct LocalSceneEntryRequest {
    uint32_t sequence = 0;
    int32_t sceneId = -1;
};

struct LocalSceneAuthority {
    int32_t playerId = -1;
    Simulation::EntityId entity{};
    uint32_t requestSequence = 0;
    uint32_t lifeEpoch = 0;
    int32_t sceneId = -1;
    struct Position {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    } position{};
    int16_t heading = 0;
    bool accepted = false;
};

enum class LocalSceneAuthorityKind : uint8_t {
    Ignored,
    Bootstrap,
    Accepted,
    Rejected,
};

struct LocalSceneAuthorityResult {
    LocalSceneAuthorityKind kind = LocalSceneAuthorityKind::Ignored;
    LocalSceneAuthority state{};

    bool Applied() const { return kind != LocalSceneAuthorityKind::Ignored; }
};

// Owns the client's ordered scene-request stream and the exact server reply
// allowed to authorize local simulation. Reliable transport ordering is not
// used as a substitute for request correlation: stale replies are rejected.
class LocalSceneAdmission final {
  public:
    explicit LocalSceneAdmission(uint32_t nextSequence = 1);

    std::optional<LocalSceneEntryRequest> Prepare(int32_t desiredSceneId);
    bool ResolveTransport(uint32_t sequence, bool sent);
    LocalSceneAuthorityResult Apply(const LocalSceneAuthority& authority);

    bool IsAuthorized(int32_t sceneId) const;
    std::optional<int32_t> AuthorizedScene() const;
    std::optional<Simulation::EntityId> AuthorizedEntity() const;
    std::optional<uint32_t> LifeEpoch() const;
    std::optional<int32_t> PendingScene() const;
    std::optional<int32_t> PendingPlacementScene() const;
    std::optional<LocalSceneAuthority> TakePlacement(int32_t loadedSceneId);
    bool ObserveLifeEpoch(uint32_t lifeEpoch);
    void Reset();

  private:
    static bool IsSane(const LocalSceneAuthority& authority);
    uint32_t TakeSequence();

    std::optional<LocalSceneEntryRequest> mOffered;
    std::optional<LocalSceneEntryRequest> mPending;
    std::optional<Simulation::EntityId> mAuthorizedEntity;
    std::optional<LocalSceneAuthority> mPendingPlacement;
    int32_t mDesiredSceneId = -1;
    int32_t mAuthorizedSceneId = -1;
    int32_t mRejectedSceneId = -1;
    uint32_t mNextSequence = 1;
    uint32_t mLifeEpoch = 0;
    bool mBootstrapApplied = false;
};

} // namespace Game::Client
