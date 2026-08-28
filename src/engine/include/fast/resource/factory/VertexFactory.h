#pragma once

#include "engine/resource/Resource.h"
#include "engine/resource/ResourceFactoryBinary.h"
#include "engine/resource/ResourceFactoryXML.h"

namespace Fast {
class ResourceFactoryBinaryVertexV0 final : public Engine::ResourceFactoryBinary {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};

class ResourceFactoryXMLVertexV0 final : public Engine::ResourceFactoryXML {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};
} // namespace Fast
