#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <engine/resource/Resource.h>
#include "SceneCommand.h"
#include <runtime/libultra/types.h>

namespace Game::Resources {
typedef struct {
    uint8_t seqId;
    uint8_t natureAmbienceId;
    uint8_t reverb;
} SoundSettings;

class SetSoundSettings : public SceneCommand<SoundSettings> {
  public:
    using SceneCommand::SceneCommand;

    SoundSettings* GetPointer();
    size_t GetPointerSize();

    SoundSettings settings;
};
}; // namespace Game::Resources
