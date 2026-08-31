#pragma once

#include "port/resource/importer/scenecommand/SceneCommandFactory.h"

namespace SOH {
class SetCameraSettingsFactory final : public SceneCommandFactoryBinaryV0 {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                  std::shared_ptr<Engine::BinaryReader> reader) override;
};

} // namespace SOH
