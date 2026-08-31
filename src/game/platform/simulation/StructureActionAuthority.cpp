#include "StructureActionAuthority.h"

namespace Game::Simulation {

StructureActionDecision StructureActionAuthority::Execute(
    const StructureActionCommand& command, const PlayerSimulation& players,
    const ObjectiveSimulation& objectives,
    StructureSimulation& structures, bool hasRequiredSupply) {
    StructureActionDecision decision{};
    decision.command = command;
    if (command.playerId < 0 || command.sequence == 0 || command.lifeEpoch == 0 ||
        command.structureKey < 0 ||
        (command.kind != StructureActionKind::Build && command.kind != StructureActionKind::Repair)) {
        return decision;
    }

    const auto player = players.SnapshotForPlayer(command.playerId);
    if (!player || player->lifeEpoch != command.lifeEpoch) {
        decision.result = StructureActionResult::StaleLife;
        return decision;
    }

    const auto structure = structures.SnapshotForStructure(command.structureKey);
    if (!structure || !CanPerformGroundedAction(*player) ||
        player->team == TeamId::Neutral ||
        player->sceneId != structure->sceneId) {
        decision.result = StructureActionResult::PlayerUnavailable;
        return decision;
    }

    const float dx = player->position.x - structure->position.x;
    const float dy = player->position.y - structure->position.y;
    const float dz = player->position.z - structure->position.z;
    if (dx * dx + dy * dy + dz * dz > kInteractionRadius * kInteractionRadius) {
        decision.result = StructureActionResult::OutOfRange;
        return decision;
    }

    const auto objective = objectives.SnapshotForObjective(structure->objectiveKey);
    if (!objective || objective->sceneId != structure->sceneId || objective->owner != player->team) {
        decision.result = StructureActionResult::ObjectiveNotOwned;
        return decision;
    }
    if (!hasRequiredSupply) {
        decision.result = StructureActionResult::SupplyUnavailable;
        return decision;
    }

    const bool accepted = command.kind == StructureActionKind::Build
                              ? structures.ContributeBuild(command.structureKey, player->team,
                                                           objective->owner, kContributionAmount)
                              : structures.Repair(command.structureKey, player->team,
                                                  kContributionAmount);
    if (!accepted) {
        decision.result = StructureActionResult::StructureRejected;
        return decision;
    }

    decision.result = StructureActionResult::Accepted;
    decision.structure = structures.SnapshotForStructure(command.structureKey);
    return decision;
}

} // namespace Game::Simulation
