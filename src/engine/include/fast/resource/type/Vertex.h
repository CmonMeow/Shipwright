#pragma once

#include "engine/resource/Resource.h"
#include <vector>

union Vtx;

namespace Fast {
class Vertex final : public Engine::Resource<Vtx> {
  public:
    using Resource::Resource;

    Vertex();

    Vtx* GetPointer() override;
    size_t GetPointerSize() override;

    std::vector<Vtx> VertexList;
};
} // namespace Fast
