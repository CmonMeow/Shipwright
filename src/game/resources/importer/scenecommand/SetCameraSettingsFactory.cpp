#include "resources/importer/scenecommand/SetCameraSettingsFactory.h"
#include "resources/type/scenecommand/SetCameraSettings.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource>
SetCameraSettingsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setCameraSettings = std::make_shared<SetCameraSettings>(initData);

    ReadCommandId(setCameraSettings, reader);

    setCameraSettings->settings.cameraMovement = reader->ReadInt8();
    setCameraSettings->settings.worldMapArea = reader->ReadInt32();

    

    return setCameraSettings;
}

} // namespace Game::Resources
