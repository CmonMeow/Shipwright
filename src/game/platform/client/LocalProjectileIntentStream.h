#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace Game::Client {

using LocalProjectilePresentationId = uint64_t;

enum class LocalProjectileIntentKind : uint8_t {
    FireArrow,
};

// Correlation data for a predicted local presentation. The native actor is
// deliberately absent: gameplay authority sees only this semantic ID.
struct LocalProjectilePresentation {
    LocalProjectilePresentationId presentationId = 0;
    int32_t sceneId = -1;
};

struct LocalProjectileIntent {
    LocalProjectileIntentKind kind = LocalProjectileIntentKind::FireArrow;
    uint32_t sequence = 0;
    int32_t sceneId = -1;
};

// Protocol-independent server decision for one submitted projectile intent.
// Life-epoch and wire-enum validation happen before this reaches gameplay.
struct LocalProjectileIntentDecision {
    uint32_t sequence = 0;
    int32_t projectileId = 0;
    LocalProjectileIntentKind kind = LocalProjectileIntentKind::FireArrow;
    bool accepted = false;
};

struct LocalProjectileAuthorityResult {
    LocalProjectilePresentationId presentationId = 0;
    LocalProjectileIntentKind kind = LocalProjectileIntentKind::FireArrow;
    bool accepted = false;
};

// Owns client command correlation IDs for explicit projectile actions.
// Binding a native presentation never creates gameplay. RequestArrowFire is
// the sole transition that emits authority intent.
class LocalProjectileIntentStream final {
  public:
    explicit LocalProjectileIntentStream(uint32_t nextSequence = 1);

    bool BindPresentation(const LocalProjectilePresentation& presentation);
    bool RequestArrowFire(LocalProjectilePresentationId presentationId,
                          int32_t sceneId);
    bool Retire(LocalProjectilePresentationId presentationId);
    std::optional<LocalProjectileIntent> NextIntent();

    // Resolves only local transport submission. A submitted intent is retained
    // until ApplyAuthorityResult receives the server's explicit decision.
    // Failed transport does not advance state and may retry next frame.
    bool Resolve(uint32_t sequence, bool sent);
    std::optional<LocalProjectileAuthorityResult> ApplyAuthorityResult(
        uint32_t sequence, int32_t projectileId,
        LocalProjectileIntentKind kind, bool accepted);
    // Retires scene-owned presentation/correlation state without rewinding
    // the per-life request sequence used by server replay protection.
    void BeginScene();
    void Reset();

    size_t TrackedCount() const { return mRecords.size(); }
    size_t AwaitingResultCount() const { return mAwaitingResults.size(); }
    bool Tracks(LocalProjectilePresentationId presentationId) const {
        return mRecords.count(presentationId) != 0;
    }
    std::optional<LocalProjectilePresentationId> PresentationForProjectile(
        int32_t projectileId) const;

  private:
    struct Record {
        LocalProjectilePresentation presentation{};
        int32_t authoritativeProjectileId = 0;
        bool arrowFireRequested = false;
        bool arrowFireSubmitted = false;
    };

    struct PendingIntent {
        LocalProjectilePresentationId presentationId = 0;
        LocalProjectileIntent intent{};
    };

    static bool IsSane(const LocalProjectilePresentation& presentation);
    uint32_t TakeSequence();
    Record& ReplaceRecord(LocalProjectilePresentationId presentationId,
                          const LocalProjectilePresentation& presentation);

    std::map<LocalProjectilePresentationId, Record> mRecords;
    std::map<uint32_t, PendingIntent> mAwaitingResults;
    std::optional<PendingIntent> mPending;
    uint32_t mNextSequence = 1;
};

} // namespace Game::Client
