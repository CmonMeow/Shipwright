#pragma once

#include <string>
#include <memory>
#include "engine/audio/AudioSettings.h"

namespace Engine {
class AudioPlayer;

class Audio {
  public:
    Audio(AudioSettings settings) : mAudioSettings(settings) {
    }
    ~Audio();

    void Init();
    std::shared_ptr<AudioPlayer> GetAudioPlayer();
    // Set audio channels configuration and reinitialize audio player
    // This can be called at runtime without restarting the game
    void SetAudioChannels(AudioChannelsSetting channels);
    AudioChannelsSetting GetAudioChannels() const;

  protected:
    void InitAudioPlayer();

  private:
    std::shared_ptr<AudioPlayer> mAudioPlayer;
    AudioSettings mAudioSettings;
};
} // namespace Engine
