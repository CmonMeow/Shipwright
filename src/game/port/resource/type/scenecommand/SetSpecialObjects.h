#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <engine/resource/Resource.h>
#include "SceneCommand.h"
#include <runtime/libultra/types.h>

namespace SOH {
typedef struct {
    int16_t globalObject;
} SpecialObjects;

class SetSpecialObjects : public SceneCommand<SpecialObjects> {
  public:
    using SceneCommand::SceneCommand;

    SpecialObjects* GetPointer();
    size_t GetPointerSize();

    SpecialObjects specialObjects;
};
}; // namespace SOH
