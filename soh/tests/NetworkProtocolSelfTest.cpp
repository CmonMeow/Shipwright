#include <sysdef.h>

#include "Network/NetworkProtocol.h"

#include <cstring>
#include <string>

namespace {

bool TestSessionEncryption() {
    cCryptoSession client;
    cCryptoSession server;
    std::string clientKey;
    std::string serverKey;

    if (!client.buildClientHello(clientKey) || !server.acceptClientHello(clientKey, serverKey) ||
        !client.acceptServerKey(serverKey) || !client.ready() || !server.ready()) {
        return false;
    }

    const std::string clientPlain = "client encrypted payload";
    std::string clientCipher;
    std::string serverPlain;
    if (!client.encrypt(clientPlain, clientCipher) || !server.decrypt(clientCipher.data(),
                                                                      static_cast<__int32>(clientCipher.size()),
                                                                      serverPlain) ||
        serverPlain != clientPlain) {
        return false;
    }

    const std::string serverPlainSource = "server encrypted payload";
    std::string serverCipher;
    std::string clientPlainResult;
    if (!server.encrypt(serverPlainSource, serverCipher) ||
        !client.decrypt(serverCipher.data(), static_cast<__int32>(serverCipher.size()), clientPlainResult) ||
        clientPlainResult != serverPlainSource) {
        return false;
    }

    serverCipher.back() ^= 1;
    std::string tamperedPlain;
    return !client.decrypt(serverCipher.data(), static_cast<__int32>(serverCipher.size()), tamperedPlain);
}

bool TestPrivateChatEncryption() {
    unsigned char recipientPublic[crypto_box_PUBLICKEYBYTES];
    unsigned char recipientSecret[crypto_box_SECRETKEYBYTES];
    if (crypto_box_keypair(recipientPublic, recipientSecret) != 0) {
        return false;
    }

    const std::string message = "end-to-end private text";
    std::string cipher(message.size() + crypto_box_SEALBYTES, '\0');
    if (crypto_box_seal(reinterpret_cast<unsigned char*>(cipher.data()),
                        reinterpret_cast<const unsigned char*>(message.data()), message.size(), recipientPublic) != 0) {
        return false;
    }

    std::string plain(message.size(), '\0');
    const bool opened = crypto_box_seal_open(reinterpret_cast<unsigned char*>(plain.data()),
                                              reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size(),
                                              recipientPublic, recipientSecret) == 0;
    sodium_memzero(recipientSecret, sizeof(recipientSecret));
    return opened && plain == message;
}

bool TestPacketSerialization() {
    NetworkPlayerStatePacket source{};
    source.playerId = 7;
    source.sceneId = 42;
    source.roomId = 3;
    source.sequence = 99;
    source.x = -1234.25f;
    source.y = 5678.5f;
    source.z = 44.75f;
    source.rotationX = -123;
    source.rotationY = 12345;
    source.rotationZ = 321;
    source.aimPitch = -2048;
    source.aimYaw = 8192;
    source.speed = 4.5f;
    source.bowStringScale = 0.625f;
    source.stateFlags = NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_GROUNDED;
    source.modelGroup = 2;
    source.itemAction = NETWORK_PLAYER_ITEM_FISHING_POLE;
    source.fishingState = 4;
    source.meleeWeaponState = 2;
    source.meleeBase[0] = 10.0f;
    source.meleeBase[1] = 20.0f;
    source.meleeTip[2] = 75.0f;
    source.fishingLineSpooled = 137;
    source.fishingLineHooked = 1;
    source.fishingLureType = 2;
    source.fishingSinkingLureUnderwater = 1;
    source.fishingLureDrawOffset[2] = 9.5f;
    source.fishingLureSpin = 0.375f;
    source.fishingLureZOffset = -725.0f;
    source.fishingLureHookOffsets[0][0] = 19.25f;
    source.fishingLureHookOffsets[1][2] = -8.5f;
    source.fishingLureHookRot[0][0] = 0.75f;
    source.fishingLureHookRot[1][1] = -1.25f;
    source.fishingLineScale = 0.0015f;
    source.fishingLineGravity = 2.25f;
    source.fishingFishActive = 1;
    source.fishingFishIsLoach = 1;
    source.fishingFishOffset[0] = 12.5f;
    source.fishingFishOffset[1] = -7.25f;
    source.fishingFishOffset[2] = 44.0f;
    source.fishingFishRot[0] = -3000;
    source.fishingFishRot[1] = 12000;
    source.fishingFishRot[2] = 900;
    source.fishingFishLength = 61.5f;
    source.fishingFishRoomId = -1;
    source.fishingFishActorParams = 400;
    source.fishingFishHomeX = 125;
    source.fishingFishHomeY = -30;
    source.fishingFishHomeZ = 450;
    for (int limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb) {
        source.jointTable[limb][0] = static_cast<short>(limb * 3);
        source.jointTable[limb][1] = static_cast<short>(limb * -5);
        source.jointTable[limb][2] = static_cast<short>(limb * 7);
    }

    const std::string encoded = BuildAppPacket(NAMTPlayerState, source);
    NetworkPlayerStatePacket decoded{};
    NetworkPlayerStatePacket expectedPose = source;
    NetworkPlayerStatePacket emptyFishing{};
    CopyNetworkFishingState(expectedPose, emptyFishing);
    // Movement remains below the compact high-rate datagram budget even when
    // the fishing pole is equipped.
    if (encoded.size() >= 256 ||
        !ParseAppPacket(encoded.data(), static_cast<__int32>(encoded.size()), NAMTPlayerState, decoded) ||
        std::memcmp(&expectedPose, &decoded, sizeof(source)) != 0) {
        return false;
    }
    NetworkMessageRaw fishingRaw;
    EncodeFishingStateRaw(fishingRaw, source);
    NetworkMessageRaw encodedFishing(fishingRaw.data(), fishingRaw.size());
    NetworkPlayerStatePacket decodedFishing{};
    if (fishingRaw.size() >= 256 || !DecodeFishingStateRaw(encodedFishing, decodedFishing) ||
        decodedFishing.playerId != source.playerId || decodedFishing.sceneId != source.sceneId ||
        decodedFishing.sequence != source.sequence || decodedFishing.fishingState != source.fishingState ||
        decodedFishing.fishingLureDrawOffset[2] != source.fishingLureDrawOffset[2] ||
        decodedFishing.fishingFishLength != source.fishingFishLength ||
        decodedFishing.fishingFishActorParams != source.fishingFishActorParams ||
        decodedFishing.fishingFishHomeZ != source.fishingFishHomeZ) {
        return false;
    }

    NetworkPlayerStatePacket ordinary{};
    ordinary.playerId = 8;
    ordinary.sceneId = 42;
    ordinary.roomId = 3;
    ordinary.sequence = 100;
    ordinary.x = 14.0f;
    ordinary.y = 28.0f;
    ordinary.z = -7.0f;
    ordinary.rotationY = 4096;
    ordinary.stateFlags = NETWORK_PLAYER_VISIBLE;
    ordinary.modelGroup = 2;
    ordinary.itemAction = 3;
    ordinary.jointTable[NETWORK_PLAYER_LIMB_COUNT - 1][2] = -1234;
    const std::string ordinaryEncoded = BuildAppPacket(NAMTPlayerState, ordinary);
    NetworkPlayerStatePacket ordinaryDecoded{};
    if (ordinaryEncoded.size() >= 1400 ||
        !ParseAppPacket(ordinaryEncoded.data(), static_cast<__int32>(ordinaryEncoded.size()),
                        NAMTPlayerState, ordinaryDecoded) ||
        std::memcmp(&ordinary, &ordinaryDecoded, sizeof(ordinary)) != 0) {
        return false;
    }

    NetworkProjectileImpactPacket impactSource{ 4, 81, 42, -15.5f, 250.25f, 99.0f };
    const std::string impactEncoded = BuildAppPacket(NAMTProjectileImpact, impactSource);
    NetworkProjectileImpactPacket impactDecoded{};
    if (!ParseAppPacket(impactEncoded.data(), static_cast<__int32>(impactEncoded.size()),
                        NAMTProjectileImpact, impactDecoded) ||
        std::memcmp(&impactSource, &impactDecoded, sizeof(impactSource)) != 0) {
        return false;
    }

    NetworkProjectileStatePacket projectileSource{};
    projectileSource.playerId = 4;
    projectileSource.projectileId = 81;
    projectileSource.sceneId = 42;
    projectileSource.sequence = 0x12345678;
    projectileSource.active = 1;
    projectileSource.projectileKind = NETWORK_PROJECTILE_ARROW;
    projectileSource.phase = NETWORK_ARROW_STUCK;
    projectileSource.projectileType = 2;
    projectileSource.x = -15.5f;
    projectileSource.y = 250.25f;
    projectileSource.z = 99.0f;
    projectileSource.rotationX = 0x4000;
    projectileSource.rotationY = -1234;
    projectileSource.velocityZ = 3000.0f;
    const std::string projectileEncoded = BuildAppPacket(NAMTDynamicObjectStateRaw, projectileSource);
    NetworkProjectileStatePacket projectileDecoded{};
    if (!ParseAppPacket(projectileEncoded.data(), static_cast<__int32>(projectileEncoded.size()),
                        NAMTDynamicObjectStateRaw, projectileDecoded) ||
        std::memcmp(&projectileSource, &projectileDecoded, sizeof(projectileSource)) != 0) {
        return false;
    }

    NetworkActorEventPacket actorEvent{ 7, 123, 42, 3, 0x127, -17, 100, 200, 300, 101.0f, 202.0f, 303.0f,
                                        NETWORK_ACTOR_EVENT_BOULDER_BREAK };
    const std::string encodedEvent = BuildAppPacket(NAMTActorEvent, actorEvent);
    NetworkActorEventPacket decodedEvent{};
    if (!ParseAppPacket(encodedEvent.data(), static_cast<__int32>(encodedEvent.size()), NAMTActorEvent,
                        decodedEvent) ||
        std::memcmp(&actorEvent, &decodedEvent, sizeof(actorEvent)) != 0) {
        return false;
    }

    NetworkPlayerRespawnPacket respawnSource{ 7 };
    const std::string respawnEncoded = BuildAppPacket(NAMTPlayerRespawn, respawnSource);
    NetworkPlayerRespawnPacket respawnDecoded{};
    if (!ParseAppPacket(respawnEncoded.data(), static_cast<__int32>(respawnEncoded.size()), NAMTPlayerRespawn,
                        respawnDecoded) ||
        std::memcmp(&respawnSource, &respawnDecoded, sizeof(respawnSource)) != 0) {
        return false;
    }

    return true;
}

} // namespace

int main() {
    ClearLog();
    if (sodium_init() < 0) {
        Error("Network protocol self-test: libsodium initialization failed");
        return 1;
    }
    if (!TestSessionEncryption()) {
        Error("Network protocol self-test: encrypted session failed");
        return 2;
    }
    if (!TestPrivateChatEncryption()) {
        Error("Network protocol self-test: private chat sealed box failed");
        return 3;
    }
    if (!TestPacketSerialization()) {
        Error("Network protocol self-test: packet serialization failed");
        return 4;
    }

    Error("Network protocol self-test passed: session encryption, private E2E text, and packet serialization");
    return 0;
}
