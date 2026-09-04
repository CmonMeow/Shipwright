#include <runtime/log/Log.hpp>
#include "resources/type/GameResourceType.h"
#include "resources/importer/SceneFactory.h"
#include "resources/type/Scene.h"
#include "resources/type/scenecommand/SceneCommand.h"
#include "resources/importer/scenecommand/SetLightingSettingsFactory.h"
#include "resources/importer/scenecommand/SetTimeSettingsFactory.h"
#include "resources/importer/scenecommand/SetSkyboxModifierFactory.h"
#include "resources/importer/scenecommand/SetEchoSettingsFactory.h"
#include "resources/importer/scenecommand/SetSoundSettingsFactory.h"
#include "resources/importer/scenecommand/SetSkyboxSettingsFactory.h"
#include "resources/importer/scenecommand/SetRoomBehaviorFactory.h"
#include "resources/importer/scenecommand/SetCameraSettingsFactory.h"
#include "resources/importer/scenecommand/SetRoomListFactory.h"
#include "resources/importer/scenecommand/SetCollisionHeaderFactory.h"
#include "resources/importer/scenecommand/SetEntranceListFactory.h"
#include "resources/importer/scenecommand/SetSpecialObjectsFactory.h"
#include "resources/importer/scenecommand/SetStartPositionListFactory.h"
#include "resources/importer/scenecommand/EndMarkerFactory.h"
#include "resources/importer/scenecommand/SetMeshFactory.h"

namespace Game::Resources {
ResourceFactoryBinarySceneV0::ResourceFactoryBinarySceneV0() {
    sceneCommandFactories[SceneCommandID::SetLightingSettings] = std::make_shared<SetLightingSettingsFactory>();
    sceneCommandFactories[SceneCommandID::SetTimeSettings] = std::make_shared<SetTimeSettingsFactory>();
    sceneCommandFactories[SceneCommandID::SetSkyboxModifier] = std::make_shared<SetSkyboxModifierFactory>();
    sceneCommandFactories[SceneCommandID::SetEchoSettings] = std::make_shared<SetEchoSettingsFactory>();
    sceneCommandFactories[SceneCommandID::SetSoundSettings] = std::make_shared<SetSoundSettingsFactory>();
    sceneCommandFactories[SceneCommandID::SetSkyboxSettings] = std::make_shared<SetSkyboxSettingsFactory>();
    sceneCommandFactories[SceneCommandID::SetRoomBehavior] = std::make_shared<SetRoomBehaviorFactory>();
    sceneCommandFactories[SceneCommandID::SetCameraSettings] = std::make_shared<SetCameraSettingsFactory>();
    sceneCommandFactories[SceneCommandID::SetRoomList] = std::make_shared<SetRoomListFactory>();
    sceneCommandFactories[SceneCommandID::SetCollisionHeader] = std::make_shared<SetCollisionHeaderFactory>();
    sceneCommandFactories[SceneCommandID::SetEntranceList] = std::make_shared<SetEntranceListFactory>();
    sceneCommandFactories[SceneCommandID::SetSpecialObjects] = std::make_shared<SetSpecialObjectsFactory>();
    sceneCommandFactories[SceneCommandID::SetStartPositionList] = std::make_shared<SetStartPositionListFactory>();
    sceneCommandFactories[SceneCommandID::EndMarker] = std::make_shared<EndMarkerFactory>();
    sceneCommandFactories[SceneCommandID::SetMesh] = std::make_shared<SetMeshFactory>();
}

void ResourceFactoryBinarySceneV0::ParseSceneCommands(std::shared_ptr<Scene> scene,
                                                      std::shared_ptr<Engine::BinaryReader> reader) {
    uint32_t commandCount = reader->ReadUInt32();
    scene->commands.reserve(commandCount);

    for (uint32_t i = 0; i < commandCount; i++) {
        scene->commands.push_back(ParseSceneCommand(scene, reader, i));
    }
}

std::shared_ptr<ISceneCommand>
ResourceFactoryBinarySceneV0::ParseSceneCommand(std::shared_ptr<Scene> scene,
                                                std::shared_ptr<Engine::BinaryReader> reader, uint32_t index) {
    SceneCommandID cmdID = (SceneCommandID)reader->ReadInt32();

    reader->Seek(-sizeof(int32_t), Engine::SeekOffsetType::Current);

    std::shared_ptr<ISceneCommand> result = nullptr;
    auto commandFactory = ResourceFactoryBinarySceneV0::sceneCommandFactories[cmdID];

    if (commandFactory != nullptr) {
        auto initData = std::make_shared<Engine::ResourceInitData>();
        initData->Manager = scene->GetInitData()->Manager;
        initData->Id = scene->GetInitData()->Id;
        initData->Type = static_cast<uint32_t>(Game::Resources::ResourceType::SceneCommand);
        initData->Path = scene->GetInitData()->Path + "/SceneCommand" + std::to_string(index);
        initData->ResourceVersion = scene->GetInitData()->ResourceVersion;
        result = std::static_pointer_cast<ISceneCommand>(commandFactory->ReadResource(initData, reader));
        // Cache the resource?
    }

    if (result == nullptr) {
        WriteLog("Failed to load scene command of type {} in scene {}", (uint32_t)cmdID,
                     scene->GetInitData()->Path);
    }

    return result;
}

std::shared_ptr<Engine::IResource>
ResourceFactoryBinarySceneV0::ReadResource(std::shared_ptr<Engine::File> file,
                                           std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto scene = std::make_shared<Scene>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    ParseSceneCommands(scene, reader);

    return scene;
};

} // namespace Game::Resources
