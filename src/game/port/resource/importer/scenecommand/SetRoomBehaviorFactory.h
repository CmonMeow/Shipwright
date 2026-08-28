#pragma once

#include "port/resource/importer/scenecommand/SceneCommandFactory.h"

namespace SOH {
class SetRoomBehaviorFactory final : public SceneCommandFactoryBinaryV0 {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                  std::shared_ptr<Engine::BinaryReader> reader) override;
};

class SetRoomBehaviorFactoryXML final : public SceneCommandFactoryXMLV0 {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                  tinyxml2::XMLElement* reader) override;
};
} // namespace SOH
