#pragma once

#include <cstdint>
#include <optional>

namespace Game::Client {

enum class LocalStructureActionKind : uint8_t {
    Build,
    Repair,
};

struct LocalStructureActionRequest {
    int32_t structureKey = -1;
    LocalStructureActionKind kind = LocalStructureActionKind::Build;
};

struct LocalStructureAction {
    uint32_t sequence = 0;
    LocalStructureActionRequest request{};
};

// Owns reliable structure-action identities before wire serialization. Each
// issue is a one-shot player decision; transport rejection never leaves an
// action queued to execute after range, ownership, or structure state changes.
class LocalStructureActionStream final {
  public:
    explicit LocalStructureActionStream(uint32_t nextSequence = 1);

    std::optional<LocalStructureAction> Issue(
        const LocalStructureActionRequest& request);
    void BeginLife();
    void Reset();

  private:
    static bool IsSane(const LocalStructureActionRequest& request);
    uint32_t TakeSequence();

    uint32_t mNextSequence = 1;
};

} // namespace Game::Client
