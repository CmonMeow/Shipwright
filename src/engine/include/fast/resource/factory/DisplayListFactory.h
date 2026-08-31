#pragma once

#include "engine/resource/Resource.h"
#include "engine/resource/ResourceFactoryBinary.h"

namespace Fast {
class ResourceFactoryDisplayList {
  protected:
    uint32_t GetCombineLERPValue(const char* valStr);
};

class ResourceFactoryBinaryDisplayListV0 final : public ResourceFactoryDisplayList, public Engine::ResourceFactoryBinary {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};

} // namespace Fast
