#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <engine/resource/Resource.h>
#include "scenecommand/SceneCommand.h"
#include <runtime/libultra/types.h>

namespace Game::Resources {

class Scene : public Engine::Resource<void> {
  public:
    using Resource::Resource;

    Scene() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
    }

    void* GetPointer();
    size_t GetPointerSize();

    std::vector<std::shared_ptr<ISceneCommand>> commands;
};
}; // namespace Game::Resources
