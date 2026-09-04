#pragma once

#include <vector>
#include <string>
#include <engine/resource/Resource.h>

namespace Game::Resources {
class PlayerAnimation : public Engine::Resource<int16_t> {
  public:
    using Resource::Resource;

    PlayerAnimation() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
    }

    int16_t* GetPointer();
    size_t GetPointerSize();

    std::vector<int16_t> limbRotData;
};
} // namespace Game::Resources