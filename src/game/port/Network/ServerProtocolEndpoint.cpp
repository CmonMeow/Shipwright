#include "ServerProtocolEndpoint.h"

#include "LocalNetworkIdentity.h"
#include "SecureTransportChannel.h"
#include "ServerCommunicationService.h"
#include "ServerGameplayPacketIngress.h"
#include "ServerPlayerSessionService.h"
#include "ServerSessionManager.h"

#include <runtime/log/Log.h>

#include <utility>

namespace SoH::Network {

ServerProtocolEndpoint::ServerProtocolEndpoint(
    ServerSessionManager& sessions, SecureTransportChannel& secureTransport,
    ServerPlayerSessionService& playerSessions,
    ServerCommunicationService& communication,
    ServerGameplayPacketIngress& gameplay,
    ServerProtocolEndpointTransport transport)
    : mSessions(sessions), mSecureTransport(secureTransport),
      mPlayerSessions(playerSessions), mCommunication(communication),
      mGameplay(gameplay), mTransport(std::move(transport)) {
}

void ServerProtocolEndpoint::Kick(int32_t sender,
                                  const std::string& reason) const {
    if (mTransport.kick) mTransport.kick(sender, reason);
}

void ServerProtocolEndpoint::OnServerKeyHello(
    int32_t sender, const std::string& keyBytes) {
    std::string response;
    cCryptoSession* crypto = mSessions.CryptoFor(sender);
    if (!crypto || !crypto->acceptClientHello(keyBytes, response)) return;
    NetworkMessageRaw raw;
    raw.put(response.data(), static_cast<__int32>(response.size()));
    mSecureTransport.SendPlainToPeer(sender, NAMTKeyAccept, raw);
    Error("Network runtime: server accepted encryption key from %d", sender);
}

void ServerProtocolEndpoint::OnServerIdentity(
    int32_t sender, const NetworkIdentity& identity) {
    cCryptoSession* crypto = mSessions.CryptoFor(sender);
    const std::string binding = crypto ? crypto->identityBinding()
                                       : std::string();
    if (!crypto || !crypto->ready() ||
        !VerifyIdentityBinding(identity.publicKey, binding,
                               identity.signature)) {
        Kick(sender, "identity proof failed");
        return;
    }
    NetworkIdentity authenticated = identity;
    authenticated.id = IdentityIdFromPublicKey(authenticated.publicKey);
    authenticated.authenticated = !authenticated.id.empty();
    if (!authenticated.authenticated) {
        Kick(sender, "identity proof failed");
        return;
    }
    mPlayerSessions.AdmitIdentity(sender, authenticated);
}

void ServerProtocolEndpoint::OnServerChat(int32_t sender,
                                          const std::string& text) {
    mCommunication.HandleChat(sender, text);
}
void ServerProtocolEndpoint::OnServerChatKey(int32_t sender,
                                             const DecodedChatKey& key) {
    mCommunication.HandleChatKey(sender, key);
}
void ServerProtocolEndpoint::OnServerPrivateChat(
    int32_t sender, const DecodedPrivateChatToServer& message) {
    mCommunication.HandlePrivateChat(sender, message);
}
void ServerProtocolEndpoint::OnServerSceneEntryIntent(
    int32_t sender, NetworkSceneEntryIntentPacket packet) {
    mGameplay.EnterScene(sender, packet);
}
void ServerProtocolEndpoint::OnServerPlayerCommand(
    int32_t sender, NetworkPlayerCommandPacket packet) {
    mGameplay.SubmitPlayerCommand(sender, packet);
}
void ServerProtocolEndpoint::OnServerWeaponSelection(
    int32_t sender, NetworkWeaponSelectionIntentPacket packet) {
    mGameplay.SelectWeapon(sender, packet);
}
void ServerProtocolEndpoint::OnServerStructureAction(
    int32_t sender, NetworkStructureActionPacket packet) {
    mGameplay.SubmitStructureAction(sender, packet);
}
void ServerProtocolEndpoint::OnServerFishingPresentation(
    int32_t sender, NetworkFishingPresentationIntentPacket packet) {
    mGameplay.SubmitFishingPresentation(sender, packet);
}
void ServerProtocolEndpoint::OnServerFishIntent(
    int32_t sender, NetworkFishIntentPacket packet) {
    mGameplay.SubmitFishAction(sender, packet);
}
void ServerProtocolEndpoint::OnServerLureControlIntent(
    int32_t sender, NetworkLureControlIntentPacket packet) {
    mGameplay.SubmitLureControl(sender, packet);
}
void ServerProtocolEndpoint::OnServerArrowFireIntent(
    int32_t sender, NetworkArrowFireIntentPacket packet) {
    mGameplay.FireProjectile(sender, packet);
}
void ServerProtocolEndpoint::OnServerVoice(
    int32_t sender, NetworkVoiceIntentPacket packet) {
    mCommunication.HandleVoice(sender, std::move(packet));
}

} // namespace SoH::Network
