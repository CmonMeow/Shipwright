#pragma once

#include "engine/resource/Resource.h"
#include <nlohmann/json.hpp>

namespace Engine {

class Json final : public Resource<void> {
  public:
    using Resource::Resource;

    Json();

    void* GetPointer() override;
    size_t GetPointerSize() override;

    nlohmann::json Data;
    size_t DataSize;
};
}; // namespace Engine
