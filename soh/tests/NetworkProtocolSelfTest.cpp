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
    source.speed = 4.5f;
    source.stateFlags = NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_GROUNDED;
    source.modelGroup = 2;
    source.itemAction = 3;
    for (int limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb) {
        source.jointTable[limb][0] = static_cast<short>(limb * 3);
        source.jointTable[limb][1] = static_cast<short>(limb * -5);
        source.jointTable[limb][2] = static_cast<short>(limb * 7);
    }

    const std::string encoded = BuildAppPacket(NAMTPlayerState, source);
    NetworkPlayerStatePacket decoded{};
    if (!ParseAppPacket(encoded.data(), static_cast<__int32>(encoded.size()), NAMTPlayerState, decoded) ||
        std::memcmp(&source, &decoded, sizeof(source)) != 0) {
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
