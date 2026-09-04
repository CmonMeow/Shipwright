#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <engine/resource/Resource.h>
#include "SceneCommand.h"
#include <runtime/libultra/types.h>

namespace Game::Resources {
typedef struct {

} Marker;

class EndMarker : public SceneCommand<Marker> {
  public:
    using SceneCommand::SceneCommand;

    Marker* GetPointer();
    size_t GetPointerSize();

    Marker endMarker;
};
}; // namespace Game::Resources
