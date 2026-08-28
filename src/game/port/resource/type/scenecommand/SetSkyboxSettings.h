#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <engine/resource/Resource.h>
#include "SceneCommand.h"
#include <runtime/libultra/types.h>

namespace SOH {
typedef struct {
    uint8_t unk;
    uint8_t skyboxId;
    uint8_t weather;
    uint8_t indoors;
} SkyboxSettings;

class SetSkyboxSettings : public SceneCommand<SkyboxSettings> {
  public:
    using SceneCommand::SceneCommand;

    SkyboxSettings* GetPointer();
    size_t GetPointerSize();

    SkyboxSettings settings;
};
}; // namespace SOH
