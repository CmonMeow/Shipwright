#pragma once

#include "ObjectiveSimulation.h"
#include "PlayerSimulation.h"
#include "StructureSimulation.h"

#include <cstdint>
#include <optional>

namespace Game::Simulation {

enum class StructureActionKind : uint8_t {
    Build,
    Repair,
};

enum class StructureActionResult : uint8_t {
    Accepted,
    Invalid,
    Replayed,
    RateLimited,
    StaleLife,
    PlayerUnavailable,
    OutOfRange,
    ObjectiveNotOwned,
    SupplyUnavailable,
    StructureRejected,
};

struct StructureActionCommand {
    int32_t playerId = -1;
    uint32_t sequence = 0;
    uint32_t lifeEpoch = 0;
    int32_t structureKey = -1;
    StructureActionKind kind = StructureActionKind::Build;
};

struct StructureActionDecision {
    StructureActionResult result = StructureActionResult::Invalid;
    StructureActionCommand command{};
    std::optional<StructureSnapshot> structure;

    bool Accepted() const { return result == StructureActionResult::Accepted; }
};

// Stateless deterministic server policy for an admitted, cooldown-approved
// fortification action. ServerIntentAdmission owns all per-player lifecycle state.
class StructureActionAuthority final {
  public:
    StructureActionDecision Execute(const StructureActionCommand& command,
                                    const PlayerSimulation& players,
                                    const ObjectiveSimulation& objectives,
                                    StructureSimulation& structures,
                                    bool hasRequiredSupply);

    static constexpr uint32_t ContributionAmount() { return kContributionAmount; }

  private:
    static constexpr float kInteractionRadius = 350.0f;
    static constexpr uint32_t kContributionAmount = 25;
};

} // namespace Game::Simulation
