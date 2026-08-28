#include "port/resource/importer/scenecommand/SetCameraSettingsFactory.h"
#include "port/resource/type/scenecommand/SetCameraSettings.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Engine::IResource>
SetCameraSettingsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setCameraSettings = std::make_shared<SetCameraSettings>(initData);

    ReadCommandId(setCameraSettings, reader);

    setCameraSettings->settings.cameraMovement = reader->ReadInt8();
    setCameraSettings->settings.worldMapArea = reader->ReadInt32();

    

    return setCameraSettings;
}

std::shared_ptr<Engine::IResource>
SetCameraSettingsFactoryXML::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                          tinyxml2::XMLElement* reader) {
    auto setCameraSettings = std::make_shared<SetCameraSettings>(initData);

    setCameraSettings->cmdId = SceneCommandID::SetCameraSettings;

    setCameraSettings->settings.cameraMovement = reader->IntAttribute("CameraMovement");
    setCameraSettings->settings.worldMapArea = reader->IntAttribute("WorldMapArea");

    return setCameraSettings;
}
} // namespace SOH
