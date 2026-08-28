#pragma once
#include "AudioPlayer.h"

namespace Engine {
class NullAudioPlayer final : public AudioPlayer {
  public:
    NullAudioPlayer(AudioSettings settings) : AudioPlayer(settings) {
    }
    ~NullAudioPlayer();

    int Buffered() override;

  protected:
    bool DoInit() override;
    void DoClose() override;
    void DoPlay(const uint8_t* buf, size_t len) override;
};
} // namespace Engine
