#include "PrivateChatService.h"

#include "sodium.h"

namespace Game::Multiplayer {

PrivateChatService::PrivateChatService() = default;

PrivateChatService::~PrivateChatService() {
    sodium_memzero(mSecretKey, sizeof(mSecretKey));
}

bool PrivateChatService::Initialize() {
    if (mKeyReady) return true;
    if (crypto_box_keypair(mPublicKey, mSecretKey) != 0) return false;
    mKeyReady = true;
    ResetPeers();
    return true;
}

void PrivateChatService::ResetPeers() {
    mPeers.clear();
    if (mKeyReady) SetPeer(0, "system", PublicKey());
}

bool PrivateChatService::SetPeer(int32_t playerId, const std::string& playerName,
                                 const std::string& publicKey) {
    if (playerId < 0 || publicKey.size() != crypto_box_PUBLICKEYBYTES) return false;
    const std::string cleanName = SanitiseIdentityText(playerName, 48);
    if (cleanName.empty()) return false;
    mPeers[playerId] = { playerId, cleanName, publicKey };
    return true;
}

void PrivateChatService::RemovePeer(int32_t playerId) {
    if (playerId != 0) mPeers.erase(playerId);
}

std::string PrivateChatService::PeerName(int32_t playerId) const {
    const auto found = mPeers.find(playerId);
    return found == mPeers.end() ? std::string() : found->second.playerName;
}

std::vector<PrivateChatPeer> PrivateChatService::Peers() const {
    std::vector<PrivateChatPeer> result;
    result.reserve(mPeers.size());
    for (const auto& [playerId, peer] : mPeers) result.push_back(peer);
    return result;
}

std::string PrivateChatService::PublicKey() const {
    if (!mKeyReady) return {};
    return std::string(reinterpret_cast<const char*>(mPublicKey), sizeof(mPublicKey));
}

bool PrivateChatService::EncryptFor(int32_t targetPlayerId, const std::string& message,
                                    std::string& cipher) const {
    cipher.clear();
    const auto peer = mPeers.find(targetPlayerId);
    if (message.empty() || peer == mPeers.end() ||
        peer->second.publicKey.size() != crypto_box_PUBLICKEYBYTES) return false;
    cipher.resize(message.size() + crypto_box_SEALBYTES);
    if (crypto_box_seal(reinterpret_cast<unsigned char*>(cipher.data()),
                        reinterpret_cast<const unsigned char*>(message.data()), message.size(),
                        reinterpret_cast<const unsigned char*>(peer->second.publicKey.data())) != 0) {
        cipher.clear();
        return false;
    }
    return true;
}

bool PrivateChatService::Decrypt(const std::string& cipher, std::string& message) const {
    message.clear();
    if (!mKeyReady || cipher.size() < crypto_box_SEALBYTES) return false;
    message.resize(cipher.size() - crypto_box_SEALBYTES);
    if (crypto_box_seal_open(reinterpret_cast<unsigned char*>(message.data()),
                             reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size(),
                             mPublicKey, mSecretKey) != 0) {
        message.clear();
        return false;
    }
    message = SanitiseChatText(message);
    return !message.empty();
}

} // namespace Game::Multiplayer
