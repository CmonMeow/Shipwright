#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <ship/resource/Resource.h>
#include "SceneCommand.h"
#include "z64.h"

namespace SOH {
class SetStartPositionList : public SceneCommand<ActorEntry> {
  public:
    using SceneCommand::SceneCommand;

    ActorEntry* GetPointer();
    size_t GetPointerSize();

    uint32_t numStartPositions;
    std::vector<ActorEntry> startPositions;
};
}; // namespace SOH
