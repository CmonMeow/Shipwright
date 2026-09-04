#include "ClientSessionIngress.h"

#include <utility>

namespace Game::Multiplayer {

ClientSessionIngress::ClientSessionIngress(
    ClientReplicationInbox& replication, CommunicationInbox& communication,
    PrivateChatService& privateChat)
    : mReplication(replication), mCommunication(communication),
      mPrivateChat(privateChat) {
}

void ClientSessionIngress::Reset() {
    mLocalPlayerId = -1;
    mLocalLifeEpoch = 0;
    mReplication.Reset();
    mCommunication.ResetVoiceSession();
    mPrivateChat.ResetPeers();
}

bool ClientSessionIngress::AssignPlayer(
    const NetworkPlayerAssignPacket& assignment) {
    if (assignment.playerId <= 0 ||
        (mLocalPlayerId >= 0 && assignment.playerId != mLocalPlayerId)) {
        return false;
    }
    mLocalPlayerId = assignment.playerId;
    return true;
}

bool ClientSessionIngress::AcceptChat(const std::string& received) {
    const std::string text = SanitiseChatLine(received);
    if (text.empty()) return false;
    mCommunication.QueueChat(
        text, text.rfind("system:", 0) == 0 ? CLKSystem : CLKNormal);
    return true;
}

bool ClientSessionIngress::AcceptChatKey(const DecodedChatKey& key) {
    return mPrivateChat.SetPeer(key.playerId, key.playerName, key.publicKey);
}

bool ClientSessionIngress::AcceptPrivateChat(
    const DecodedPrivateChatToClient& message) {
    std::string text;
    if (message.senderPlayerId < 0 ||
        !mPrivateChat.Decrypt(message.cipherText, text)) {
        return false;
    }
    std::string sender = SanitiseIdentityText(message.senderName, 48);
    if (sender.empty()) sender = "player " + std::to_string(message.senderPlayerId);
    mCommunication.QueueChat("(private) " + sender + ": " + text,
                             CLKPrivate);
    return true;
}

bool ClientSessionIngress::AcceptPlayerSnapshot(
    const NetworkPlayerSnapshotPacket& packet) {
    if (!mReplication.AcceptPlayerSnapshot(packet)) return false;
    if (packet.playerId == mLocalPlayerId) mLocalLifeEpoch = packet.lifeEpoch;
    return true;
}

bool ClientSessionIngress::AcceptSceneEntryState(
    const NetworkSceneEntryStatePacket& packet) {
    if (!mReplication.AcceptSceneEntryState(packet, mLocalPlayerId,
                                            mLocalLifeEpoch)) {
        return false;
    }
    if (mLocalLifeEpoch == 0 && packet.requestSequence == 0) {
        mLocalLifeEpoch = packet.lifeEpoch;
    }
    return true;
}

bool ClientSessionIngress::AcceptObjectiveState(
    const NetworkObjectiveStatePacket& packet) {
    return mReplication.AcceptObjectiveState(packet);
}

bool ClientSessionIngress::AcceptStrategicTopology(
    const NetworkStrategicTopologyPacket& packet) {
    return mReplication.AcceptStrategicTopology(packet);
}

bool ClientSessionIngress::AcceptStructureState(
    const NetworkStructureStatePacket& packet) {
    return mReplication.AcceptStructureState(packet);
}

bool ClientSessionIngress::AcceptCorpseState(
    const NetworkCorpseStatePacket& packet) {
    return mReplication.AcceptCorpseState(packet);
}

bool ClientSessionIngress::AcceptFishingPresentation(
    const NetworkFishingPresentationPacket& packet) {
    return mReplication.AcceptFishingPresentation(packet, mLocalPlayerId);
}

bool ClientSessionIngress::AcceptPlayerLifecycle(
    const NetworkPlayerLifecyclePacket& packet) {
    if (!mReplication.AcceptPlayerLifecycle(packet)) return false;
    if (packet.active) {
        mCommunication.ActivateVoicePlayer(packet.playerId);
    } else {
        mCommunication.ForgetVoicePlayer(packet.playerId);
    }
    return true;
}

bool ClientSessionIngress::AcceptFishState(
    const NetworkFishStatePacket& packet) {
    return mReplication.AcceptFishState(packet);
}

bool ClientSessionIngress::AcceptLureState(
    const NetworkLureStatePacket& packet) {
    return mReplication.AcceptLureState(packet);
}

bool ClientSessionIngress::AcceptProjectileLifecycle(
    const NetworkProjectileLifecyclePacket& packet) {
    return mReplication.AcceptProjectileLifecycle(packet);
}

bool ClientSessionIngress::AcceptProjectileIntentResult(
    const NetworkProjectileIntentResultPacket& packet) {
    return mReplication.AcceptProjectileIntentResult(packet, mLocalLifeEpoch);
}

bool ClientSessionIngress::AcceptProjectileState(
    const NetworkProjectileStatePacket& packet) {
    return mReplication.AcceptProjectileState(packet, mLocalPlayerId);
}

bool ClientSessionIngress::AcceptCombatResult(
    const NetworkCombatResultPacket& packet) {
    return mReplication.AcceptCombatResult(packet);
}

bool ClientSessionIngress::AcceptPlayerRespawn(
    const NetworkPlayerRespawnPacket& packet) {
    if (packet.playerId != mLocalPlayerId ||
        !mReplication.AcceptPlayerRespawn(packet)) {
        return false;
    }
    mLocalLifeEpoch = packet.lifeEpoch;
    return true;
}

bool ClientSessionIngress::AcceptVoice(NetworkVoicePacket packet) {
    return packet.playerId != mLocalPlayerId &&
           mCommunication.QueueVoice(std::move(packet));
}

} // namespace Game::Multiplayer
