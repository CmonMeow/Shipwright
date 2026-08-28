#include "port/resource/importer/scenecommand/SetSoundSettingsFactory.h"
#include "port/resource/type/scenecommand/SetSoundSettings.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Engine::IResource> SetSoundSettingsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setSoundSettings = std::make_shared<SetSoundSettings>(initData);

    ReadCommandId(setSoundSettings, reader);

    setSoundSettings->settings.reverb = reader->ReadInt8();
    setSoundSettings->settings.natureAmbienceId = reader->ReadInt8();
    setSoundSettings->settings.seqId = reader->ReadInt8();

    

    return setSoundSettings;
}

std::shared_ptr<Engine::IResource>
SetSoundSettingsFactoryXML::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                         tinyxml2::XMLElement* reader) {
    auto setSoundSettings = std::make_shared<SetSoundSettings>(initData);

    setSoundSettings->cmdId = SceneCommandID::SetSoundSettings;

    setSoundSettings->settings.reverb = reader->IntAttribute("Reverb");
    setSoundSettings->settings.natureAmbienceId = reader->IntAttribute("NatureAmbienceId");
    setSoundSettings->settings.seqId = reader->IntAttribute("SeqId");

    return setSoundSettings;
}
} // namespace SOH
