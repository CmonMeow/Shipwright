#include "port/resource/importer/scenecommand/SetSkyboxSettingsFactory.h"
#include "port/resource/type/scenecommand/SetSkyboxSettings.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Engine::IResource>
SetSkyboxSettingsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setSkyboxSettings = std::make_shared<SetSkyboxSettings>(initData);

    ReadCommandId(setSkyboxSettings, reader);

    setSkyboxSettings->settings.unk = reader->ReadInt8();
    setSkyboxSettings->settings.skyboxId = reader->ReadInt8();
    setSkyboxSettings->settings.weather = reader->ReadInt8();
    setSkyboxSettings->settings.indoors = reader->ReadInt8();

    

    return setSkyboxSettings;
}

std::shared_ptr<Engine::IResource>
SetSkyboxSettingsFactoryXML::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                          tinyxml2::XMLElement* reader) {
    auto setSkyboxSettings = std::make_shared<SetSkyboxSettings>(initData);

    setSkyboxSettings->cmdId = SceneCommandID::SetSkyboxSettings;

    setSkyboxSettings->settings.unk = reader->IntAttribute("Unknown");
    setSkyboxSettings->settings.skyboxId = reader->IntAttribute("SkyboxId");
    setSkyboxSettings->settings.weather = reader->IntAttribute("Weather");
    setSkyboxSettings->settings.indoors = reader->IntAttribute("Indoors");

    return setSkyboxSettings;
}
} // namespace SOH
