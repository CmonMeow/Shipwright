#pragma once

#include "NetworkProtocol.h"

#include <cstdint>
#include <string>

namespace Game::Multiplayer {

enum class ProtocolDispatchResult : uint8_t {
    Dispatched,
    Unsupported,
    Malformed,
};

struct DecodedChatKey {
    int32_t playerId = -1;
    std::string playerName;
    std::string publicKey;
};

struct DecodedPrivateChatToClient {
    int32_t senderPlayerId = -1;
    std::string senderName;
    std::string cipherText;
};

struct DecodedPrivateChatToServer {
    int32_t targetPlayerId = -1;
    std::string cipherText;
};

class ClientProtocolSink {
  public:
    virtual ~ClientProtocolSink() = default;

    virtual void OnClientKeyAccept(const std::string&) {}
    virtual void OnClientPlayerAssign(const NetworkPlayerAssignPacket&) {}
    virtual void OnClientChat(const std::string&) {}
    virtual void OnClientChatKey(const DecodedChatKey&) {}
    virtual void OnClientPrivateChat(const DecodedPrivateChatToClient&) {}
    virtual void OnClientPlayerSnapshot(const NetworkPlayerSnapshotPacket&) {}
    virtual void OnClientSceneEntryState(const NetworkSceneEntryStatePacket&) {}
    virtual void OnClientObjectiveState(const NetworkObjectiveStatePacket&) {}
    virtual void OnClientStrategicTopology(const NetworkStrategicTopologyPacket&) {}
    virtual void OnClientStructureState(const NetworkStructureStatePacket&) {}
    virtual void OnClientCorpseState(const NetworkCorpseStatePacket&) {}
    virtual void OnClientFishingPresentation(const NetworkFishingPresentationPacket&) {}
    virtual void OnClientPlayerLifecycle(const NetworkPlayerLifecyclePacket&) {}
    virtual void OnClientFishState(const NetworkFishStatePacket&) {}
    virtual void OnClientLureState(const NetworkLureStatePacket&) {}
    virtual void OnClientProjectileLifecycle(const NetworkProjectileLifecyclePacket&) {}
    virtual void OnClientProjectileIntentResult(const NetworkProjectileIntentResultPacket&) {}
    virtual void OnClientProjectileState(const NetworkProjectileStatePacket&) {}
    virtual void OnClientCombatResult(const NetworkCombatResultPacket&) {}
    virtual void OnClientPlayerRespawn(const NetworkPlayerRespawnPacket&) {}
    virtual void OnClientVoice(NetworkVoicePacket) {}
};

class ServerProtocolSink {
  public:
    virtual ~ServerProtocolSink() = default;

    virtual void OnServerKeyHello(int32_t, const std::string&) {}
    virtual void OnServerIdentity(int32_t, const NetworkIdentity&) {}
    virtual void OnServerChat(int32_t, const std::string&) {}
    virtual void OnServerChatKey(int32_t, const DecodedChatKey&) {}
    virtual void OnServerPrivateChat(int32_t, const DecodedPrivateChatToServer&) {}
    virtual void OnServerSceneEntryIntent(int32_t, NetworkSceneEntryIntentPacket) {}
    virtual void OnServerPlayerCommand(int32_t, NetworkPlayerCommandPacket) {}
    virtual void OnServerWeaponSelection(int32_t, NetworkWeaponSelectionIntentPacket) {}
    virtual void OnServerStructureAction(int32_t, NetworkStructureActionPacket) {}
    virtual void OnServerFishingPresentation(int32_t, NetworkFishingPresentationIntentPacket) {}
    virtual void OnServerFishIntent(int32_t, NetworkFishIntentPacket) {}
    virtual void OnServerLureControlIntent(int32_t, NetworkLureControlIntentPacket) {}
    virtual void OnServerArrowFireIntent(int32_t, NetworkArrowFireIntentPacket) {}
    virtual void OnServerVoice(int32_t, NetworkVoiceIntentPacket) {}
};

class ProtocolDispatcher final {
  public:
    static ProtocolDispatchResult DispatchClient(const char* message, int32_t size,
                                                 ClientProtocolSink& sink);
    static ProtocolDispatchResult DispatchServer(int32_t sender, const char* message, int32_t size,
                                                 bool awaitingIdentity, ServerProtocolSink& sink);
};

} // namespace Game::Multiplayer
