#include "ServerSessionManager.h"

#include <algorithm>
#include <utility>

namespace Game::Multiplayer {

ServerSessionManager::~ServerSessionManager() {
    Reset();
}

bool ServerSessionManager::ConnectPeer(int32_t peer) {
    if (peer <= 0 || mSessions.contains(peer)) return false;
    mSessions.try_emplace(peer);
    mPeerOrder.push_back(peer);
    return true;
}

std::optional<ServerSessionDeparture> ServerSessionManager::DisconnectPeer(int32_t peer) {
    const auto found = mSessions.find(peer);
    if (found == mSessions.end()) return std::nullopt;
    ServerSessionDeparture departure{ peer, found->second.identity,
                                      std::move(found->second.moderationReason) };
    found->second.crypto.clear();
    mSessions.erase(found);
    mPeerOrder.erase(std::remove(mPeerOrder.begin(), mPeerOrder.end(), peer), mPeerOrder.end());
    mAdmittedPeerOrder.erase(std::remove(mAdmittedPeerOrder.begin(), mAdmittedPeerOrder.end(), peer),
                             mAdmittedPeerOrder.end());
    return departure;
}

void ServerSessionManager::Reset() {
    for (auto& entry : mSessions) {
        entry.second.crypto.clear();
    }
    mSessions.clear();
    mPeerOrder.clear();
    mAdmittedPeerOrder.clear();
}

bool ServerSessionManager::AdmitIdentity(int32_t peer, NetworkIdentity identity) {
    const auto found = mSessions.find(peer);
    if (found == mSessions.end() || found->second.identity ||
        !identity.authenticated || identity.id.empty() ||
        identity.publicKey.size() != crypto_sign_PUBLICKEYBYTES) return false;
    found->second.identity = std::move(identity);
    mAdmittedPeerOrder.push_back(peer);
    return true;
}

bool ServerSessionManager::HasPeer(int32_t peer) const {
    return mSessions.contains(peer);
}

bool ServerSessionManager::HasIdentity(int32_t peer) const {
    const auto found = mSessions.find(peer);
    return found != mSessions.end() && found->second.identity.has_value();
}

const NetworkIdentity* ServerSessionManager::IdentityFor(int32_t peer) const {
    const auto found = mSessions.find(peer);
    return found == mSessions.end() || !found->second.identity ? nullptr : &*found->second.identity;
}

cCryptoSession* ServerSessionManager::CryptoFor(int32_t peer) {
    const auto found = mSessions.find(peer);
    return found == mSessions.end() ? nullptr : &found->second.crypto;
}

const cCryptoSession* ServerSessionManager::CryptoFor(int32_t peer) const {
    const auto found = mSessions.find(peer);
    return found == mSessions.end() ? nullptr : &found->second.crypto;
}

const std::vector<int32_t>& ServerSessionManager::Peers() const {
    return mPeerOrder;
}

const std::vector<int32_t>& ServerSessionManager::AdmittedPeers() const {
    return mAdmittedPeerOrder;
}

bool ServerSessionManager::AllPeersSecure() const {
    return std::all_of(mPeerOrder.begin(), mPeerOrder.end(), [this](int32_t peer) {
        const auto found = mSessions.find(peer);
        return found != mSessions.end() && found->second.identity && found->second.crypto.ready();
    });
}

size_t ServerSessionManager::PeerCount() const {
    return mPeerOrder.size();
}

bool ServerSessionManager::SetModerationReason(int32_t peer, std::string reason) {
    const auto found = mSessions.find(peer);
    if (found == mSessions.end() || reason.empty()) return false;
    found->second.moderationReason = std::move(reason);
    return true;
}

} // namespace Game::Multiplayer
