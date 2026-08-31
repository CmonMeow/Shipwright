#pragma once

#include <memory>
#include <engine/resource/Resource.h>
#include <engine/resource/ResourceFactory.h>
#include "port/resource/type/scenecommand/SceneCommand.h"
#include <runtime/bridge/consolevariablebridge.h>

namespace SOH {
class SceneCommandFactoryBinaryV0 {
  public:
    virtual std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                          std::shared_ptr<Engine::BinaryReader> reader) = 0;

  protected:
    void ReadCommandId(std::shared_ptr<ISceneCommand> command, std::shared_ptr<Engine::BinaryReader> reader);
};

} // namespace SOH
