#pragma once

#include <engine/resource/Resource.h>
#include <engine/resource/ResourceFactoryBinary.h>
#include <engine/resource/ResourceFactoryXML.h>

namespace SOH {
class ResourceFactoryBinaryCollisionHeaderV0 final : public Engine::ResourceFactoryBinary {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};

class ResourceFactoryXMLCollisionHeaderV0 final : public Engine::ResourceFactoryXML {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};
} // namespace SOH
