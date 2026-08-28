#include "port/resource/importer/scenecommand/SetTimeSettingsFactory.h"
#include "port/resource/type/scenecommand/SetTimeSettings.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Engine::IResource> SetTimeSettingsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                      std::shared_ptr<Engine::BinaryReader> reader) {
    auto setTimeSettings = std::make_shared<SetTimeSettings>(initData);

    ReadCommandId(setTimeSettings, reader);

    setTimeSettings->settings.hour = reader->ReadInt8();
    setTimeSettings->settings.minute = reader->ReadInt8();
    setTimeSettings->settings.timeIncrement = reader->ReadInt8();

    

    return setTimeSettings;
}

std::shared_ptr<Engine::IResource>
SetTimeSettingsFactoryXML::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                        tinyxml2::XMLElement* reader) {
    auto setTimeSettings = std::make_shared<SetTimeSettings>(initData);

    setTimeSettings->cmdId = SceneCommandID::SetTimeSettings;

    setTimeSettings->settings.hour = reader->IntAttribute("Hour");
    setTimeSettings->settings.minute = reader->IntAttribute("Minute");
    setTimeSettings->settings.timeIncrement = reader->IntAttribute("TimeIncrement");

    return setTimeSettings;
}
} // namespace SOH
