#pragma once

#include "engine/resource/Resource.h"

namespace Engine {
class Blob final : public Engine::Resource<void> {
  public:
    using Resource::Resource;

    Blob();

    void* GetPointer() override;
    size_t GetPointerSize() override;

    std::vector<uint8_t> Data;
};
}; // namespace Engine
