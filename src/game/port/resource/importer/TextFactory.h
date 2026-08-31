#pragma once

#include <engine/resource/Resource.h>
#include <engine/resource/ResourceFactoryBinary.h>

namespace SOH {
class ResourceFactoryBinaryTextV0 final : public Engine::ResourceFactoryBinary {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};

} // namespace SOH
