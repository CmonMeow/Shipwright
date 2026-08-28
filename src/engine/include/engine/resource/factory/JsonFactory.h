#pragma once

#include "engine/resource/Resource.h"
#include "engine/resource/ResourceFactoryBinary.h"

namespace Engine {
class ResourceFactoryBinaryJsonV0 final : public ResourceFactoryBinary {
  public:
    std::shared_ptr<IResource> ReadResource(std::shared_ptr<File> file,
                                            std::shared_ptr<Engine::ResourceInitData> initData) override;
};
}; // namespace Engine
