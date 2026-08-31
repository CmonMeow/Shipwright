#include "LocalTextCommunicationService.h"

#include "CommunicationInbox.h"
#include "PrivateChatService.h"

#include <utility>

namespace SoH::Network {

namespace {

const NetMsgFlags kReliable = static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

LocalTextCommunicationService::LocalTextCommunicationService(
    PrivateChatService& privateChat, CommunicationInbox& communication,
    LocalTextCommunicationDelivery delivery)
    : mPrivateChat(privateChat), mCommunication(communication), mDelivery(std::move(delivery)) {
}

bool LocalTextCommunicationService::SendChat(const std::string& message) {
    const std::string text = SanitiseChatText(message);
    if (text.empty() || !mDelivery.role) return false;

    switch (mDelivery.role()) {
        case LocalTextCommunicationRole::Client: {
            if (!mDelivery.sendToServer) return false;
            NetworkMessageRaw raw;
            raw.putString(text, CHAT_MAX_MESSAGE_CHARS);
            return mDelivery.sendToServer(NAMTChat, raw, kReliable);
        }
        case LocalTextCommunicationRole::Host:
            return mDelivery.sendHostChat && mDelivery.sendHostChat(text);
        case LocalTextCommunicationRole::Inactive:
        default:
            return false;
    }
}

bool LocalTextCommunicationService::SendPrivateChat(int32_t targetPlayer,
                                                     const std::string& message) {
    const std::string text = SanitiseChatText(message);
    if (text.empty() || !mDelivery.role) return false;

    switch (mDelivery.role()) {
        case LocalTextCommunicationRole::Client: {
            if (!mDelivery.sendToServer) return false;
            std::string cipher;
            if (!mPrivateChat.EncryptFor(targetPlayer, text, cipher)) return false;
            NetworkMessageRaw raw;
            raw.putInt32(targetPlayer);
            raw.putString(cipher, 255);
            if (!mDelivery.sendToServer(NAMTPrivateChat, raw, kReliable)) return false;
            mCommunication.QueueChat(">" + PlayerName(targetPlayer) + ": " + text,
                                     CLKPrivate);
            return true;
        }
        case LocalTextCommunicationRole::Host:
            return mDelivery.sendHostPrivateChat &&
                   mDelivery.sendHostPrivateChat(targetPlayer, text);
        case LocalTextCommunicationRole::Inactive:
        default:
            return false;
    }
}

std::string LocalTextCommunicationService::PlayerName(int32_t player) const {
    const std::string peerName = mPrivateChat.PeerName(player);
    if (!peerName.empty()) return peerName;
    if (mDelivery.playerName) return mDelivery.playerName(player);
    return std::to_string(player);
}

} // namespace SoH::Network
