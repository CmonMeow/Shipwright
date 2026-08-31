#pragma once

#include "port/resource/type/Scene.h"
#include "port/resource/type/scenecommand/SceneCommand.h"
#include "port/resource/importer/scenecommand/SceneCommandFactory.h"
#include <engine/resource/Resource.h>
#include <engine/resource/ResourceFactoryBinary.h>

namespace SOH {
class ResourceFactoryBinarySceneV0 final : public Engine::ResourceFactoryBinary {
  public:
    ResourceFactoryBinarySceneV0();

    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
    void ParseSceneCommands(std::shared_ptr<Scene> scene, std::shared_ptr<Engine::BinaryReader> reader);

    // Doing something very similar to what we do on the ResourceLoader.
    // Eventually, scene commands should be moved up to the ResourceLoader as well.
    // They can not right now because the exporter does not give them a proper resource type enum value,
    // and the exporter does not export the commands with a proper OTR header.
    static inline std::unordered_map<SceneCommandID, std::shared_ptr<SceneCommandFactoryBinaryV0>>
        sceneCommandFactories;

  protected:
    std::shared_ptr<ISceneCommand> ParseSceneCommand(std::shared_ptr<Scene> scene,
                                                     std::shared_ptr<Engine::BinaryReader> reader, uint32_t index);
};

} // namespace SOH
