#include "CorpseSimulation.h"

namespace Game::Simulation {

EntityId CorpseSimulation::Create(const CorpsePose& pose) {
    if (pose.sourcePlayerId < 0 || !pose.sourcePlayerEntity.Valid() ||
        pose.sourceLifeEpoch == 0 || pose.sceneId < 0) return {};
    CorpseEntity corpse{};
    corpse.pose = pose;
    const EntityId id = mCorpses.Create(corpse);
    CorpseEntity* created = mCorpses.Get(id);
    created->id = id;
    auto& order = mSceneOrder[pose.sceneId];
    order.push_back(id);
    while (order.size() > kMaximumPerScene) {
        const EntityId oldest = order.front();
        order.pop_front();
        mCorpses.Destroy(oldest);
    }
    return id;
}

void CorpseSimulation::Reset() {
    mCorpses.Clear();
    mSceneOrder.clear();
}

std::vector<CorpseSnapshot> CorpseSimulation::Snapshots() const {
    std::vector<CorpseSnapshot> snapshots;
    snapshots.reserve(mCorpses.Size());
    mCorpses.ForEach([&](const CorpseEntity& corpse) { snapshots.push_back(BuildSnapshot(corpse)); });
    return snapshots;
}

CorpseSnapshot CorpseSimulation::BuildSnapshot(const CorpseEntity& corpse) {
    return { corpse.id, corpse.pose };
}

} // namespace Game::Simulation
