#include "ProtocolDispatcher.h"

#include "sodium.h"

namespace Game::Multiplayer {

namespace {

bool ReadRaw(const char* message, int32_t size, NetAppMessageType expectedType,
             NetworkMessageRaw& raw) {
    if (!message || size < static_cast<int32_t>(sizeof(NetAppMessageHeader))) {
        return false;
    }
    const auto* header = reinterpret_cast<const NetAppMessageHeader*>(message);
    if (header->type != expectedType || !ValidAppMessageType(header->type)) {
        return false;
    }
    raw = NetworkMessageRaw(message + sizeof(NetAppMessageHeader),
                            size - static_cast<int32_t>(sizeof(NetAppMessageHeader)));
    return true;
}

bool DecodeChatKey(const char* message, int32_t size, DecodedChatKey& decoded) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTChatKey, raw) &&
           raw.getInt32(decoded.playerId) && raw.getString(decoded.playerName, 48) &&
           raw.getString(decoded.publicKey, crypto_box_PUBLICKEYBYTES) &&
           decoded.publicKey.size() == crypto_box_PUBLICKEYBYTES && raw.fullyRead();
}

bool DecodePrivateForClient(const char* message, int32_t size,
                            DecodedPrivateChatToClient& decoded) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTPrivateChat, raw) &&
           raw.getInt32(decoded.senderPlayerId) && raw.getString(decoded.senderName, 48) &&
           raw.getString(decoded.cipherText, 255) && raw.fullyRead();
}

bool DecodePrivateForServer(const char* message, int32_t size,
                            DecodedPrivateChatToServer& decoded) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTPrivateChat, raw) &&
           raw.getInt32(decoded.targetPlayerId) && raw.getString(decoded.cipherText, 255) &&
           raw.fullyRead();
}

bool DecodeFishing(const char* message, int32_t size,
                   NetworkFishingPresentationPacket& packet) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTFishingState, raw) && DecodeFishingStateRaw(raw, packet);
}

bool DecodeFishingIntent(const char* message, int32_t size,
                         NetworkFishingPresentationIntentPacket& packet) {
    NetworkMessageRaw raw;
    return ReadRaw(message, size, NAMTFishingState, raw) &&
           DecodeFishingIntentRaw(raw, packet);
}

template <typename Packet, typename Callback>
ProtocolDispatchResult DecodePacket(const char* message, int32_t size, NetAppMessageType type,
                                    Callback&& callback) {
    Packet packet{};
    if (!ParseAppPacket(message, size, type, packet)) {
        return ProtocolDispatchResult::Malformed;
    }
    callback(packet);
    return ProtocolDispatchResult::Dispatched;
}

bool HasValidHeader(const char* message, int32_t size) {
    if (!message || size < static_cast<int32_t>(sizeof(NetAppMessageHeader)) ||
        static_cast<size_t>(size) > NET_MAX_ENCRYPTED_BYTES) {
        return false;
    }
    return ValidAppMessageType(reinterpret_cast<const NetAppMessageHeader*>(message)->type);
}

} // namespace

ProtocolDispatchResult ProtocolDispatcher::DispatchClient(const char* message, int32_t size,
                                                          ClientProtocolSink& sink) {
    if (!HasValidHeader(message, size)) {
        return ProtocolDispatchResult::Malformed;
    }
    const NetAppMessageType type = reinterpret_cast<const NetAppMessageHeader*>(message)->type;
    switch (type) {
        case NAMTKeyAccept: {
            std::string key;
            if (!ParseAppRawBytes(message, size, type, key, crypto_kx_PUBLICKEYBYTES)) {
                return ProtocolDispatchResult::Malformed;
            }
            sink.OnClientKeyAccept(key);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTPlayerAssign:
            return DecodePacket<NetworkPlayerAssignPacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientPlayerAssign(packet); });
        case NAMTChat: {
            std::string text;
            if (!ParseAppRawString(message, size, type, text, CHAT_MAX_LINE_CHARS)) {
                return ProtocolDispatchResult::Malformed;
            }
            sink.OnClientChat(text);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTChatKey: {
            DecodedChatKey decoded;
            if (!DecodeChatKey(message, size, decoded)) return ProtocolDispatchResult::Malformed;
            sink.OnClientChatKey(decoded);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTPrivateChat: {
            DecodedPrivateChatToClient decoded;
            if (!DecodePrivateForClient(message, size, decoded)) return ProtocolDispatchResult::Malformed;
            sink.OnClientPrivateChat(decoded);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTPlayerSnapshot:
            return DecodePacket<NetworkPlayerSnapshotPacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientPlayerSnapshot(packet); });
        case NAMTSceneEntryState:
            return DecodePacket<NetworkSceneEntryStatePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientSceneEntryState(packet); });
        case NAMTObjectiveState:
            return DecodePacket<NetworkObjectiveStatePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientObjectiveState(packet); });
        case NAMTStrategicTopology:
            return DecodePacket<NetworkStrategicTopologyPacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientStrategicTopology(packet); });
        case NAMTStructureState:
            return DecodePacket<NetworkStructureStatePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientStructureState(packet); });
        case NAMTCorpseState:
            return DecodePacket<NetworkCorpseStatePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientCorpseState(packet); });
        case NAMTFishingState: {
            NetworkFishingPresentationPacket packet{};
            if (!DecodeFishing(message, size, packet)) return ProtocolDispatchResult::Malformed;
            sink.OnClientFishingPresentation(packet);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTPlayerLifecycle:
            return DecodePacket<NetworkPlayerLifecyclePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientPlayerLifecycle(packet); });
        case NAMTFishState:
            return DecodePacket<NetworkFishStatePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientFishState(packet); });
        case NAMTLureState:
            return DecodePacket<NetworkLureStatePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientLureState(packet); });
        case NAMTProjectileLifecycle:
            return DecodePacket<NetworkProjectileLifecyclePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientProjectileLifecycle(packet); });
        case NAMTProjectileIntentResult:
            return DecodePacket<NetworkProjectileIntentResultPacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientProjectileIntentResult(packet); });
        case NAMTProjectileState:
            return DecodePacket<NetworkProjectileStatePacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientProjectileState(packet); });
        case NAMTCombatResult:
            return DecodePacket<NetworkCombatResultPacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientCombatResult(packet); });
        case NAMTPlayerRespawn:
            return DecodePacket<NetworkPlayerRespawnPacket>(message, size, type,
                [&sink](const auto& packet) { sink.OnClientPlayerRespawn(packet); });
        case NAMTVoice: {
            NetworkVoicePacket packet;
            if (!ParseVoicePacket(message, size, packet)) return ProtocolDispatchResult::Malformed;
            sink.OnClientVoice(std::move(packet));
            return ProtocolDispatchResult::Dispatched;
        }
        default:
            return ProtocolDispatchResult::Unsupported;
    }
}

ProtocolDispatchResult ProtocolDispatcher::DispatchServer(int32_t sender, const char* message,
                                                          int32_t size, bool awaitingIdentity,
                                                          ServerProtocolSink& sink) {
    if (!HasValidHeader(message, size)) {
        return ProtocolDispatchResult::Malformed;
    }
    const NetAppMessageType type = reinterpret_cast<const NetAppMessageHeader*>(message)->type;
    if (type == NAMTKeyHello) {
        std::string key;
        if (!ParseAppRawBytes(message, size, type, key, crypto_kx_PUBLICKEYBYTES)) {
            return ProtocolDispatchResult::Malformed;
        }
        sink.OnServerKeyHello(sender, key);
        return ProtocolDispatchResult::Dispatched;
    }
    if (awaitingIdentity) {
        NetworkIdentity identity;
        if (type != NAMTConnect || !ParseIdentityRaw(message, size, identity)) {
            return ProtocolDispatchResult::Malformed;
        }
        sink.OnServerIdentity(sender, identity);
        return ProtocolDispatchResult::Dispatched;
    }
    switch (type) {
        case NAMTChat: {
            std::string text;
            if (!ParseAppRawString(message, size, type, text, CHAT_MAX_MESSAGE_CHARS)) {
                return ProtocolDispatchResult::Malformed;
            }
            sink.OnServerChat(sender, text);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTChatKey: {
            DecodedChatKey decoded;
            if (!DecodeChatKey(message, size, decoded)) return ProtocolDispatchResult::Malformed;
            sink.OnServerChatKey(sender, decoded);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTPrivateChat: {
            DecodedPrivateChatToServer decoded;
            if (!DecodePrivateForServer(message, size, decoded)) return ProtocolDispatchResult::Malformed;
            sink.OnServerPrivateChat(sender, decoded);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTSceneEntryIntent:
            return DecodePacket<NetworkSceneEntryIntentPacket>(message, size, type,
                [&sink, sender](auto packet) { sink.OnServerSceneEntryIntent(sender, packet); });
        case NAMTPlayerIntent:
            return DecodePacket<NetworkPlayerCommandPacket>(message, size, type,
                [&sink, sender](auto packet) { sink.OnServerPlayerCommand(sender, packet); });
        case NAMTWeaponSelectionIntent:
            return DecodePacket<NetworkWeaponSelectionIntentPacket>(message, size, type,
                [&sink, sender](auto packet) { sink.OnServerWeaponSelection(sender, packet); });
        case NAMTStructureAction:
            return DecodePacket<NetworkStructureActionPacket>(message, size, type,
                [&sink, sender](auto packet) { sink.OnServerStructureAction(sender, packet); });
        case NAMTFishingState: {
            NetworkFishingPresentationIntentPacket packet{};
            if (!DecodeFishingIntent(message, size, packet)) {
                return ProtocolDispatchResult::Malformed;
            }
            sink.OnServerFishingPresentation(sender, packet);
            return ProtocolDispatchResult::Dispatched;
        }
        case NAMTFishIntent:
            return DecodePacket<NetworkFishIntentPacket>(message, size, type,
                [&sink, sender](auto packet) { sink.OnServerFishIntent(sender, packet); });
        case NAMTLureControlIntent:
            return DecodePacket<NetworkLureControlIntentPacket>(message, size, type,
                [&sink, sender](auto packet) { sink.OnServerLureControlIntent(sender, packet); });
        case NAMTArrowFireIntent:
            return DecodePacket<NetworkArrowFireIntentPacket>(message, size, type,
                [&sink, sender](auto packet) { sink.OnServerArrowFireIntent(sender, packet); });
        case NAMTVoice: {
            NetworkVoiceIntentPacket packet;
            if (!ParseVoiceIntentPacket(message, size, packet)) {
                return ProtocolDispatchResult::Malformed;
            }
            sink.OnServerVoice(sender, std::move(packet));
            return ProtocolDispatchResult::Dispatched;
        }
        default:
            return ProtocolDispatchResult::Unsupported;
    }
}

} // namespace Game::Multiplayer
