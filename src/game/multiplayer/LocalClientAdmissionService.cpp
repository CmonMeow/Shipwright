#include "LocalClientAdmissionService.h"

#include "LocalNetworkIdentity.h"
#include "PrivateChatService.h"

#include <runtime/log/Log.h>

#include <utility>

namespace Game::Multiplayer {

namespace {

const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

LocalClientAdmissionService::LocalClientAdmissionService(
    cCryptoSession& crypto, PrivateChatService& privateChat,
    LocalClientAdmissionDelivery delivery)
    : mCrypto(crypto), mPrivateChat(privateChat),
      mDelivery(std::move(delivery)) {
}

bool LocalClientAdmissionService::BeginCryptoHandshake() {
    if (!mDelivery.clientActive || !mDelivery.clientActive() ||
        !mDelivery.sendPlain) {
        return false;
    }
    std::string hello;
    if (!mCrypto.buildClientHello(hello)) return false;
    NetworkMessageRaw raw;
    raw.put(hello.data(), static_cast<int32_t>(hello.size()));
    return mDelivery.sendPlain(NAMTKeyHello, raw);
}

bool LocalClientAdmissionService::SubmitIdentity() {
    mIdentitySent = false;
    if (!mDelivery.clientActive || !mDelivery.clientActive() ||
        !mDelivery.sendSecure || !mCrypto.ready()) {
        return false;
    }
    NetworkMessageRaw raw;
    if (!EncodeLocalIdentityRaw(raw, mCrypto.identityBinding())) {
        Error("Client admission: could not create persistent identity proof");
        return false;
    }
    mIdentitySent = mDelivery.sendSecure(NAMTConnect, raw, kReliable);
    return mIdentitySent;
}

bool LocalClientAdmissionService::SubmitPrivateChatKey() {
    if (!mDelivery.clientActive || !mDelivery.clientActive() ||
        !mDelivery.sendSecure || !mPrivateChat.Ready()) {
        return false;
    }
    NetworkMessageRaw raw;
    raw.putInt32(0);
    raw.putString(LocalUserName(), 48);
    raw.putString(mPrivateChat.PublicKey(), crypto_box_PUBLICKEYBYTES);
    return mDelivery.sendSecure(NAMTChatKey, raw, kReliable);
}

void LocalClientAdmissionService::Reset() {
    mIdentitySent = false;
}

} // namespace Game::Multiplayer
