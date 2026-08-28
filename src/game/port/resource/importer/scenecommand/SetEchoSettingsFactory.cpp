#include "port/resource/importer/scenecommand/SetEchoSettingsFactory.h"
#include "port/resource/type/scenecommand/SetEchoSettings.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Engine::IResource> SetEchoSettingsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                      std::shared_ptr<Engine::BinaryReader> reader) {
    auto setEchoSettings = std::make_shared<SetEchoSettings>(initData);

    ReadCommandId(setEchoSettings, reader);

    setEchoSettings->settings.echo = reader->ReadInt8();

    

    return setEchoSettings;
}

std::shared_ptr<Engine::IResource>
SetEchoSettingsFactoryXML::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                        tinyxml2::XMLElement* reader) {
    auto setEchoSettings = std::make_shared<SetEchoSettings>(initData);

    setEchoSettings->cmdId = SceneCommandID::SetEchoSettings;

    setEchoSettings->settings.echo = reader->IntAttribute("Echo");

    return setEchoSettings;
}
} // namespace SOH
