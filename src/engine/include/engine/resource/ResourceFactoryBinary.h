#pragma once

#include <engine/resource/ResourceFactory.h>

namespace Engine {
class ResourceFactoryBinary : public ResourceFactory {
  protected:
    bool FileHasValidFormatAndReader(std::shared_ptr<File> file,
                                     std::shared_ptr<Engine::ResourceInitData> initData) override;
};
} // namespace Engine
