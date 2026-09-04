#pragma once

#include <vector>
#include "engine/resource/Resource.h"
#include "rendering/ucodehandlers.h"
#include <runtime/libultra/gbi.h>

namespace Engine::Rendering {
class DisplayList final : public Engine::Resource<Gfx> {
  public:
    using Resource::Resource;

    DisplayList();
    ~DisplayList();

    Gfx* GetPointer() override;
    size_t GetPointerSize() override;

    UcodeHandlers UCode;
    std::vector<Gfx> Instructions;
    std::vector<char*> Strings;
};
} // namespace Engine::Rendering
