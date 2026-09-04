#pragma once

#include "ClientReplicationInbox.h"
#include "CommunicationInbox.h"
#include "PrivateChatService.h"
#include "ProtocolDispatcher.h"

#include <cstdint>

namespace Game::Multiplayer {

// Owns all state admitted from the server into one client session. Local player
// identity and incarnation advance only with accepted replicated state, and one
// reset retires every session-scoped queue, voice floor, and peer key.
class ClientSessionIngress final {
  public:
    ClientSessionIngress(ClientReplicationInbox& replication,
                         CommunicationInbox& communication,
                         PrivateChatService& privateChat);

    void Reset();
    int32_t LocalPlayerId() const { return mLocalPlayerId; }
    uint32_t LocalLifeEpoch() const { return mLocalLifeEpoch; }

    bool AssignPlayer(const NetworkPlayerAssignPacket& assignment);
    bool AcceptChat(const std::string& text);
    bool AcceptChatKey(const DecodedChatKey& key);
    bool AcceptPrivateChat(const DecodedPrivateChatToClient& message);
    bool AcceptPlayerSnapshot(const NetworkPlayerSnapshotPacket& packet);
    bool AcceptSceneEntryState(const NetworkSceneEntryStatePacket& packet);
    bool AcceptObjectiveState(const NetworkObjectiveStatePacket& packet);
    bool AcceptStrategicTopology(const NetworkStrategicTopologyPacket& packet);
    bool AcceptStructureState(const NetworkStructureStatePacket& packet);
    bool AcceptCorpseState(const NetworkCorpseStatePacket& packet);
    bool AcceptFishingPresentation(const NetworkFishingPresentationPacket& packet);
    bool AcceptPlayerLifecycle(const NetworkPlayerLifecyclePacket& packet);
    bool AcceptFishState(const NetworkFishStatePacket& packet);
    bool AcceptLureState(const NetworkLureStatePacket& packet);
    bool AcceptProjectileLifecycle(const NetworkProjectileLifecyclePacket& packet);
    bool AcceptProjectileIntentResult(
        const NetworkProjectileIntentResultPacket& packet);
    bool AcceptProjectileState(const NetworkProjectileStatePacket& packet);
    bool AcceptCombatResult(const NetworkCombatResultPacket& packet);
    bool AcceptPlayerRespawn(const NetworkPlayerRespawnPacket& packet);
    bool AcceptVoice(NetworkVoicePacket packet);

  private:
    ClientReplicationInbox& mReplication;
    CommunicationInbox& mCommunication;
    PrivateChatService& mPrivateChat;
    int32_t mLocalPlayerId = -1;
    uint32_t mLocalLifeEpoch = 0;
};

} // namespace Game::Multiplayer
