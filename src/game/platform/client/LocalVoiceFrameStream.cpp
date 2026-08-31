#include "LocalVoiceFrameStream.h"

#include <utility>

namespace Game::Client {

LocalVoiceFrameStream::LocalVoiceFrameStream(uint32_t nextSequence)
    : mNextSequence(nextSequence == 0 ? 1 : nextSequence) {
}

std::optional<LocalVoiceFrame> LocalVoiceFrameStream::Issue(
    std::vector<uint8_t> opusData) {
    if (opusData.empty() || opusData.size() > kMaximumEncodedVoiceFrameBytes) {
        return std::nullopt;
    }
    return LocalVoiceFrame{ TakeSequence(), std::move(opusData) };
}

void LocalVoiceFrameStream::Reset() {
    mNextSequence = 1;
}

uint32_t LocalVoiceFrameStream::TakeSequence() {
    const uint32_t sequence = mNextSequence++;
    if (mNextSequence == 0) mNextSequence = 1;
    return sequence;
}

} // namespace Game::Client
