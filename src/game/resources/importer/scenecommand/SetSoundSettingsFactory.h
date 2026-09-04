#pragma once

#include "resources/importer/scenecommand/SceneCommandFactory.h"

namespace Game::Resources {
class SetSoundSettingsFactory final : public SceneCommandFactoryBinaryV0 {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                  std::shared_ptr<Engine::BinaryReader> reader) override;
};

} // namespace Game::Resources
