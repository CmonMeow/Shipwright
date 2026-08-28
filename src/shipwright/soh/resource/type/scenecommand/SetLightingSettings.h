#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <ship/resource/Resource.h>
#include "SceneCommand.h"
#include <libultraship/libultra/types.h>

namespace SOH {
typedef struct {
    /* 0x00 */ uint8_t ambientColor[3];
    /* 0x03 */ int8_t light1Dir[3];
    /* 0x06 */ uint8_t light1Color[3];
    /* 0x09 */ int8_t light2Dir[3];
    /* 0x0C */ uint8_t light2Color[3];
    /* 0x0F */ uint8_t fogColor[3];
    /* 0x12 */ int16_t fogNear;
    /* 0x14 */ int16_t fogFar;
} EnvLightSettings; // size = 0x16

class SetLightingSettings final : public SceneCommand<EnvLightSettings> {
  public:
    using SceneCommand::SceneCommand;

    EnvLightSettings* GetPointer();
    size_t GetPointerSize();

    std::vector<EnvLightSettings> settings;
};
}; // namespace SOH
