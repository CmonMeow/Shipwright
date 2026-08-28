#include <runtime/log/Log.hpp>
#include <tinyxml2.h>
#include "port/resource/type/SohResourceType.h"
#include "port/resource/importer/SceneFactory.h"
#include "port/resource/type/Scene.h"
#include "port/resource/type/scenecommand/SceneCommand.h"
#include "port/resource/importer/scenecommand/SetLightingSettingsFactory.h"
#include "port/resource/importer/scenecommand/SetTimeSettingsFactory.h"
#include "port/resource/importer/scenecommand/SetSkyboxModifierFactory.h"
#include "port/resource/importer/scenecommand/SetEchoSettingsFactory.h"
#include "port/resource/importer/scenecommand/SetSoundSettingsFactory.h"
#include "port/resource/importer/scenecommand/SetSkyboxSettingsFactory.h"
#include "port/resource/importer/scenecommand/SetRoomBehaviorFactory.h"
#include "port/resource/importer/scenecommand/SetCameraSettingsFactory.h"
#include "port/resource/importer/scenecommand/SetRoomListFactory.h"
#include "port/resource/importer/scenecommand/SetCollisionHeaderFactory.h"
#include "port/resource/importer/scenecommand/SetEntranceListFactory.h"
#include "port/resource/importer/scenecommand/SetSpecialObjectsFactory.h"
#include "port/resource/importer/scenecommand/SetStartPositionListFactory.h"
#include "port/resource/importer/scenecommand/EndMarkerFactory.h"
#include "port/resource/importer/scenecommand/SetMeshFactory.h"

namespace SOH {
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
        initData->Id = scene->GetInitData()->Id;
        initData->Type = static_cast<uint32_t>(SOH::ResourceType::SOH_SceneCommand);
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

ResourceFactoryXMLSceneV0::ResourceFactoryXMLSceneV0() {
    sceneCommandFactories[SceneCommandID::SetLightingSettings] = std::make_shared<SetLightingSettingsFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetTimeSettings] = std::make_shared<SetTimeSettingsFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetSkyboxModifier] = std::make_shared<SetSkyboxModifierFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetEchoSettings] = std::make_shared<SetEchoSettingsFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetSoundSettings] = std::make_shared<SetSoundSettingsFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetSkyboxSettings] = std::make_shared<SetSkyboxSettingsFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetRoomBehavior] = std::make_shared<SetRoomBehaviorFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetCameraSettings] = std::make_shared<SetCameraSettingsFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetRoomList] = std::make_shared<SetRoomListFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetCollisionHeader] = std::make_shared<SetCollisionHeaderFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetEntranceList] = std::make_shared<SetEntranceListFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetSpecialObjects] = std::make_shared<SetSpecialObjectsFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetStartPositionList] = std::make_shared<SetStartPositionListFactoryXML>();
    sceneCommandFactories[SceneCommandID::EndMarker] = std::make_shared<EndMarkerFactoryXML>();
    sceneCommandFactories[SceneCommandID::SetMesh] = std::make_shared<SetMeshFactoryXML>();
}

SceneCommandID GetCommandID(const std::string& commandName) {
    static const std::pair<const char*, SceneCommandID> commandNames[] = {
        { "SetStartPositionList", SceneCommandID::SetStartPositionList },
        { "SetCollisionHeader", SceneCommandID::SetCollisionHeader },
        { "SetRoomList", SceneCommandID::SetRoomList },
        { "SetEntranceList", SceneCommandID::SetEntranceList },
        { "SetSpecialObjects", SceneCommandID::SetSpecialObjects },
        { "SetRoomBehavior", SceneCommandID::SetRoomBehavior },
        { "SetMesh", SceneCommandID::SetMesh },
        { "SetLightingSettings", SceneCommandID::SetLightingSettings },
        { "SetTimeSettings", SceneCommandID::SetTimeSettings },
        { "SetSkyboxSettings", SceneCommandID::SetSkyboxSettings },
        { "SetSkyboxModifier", SceneCommandID::SetSkyboxModifier },
        { "EndMarker", SceneCommandID::EndMarker },
        { "SetSoundSettings", SceneCommandID::SetSoundSettings },
        { "SetEchoSettings", SceneCommandID::SetEchoSettings },
        { "SetCameraSettings", SceneCommandID::SetCameraSettings },
    };

    for (const auto& command : commandNames) {
        if (command.first == commandName) {
            return command.second;
        }
    }

    return SceneCommandID::Error;
}

void ResourceFactoryXMLSceneV0::ParseSceneCommands(std::shared_ptr<Scene> scene,
                                                   std::shared_ptr<tinyxml2::XMLDocument> reader) {
    auto child = reader->RootElement()->FirstChildElement();

    int i = 0;

    while (child != nullptr) {
        scene->commands.push_back(ParseSceneCommand(scene, child, i));

        child = child->NextSiblingElement();
        i += 1;
    }
}

std::shared_ptr<ISceneCommand> ResourceFactoryXMLSceneV0::ParseSceneCommand(std::shared_ptr<Scene> scene,
                                                                            tinyxml2::XMLElement* child,
                                                                            uint32_t index) {
    std::string commandName = child->Name();
    SceneCommandID cmdID = GetCommandID(commandName);

    if (cmdID == SceneCommandID::Error) {
        WriteLog("Failed to load scene command with name {} in scene {}", commandName, scene->GetInitData()->Path);
        return nullptr;
    }

    std::shared_ptr<ISceneCommand> result = nullptr;
    auto commandFactory = ResourceFactoryXMLSceneV0::sceneCommandFactories[cmdID];

    if (commandFactory != nullptr) {
        auto initData = std::make_shared<Engine::ResourceInitData>();
        initData->Id = scene->GetInitData()->Id;
        initData->Type = static_cast<uint32_t>(ResourceType::SOH_SceneCommand);
        initData->Path = scene->GetInitData()->Path + "/SceneCommand" + std::to_string(index);
        initData->ResourceVersion = scene->GetInitData()->ResourceVersion;
        result = std::static_pointer_cast<ISceneCommand>(commandFactory->ReadResource(initData, child));
        // Cache the resource?
    }

    if (result == nullptr) {
        WriteLog("Failed to load scene command of type {} in scene {}", (uint32_t)cmdID,
                     scene->GetInitData()->Path);
    }

    return result;
}

std::shared_ptr<Engine::IResource>
ResourceFactoryXMLSceneV0::ReadResource(std::shared_ptr<Engine::File> file,
                                        std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto scene = std::make_shared<Scene>(initData);
    auto reader = std::get<std::shared_ptr<tinyxml2::XMLDocument>>(file->Reader);

    ParseSceneCommands(scene, reader);

    return scene;
};
} // namespace SOH
