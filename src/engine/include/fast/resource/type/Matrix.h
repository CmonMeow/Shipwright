#pragma once

#include "engine/resource/Resource.h"
#include "fast/types.h"

namespace Fast {
class Matrix final : public Engine::Resource<Mtx> {
  public:
    using Resource::Resource;

    Matrix();

    Mtx* GetPointer() override;
    size_t GetPointerSize() override;

    Mtx Matrx;
};
} // namespace Fast
