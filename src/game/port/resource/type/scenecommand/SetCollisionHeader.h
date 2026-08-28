#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <engine/resource/Resource.h>
#include "port/resource/type/scenecommand/SceneCommand.h"
#include "port/resource/type/CollisionHeader.h"
// #include <runtime/libultra/types.h>

namespace SOH {
class SetCollisionHeader : public SceneCommand<CollisionHeaderData> {
  public:
    using SceneCommand::SceneCommand;

    CollisionHeaderData* GetPointer();
    size_t GetPointerSize();

    std::string fileName;

    std::shared_ptr<CollisionHeader> collisionHeader;
};
}; // namespace SOH
