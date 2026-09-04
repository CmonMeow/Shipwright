#include "LocalVoiceSubmissionService.h"

#include "CommunicationInbox.h"

#include <utility>

namespace Game::Multiplayer {

LocalVoiceSubmissionService::LocalVoiceSubmissionService(
    LocalVoiceSubmissionDelivery delivery)
    : mDelivery(std::move(delivery)) {
}

bool LocalVoiceSubmissionService::Submit(std::vector<uint8_t> opusData) {
    static_assert(Game::Client::kMaximumEncodedVoiceFrameBytes ==
                  VOICE_MAX_OPUS_BYTES);
    if (!mDelivery.role) return false;
    const LocalVoiceSubmissionRole role = mDelivery.role();
    if (role == LocalVoiceSubmissionRole::Inactive) return false;
    const auto frame = mFrames.Issue(std::move(opusData));
    if (!frame) return false;

    NetworkVoiceIntentPacket intent{};
    intent.sequence = frame->sequence;
    intent.codec = VOICE_CODEC_OPUS;
    intent.sampleRate = VOICE_SAMPLE_RATE;
    intent.frameSamples = VOICE_SAMPLES_PER_PACKET;
    intent.data = frame->opusData;
    if (!CommunicationInbox::IsSaneVoice(intent)) return false;

    switch (role) {
        case LocalVoiceSubmissionRole::Client:
            return mDelivery.sendToServer && mDelivery.sendToServer(intent);
        case LocalVoiceSubmissionRole::Host: {
            if (!mDelivery.hostObservers || !mDelivery.sendHostPayload) {
                return false;
            }
            NetworkVoicePacket state{};
            state.playerId = 0;
            state.sequence = intent.sequence;
            state.codec = intent.codec;
            state.sampleRate = intent.sampleRate;
            state.frameSamples = intent.frameSamples;
            state.data = std::move(intent.data);
            const std::string payload = BuildVoicePayload(state);
            bool sent = true;
            for (const int32_t observer : mDelivery.hostObservers()) {
                if (observer > 0) {
                    sent = mDelivery.sendHostPayload(observer, payload) && sent;
                }
            }
            return sent;
        }
        case LocalVoiceSubmissionRole::Inactive:
        default:
            return false;
    }
}

void LocalVoiceSubmissionService::Reset() {
    mFrames.Reset();
}

} // namespace Game::Multiplayer
