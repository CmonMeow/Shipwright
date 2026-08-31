#include "ReplicationCadence.h"

namespace Game::Replication {
namespace {

bool ConsumeInterval(uint32_t& accumulated, uint32_t added, uint32_t interval) {
    const uint64_t total = static_cast<uint64_t>(accumulated) + added;
    accumulated = static_cast<uint32_t>(total % interval);
    return total >= interval;
}

} // namespace

ReplicationCadenceDue ReplicationCadence::Advance(uint32_t worldSteps) {
    if (worldSteps == 0) return {};
    return {
        ConsumeInterval(mPlayerSteps, worldSteps, kPlayerInterval),
        ConsumeInterval(mObjectiveSteps, worldSteps, kWorldStateInterval),
        ConsumeInterval(mStructureSteps, worldSteps, kWorldStateInterval),
    };
}

void ReplicationCadence::Reset() {
    mPlayerSteps = 0;
    mObjectiveSteps = 0;
    mStructureSteps = 0;
}

} // namespace Game::Replication
