#pragma once

#include <cstdint>

#include "engine/audio/AudioChannelsSetting.h"

namespace Engine {

struct AudioSettings {
    int32_t SampleRate = 44100;
    int32_t SampleLength = 1024;
    int32_t DesiredBuffered = 2480;
    AudioChannelsSetting ChannelSetting = AudioChannelsSetting::audioStereo;
};

} // namespace Engine
