#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Game::Client {

constexpr size_t kMaximumEncodedVoiceFrameBytes = 1275;

struct LocalVoiceFrame {
    uint32_t sequence = 0;
    std::vector<uint8_t> opusData;
};

// Owns transient voice-frame identities before wire serialization. Capture
// code supplies only encoded Opus bytes; protocol codec/rate/frame metadata is
// assigned by the network boundary and cannot be forged by UI code.
class LocalVoiceFrameStream final {
  public:
    explicit LocalVoiceFrameStream(uint32_t nextSequence = 1);

    std::optional<LocalVoiceFrame> Issue(std::vector<uint8_t> opusData);
    void Reset();

  private:
    uint32_t TakeSequence();

    uint32_t mNextSequence = 1;
};

} // namespace Game::Client
