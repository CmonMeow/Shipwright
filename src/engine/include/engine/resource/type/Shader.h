#pragma once

#include "engine/resource/Resource.h"
namespace Engine {

class Shader final : public Resource<void> {
  public:
    using Resource::Resource;

    Shader();

    void* GetPointer() override;
    size_t GetPointerSize() override;

    std::string Data;
};
}; // namespace Engine
