#include "CorpseSimulation.h"

namespace Game::Simulation {

EntityId CorpseSimulation::Create(const CorpsePose& pose) {
    if (pose.sourcePlayerId < 0 || !pose.sourcePlayerEntity.Valid() ||
        pose.sourceLifeEpoch == 0 || pose.sceneId < 0) return {};

    const SourceKey source = Source(pose);
    const auto existing = mCorpseBySource.find(source);
    if (existing != mCorpseBySource.end()) {
        if (mCorpses.Get(existing->second)) return existing->second;
        mCorpseBySource.erase(existing);
    }

    CorpseEntity corpse{};
    corpse.pose = pose;
    const EntityId id = mCorpses.Create(corpse);
    CorpseEntity* created = mCorpses.Get(id);
    created->id = id;
    mCorpseBySource[source] = id;
    auto& order = mSceneOrder[pose.sceneId];
    order.push_back(id);
    while (order.size() > kMaximumPerScene) {
        const EntityId oldest = order.front();
        order.pop_front();
        if (const CorpseEntity* retired = mCorpses.Get(oldest)) {
            mCorpseBySource.erase(Source(retired->pose));
        }
        mCorpses.Destroy(oldest);
    }
    return id;
}

void CorpseSimulation::Reset() {
    mCorpses.Clear();
    mSceneOrder.clear();
    mCorpseBySource.clear();
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

CorpseSimulation::SourceKey CorpseSimulation::Source(const CorpsePose& pose) {
    return { pose.sourcePlayerEntity.index,
             pose.sourcePlayerEntity.generation,
             pose.sourceLifeEpoch };
}

} // namespace Game::Simulation
