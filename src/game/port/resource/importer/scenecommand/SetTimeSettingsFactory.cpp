#include "port/resource/importer/scenecommand/SetTimeSettingsFactory.h"
#include "port/resource/type/scenecommand/SetTimeSettings.h"

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

} // namespace SOH
