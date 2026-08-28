#pragma once

#include "engine/resource/Resource.h"
#include "engine/resource/ResourceFactoryBinary.h"

namespace Fast {
class ResourceFactoryBinaryTextureV0 final : public Engine::ResourceFactoryBinary {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};

class ResourceFactoryBinaryTextureV1 final : public Engine::ResourceFactoryBinary {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};
} // namespace Fast
