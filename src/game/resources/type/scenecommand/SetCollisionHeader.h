#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <engine/resource/Resource.h>
#include "resources/type/scenecommand/SceneCommand.h"
#include "resources/type/CollisionHeader.h"
// #include <runtime/libultra/types.h>

namespace Game::Resources {
class SetCollisionHeader : public SceneCommand<CollisionHeaderData> {
  public:
    using SceneCommand::SceneCommand;

    CollisionHeaderData* GetPointer();
    size_t GetPointerSize();

    std::string fileName;

    std::shared_ptr<CollisionHeader> collisionHeader;
};
}; // namespace Game::Resources
