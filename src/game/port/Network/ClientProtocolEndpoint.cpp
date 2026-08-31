#include "ClientProtocolEndpoint.h"

#include <runtime/log/Log.h>

#include <utility>

namespace SoH::Network {

ClientProtocolEndpoint::ClientProtocolEndpoint(
    ClientSessionIngress& ingress, cCryptoSession& crypto,
    ClientProtocolHandshakeActions handshake)
    : mIngress(ingress), mCrypto(crypto), mHandshake(std::move(handshake)) {
}

void ClientProtocolEndpoint::OnClientKeyAccept(
    const std::string& keyBytes) {
    if (!mCrypto.acceptServerKey(keyBytes)) return;
    Error("Network runtime: client accepted server encryption key");
    if (mHandshake.sendIdentity) mHandshake.sendIdentity();
    if (mHandshake.sendPrivateChatKey) mHandshake.sendPrivateChatKey();
}

void ClientProtocolEndpoint::OnClientPlayerAssign(
    const NetworkPlayerAssignPacket& packet) {
    mIngress.AssignPlayer(packet);
}
void ClientProtocolEndpoint::OnClientChat(const std::string& text) {
    mIngress.AcceptChat(text);
}
void ClientProtocolEndpoint::OnClientChatKey(const DecodedChatKey& key) {
    mIngress.AcceptChatKey(key);
}
void ClientProtocolEndpoint::OnClientPrivateChat(
    const DecodedPrivateChatToClient& message) {
    mIngress.AcceptPrivateChat(message);
}
void ClientProtocolEndpoint::OnClientPlayerSnapshot(
    const NetworkPlayerSnapshotPacket& packet) {
    mIngress.AcceptPlayerSnapshot(packet);
}
void ClientProtocolEndpoint::OnClientSceneEntryState(
    const NetworkSceneEntryStatePacket& packet) {
    mIngress.AcceptSceneEntryState(packet);
}
void ClientProtocolEndpoint::OnClientObjectiveState(
    const NetworkObjectiveStatePacket& packet) {
    mIngress.AcceptObjectiveState(packet);
}
void ClientProtocolEndpoint::OnClientStrategicTopology(
    const NetworkStrategicTopologyPacket& packet) {
    mIngress.AcceptStrategicTopology(packet);
}
void ClientProtocolEndpoint::OnClientStructureState(
    const NetworkStructureStatePacket& packet) {
    mIngress.AcceptStructureState(packet);
}
void ClientProtocolEndpoint::OnClientCorpseState(
    const NetworkCorpseStatePacket& packet) {
    mIngress.AcceptCorpseState(packet);
}
void ClientProtocolEndpoint::OnClientFishingPresentation(
    const NetworkFishingPresentationPacket& packet) {
    mIngress.AcceptFishingPresentation(packet);
}
void ClientProtocolEndpoint::OnClientPlayerLifecycle(
    const NetworkPlayerLifecyclePacket& packet) {
    mIngress.AcceptPlayerLifecycle(packet);
}
void ClientProtocolEndpoint::OnClientFishState(
    const NetworkFishStatePacket& packet) {
    mIngress.AcceptFishState(packet);
}
void ClientProtocolEndpoint::OnClientLureState(
    const NetworkLureStatePacket& packet) {
    mIngress.AcceptLureState(packet);
}
void ClientProtocolEndpoint::OnClientProjectileLifecycle(
    const NetworkProjectileLifecyclePacket& packet) {
    mIngress.AcceptProjectileLifecycle(packet);
}
void ClientProtocolEndpoint::OnClientProjectileIntentResult(
    const NetworkProjectileIntentResultPacket& packet) {
    mIngress.AcceptProjectileIntentResult(packet);
}
void ClientProtocolEndpoint::OnClientProjectileState(
    const NetworkProjectileStatePacket& packet) {
    mIngress.AcceptProjectileState(packet);
}
void ClientProtocolEndpoint::OnClientCombatResult(
    const NetworkCombatResultPacket& packet) {
    mIngress.AcceptCombatResult(packet);
}
void ClientProtocolEndpoint::OnClientPlayerRespawn(
    const NetworkPlayerRespawnPacket& packet) {
    mIngress.AcceptPlayerRespawn(packet);
}
void ClientProtocolEndpoint::OnClientVoice(NetworkVoicePacket packet) {
    mIngress.AcceptVoice(std::move(packet));
}

} // namespace SoH::Network
