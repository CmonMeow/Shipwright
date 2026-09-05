#pragma once

#include "NetworkProtocol.h"
#include "platform/replication/EntityLifetimeRegistry.h"
#include "platform/replication/ProjectileLifetimeRegistry.h"
#include "platform/client/RemoteProjectileReplicaStore.h"
#include "platform/client/RemoteFishingEntityState.h"
#include "platform/client/RemotePlayerPresentationRegistry.h"
#include "platform/client/LocalProjectileIntentStream.h"
#include "platform/client/CorpsePresentationRegistry.h"
#include "platform/client/ClientWorldState.h"
#include "platform/client/LocalSceneAdmission.h"
#include "platform/replication/FishingPresentationState.h"
#include "platform/simulation/PlayerSimulation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <tuple>
#include <utility>

namespace Game::Multiplayer {

// Client-only replicated-state boundary. Reliable lifecycle messages establish
// exact generations; disposable snapshots and semantic events are admitted only
// against those lifetimes and ordered before entering presentation queues. Current
// state is coalesced by logical identity; reliable events are never silently
// discarded because an arbitrary local queue limit was reached.
class ClientReplicationInbox final {
  public:
    void Reset();

    bool AcceptPlayerLifecycle(const NetworkPlayerLifecyclePacket& packet);
    bool AcceptPlayerSnapshot(const NetworkPlayerSnapshotPacket& packet);
    bool AcceptSceneEntryState(const NetworkSceneEntryStatePacket& packet,
                               int32_t localPlayerId, uint32_t localLifeEpoch);
    bool AcceptFishingPresentation(const NetworkFishingPresentationPacket& packet);
    bool AcceptFishState(const NetworkFishStatePacket& packet);
    bool AcceptLureState(const NetworkLureStatePacket& packet);
    bool AcceptProjectileLifecycle(const NetworkProjectileLifecyclePacket& packet);
    bool AcceptProjectileIntentResult(const NetworkProjectileIntentResultPacket& packet,
                                      uint32_t localLifeEpoch);
    bool AcceptProjectileState(const NetworkProjectileStatePacket& packet,
                               int32_t localPlayerId);
    bool AcceptCombatResult(const NetworkCombatResultPacket& packet);
    bool AcceptPlayerRespawn(const NetworkPlayerRespawnPacket& packet);
    bool AcceptObjectiveState(const NetworkObjectiveStatePacket& packet);
    bool AcceptStrategicTopology(const NetworkStrategicTopologyPacket& packet);
    bool AcceptStructureState(const NetworkStructureStatePacket& packet);
    bool AcceptCorpseState(const NetworkCorpseStatePacket& packet);

    bool Poll(Game::Simulation::PlayerSnapshot& snapshot);
    bool Poll(Game::Client::LocalSceneAuthority& authority);
    bool Poll(Game::Replication::FishingPresentationState& state);
    bool Poll(Game::Client::RemotePlayerPresentationState& state);
    bool Poll(Game::Client::RemoteFishEntity& state);
    bool Poll(Game::Client::RemoteLureEntity& state);
    bool Poll(Game::Client::RemoteProjectileReplicaState& state);
    bool Poll(Game::Client::LocalProjectileIntentDecision& decision);
    bool Poll(Game::Simulation::CombatResultEvent& event);
    bool Poll(Game::Simulation::PlayerRespawnEvent& event);
    bool Poll(Game::Client::ReplicatedObjectiveState& state);
    bool Poll(Game::Client::ReplicatedStrategicTopologyState& state);
    bool Poll(Game::Client::ReplicatedStructureState& state);
    bool Poll(Game::Client::CorpsePresentationState& state);

    size_t PlayerSnapshotCount() const { return mPlayerSnapshots.Size(); }
    size_t SceneEntryStateCount() const { return mSceneEntryStates.size(); }
    size_t FishingPresentationCount() const { return mFishingPresentations.Size(); }
    size_t PlayerLifecycleCount() const { return mPlayerLifecycles.Size(); }
    size_t ObjectiveStateCount() const { return mObjectiveStates.Size(); }
    size_t StrategicTopologyStateCount() const { return mStrategicTopologyStates.Size(); }
    size_t StructureStateCount() const { return mStructureStates.Size(); }
    size_t FishStateCount() const { return mFishStates.Size(); }
    size_t LureStateCount() const { return mLureStates.Size(); }
    size_t ProjectileStateCount() const { return mProjectileStates.Size(); }
    size_t ProjectileIntentResultCount() const { return mProjectileIntentResults.size(); }
    size_t CombatResultCount() const { return mCombatResults.size(); }
    size_t PlayerRespawnCount() const { return mPlayerRespawns.size(); }
    size_t CorpseStateCount() const { return mCorpseStates.Size(); }

  private:
    using ProjectileLifetimeKey =
        std::tuple<int32_t, int32_t, uint8_t, uint32_t, uint32_t>;
    using ProjectileLogicalKey = std::tuple<int32_t, int32_t, uint8_t>;
    using CorpseEntityKey = uint32_t;

    // Reliable state streams carry the current value of a logical entity. Keep
    // only its newest pending value instead of dropping valid entities at an
    // arbitrary packet count when a large interest set is reconciled at once.
    template <typename Key, typename Packet>
    class CoalescingQueue final {
      public:
        void Push(const Key& key, const Packet& packet) {
            const auto [unused, inserted] = mPackets.insert_or_assign(key, packet);
            (void)unused;
            if (inserted) mOrder.push_back(key);
        }

        bool Poll(Packet& packet) {
            if (mOrder.empty()) return false;
            const Key key = std::move(mOrder.front());
            mOrder.pop_front();
            auto found = mPackets.find(key);
            if (found == mPackets.end()) return false;
            packet = std::move(found->second);
            mPackets.erase(found);
            return true;
        }

        void Clear() {
            mOrder.clear();
            mPackets.clear();
        }

        size_t Size() const { return mPackets.size(); }

        void Erase(const Key& key) {
            if (mPackets.erase(key) == 0) return;
            const auto found = std::find(mOrder.begin(), mOrder.end(), key);
            if (found != mOrder.end()) mOrder.erase(found);
        }

        template <typename Predicate>
        void EraseIf(const Key& key, Predicate predicate) {
            const auto found = mPackets.find(key);
            if (found != mPackets.end() && predicate(found->second)) Erase(key);
        }

      private:
        std::deque<Key> mOrder;
        std::map<Key, Packet> mPackets;
    };

    template <typename Packet>
    static void QueueEvent(std::deque<Packet>& queue, const Packet& packet) {
        queue.push_back(packet);
    }

    template <typename Packet>
    static bool PollQueue(std::deque<Packet>& queue, Packet& packet) {
        if (queue.empty()) return false;
        packet = std::move(queue.front());
        queue.pop_front();
        return true;
    }

    void QueuePlayerLifecycle(
        const Game::Client::RemotePlayerPresentationState& state);
    void QueuePlayerSnapshot(const Game::Simulation::PlayerSnapshot& snapshot);
    void QueueSceneEntryState(const Game::Client::LocalSceneAuthority& authority);
    void QueueFishingPresentation(
        const Game::Replication::FishingPresentationState& state);
    void QueueFishState(const Game::Client::RemoteFishEntity& state);
    void QueueLureState(const Game::Client::RemoteLureEntity& state);
    void QueueProjectileState(const Game::Client::RemoteProjectileReplicaState& state);
    void QueueProjectileIntentResult(
        const Game::Client::LocalProjectileIntentDecision& decision);
    void QueueCombatResult(const Game::Simulation::CombatResultEvent& event);
    void QueuePlayerRespawn(const Game::Simulation::PlayerRespawnEvent& event);
    void QueueObjectiveState(const Game::Client::ReplicatedObjectiveState& state);
    void QueueStrategicTopology(
        const Game::Client::ReplicatedStrategicTopologyState& state);
    void QueueStructureState(const Game::Client::ReplicatedStructureState& state);
    void QueueCorpseState(const Game::Client::CorpsePresentationState& state);

    CoalescingQueue<int32_t, Game::Simulation::PlayerSnapshot> mPlayerSnapshots;
    std::deque<Game::Client::LocalSceneAuthority> mSceneEntryStates;
    CoalescingQueue<int32_t, Game::Client::ReplicatedObjectiveState> mObjectiveStates;
    CoalescingQueue<uint8_t, Game::Client::ReplicatedStrategicTopologyState>
        mStrategicTopologyStates;
    CoalescingQueue<int32_t, Game::Client::ReplicatedStructureState> mStructureStates;
    CoalescingQueue<CorpseEntityKey, Game::Client::CorpsePresentationState>
        mCorpseStates;
    CoalescingQueue<int32_t, Game::Replication::FishingPresentationState>
        mFishingPresentations;
    CoalescingQueue<int32_t, Game::Client::RemotePlayerPresentationState>
        mPlayerLifecycles;
    CoalescingQueue<int32_t, Game::Client::RemoteFishEntity> mFishStates;
    CoalescingQueue<int32_t, Game::Client::RemoteLureEntity> mLureStates;
    CoalescingQueue<ProjectileLogicalKey, Game::Client::RemoteProjectileReplicaState>
        mProjectileStates;
    std::deque<Game::Client::LocalProjectileIntentDecision>
        mProjectileIntentResults;
    std::deque<Game::Simulation::CombatResultEvent> mCombatResults;
    std::deque<Game::Simulation::PlayerRespawnEvent> mPlayerRespawns;

    Game::Replication::EntityLifetimeRegistry mPlayerLifetimes;
    Game::Replication::EntityLifetimeRegistry mFishLifetimes;
    Game::Replication::EntityLifetimeRegistry mLureLifetimes;
    Game::Replication::ProjectileLifetimeRegistry mProjectileLifetimes;
    std::map<int32_t, int32_t> mActivePlayerScenes;
    std::map<int32_t, uint32_t> mLatestPlayerSnapshotTicks;
    std::map<int32_t, uint32_t> mLatestPlayerLifeEpochs;
    std::map<int32_t, uint32_t> mLatestPlayerRespawnEpochs;
    std::map<int32_t, uint32_t> mLatestFishingPresentationSequences;
    std::map<int32_t, uint32_t> mLatestFishStateSequences;
    std::map<int32_t, uint32_t> mLatestLureStateSequences;
    std::map<int32_t, uint32_t> mLatestObjectiveStateSequences;
    uint32_t mLatestStrategicTopologyRevision = 0;
    std::map<int32_t, uint32_t> mLatestStructureStateSequences;
    std::map<CorpseEntityKey, uint32_t> mLatestCorpseStateSequences;
    std::map<ProjectileLifetimeKey, uint32_t> mLatestProjectileSequences;
    uint32_t mLatestCombatEventId = 0;
};

} // namespace Game::Multiplayer
