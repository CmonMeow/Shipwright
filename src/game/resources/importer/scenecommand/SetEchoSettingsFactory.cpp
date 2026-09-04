#include "resources/importer/scenecommand/SetEchoSettingsFactory.h"
#include "resources/type/scenecommand/SetEchoSettings.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource> SetEchoSettingsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                      std::shared_ptr<Engine::BinaryReader> reader) {
    auto setEchoSettings = std::make_shared<SetEchoSettings>(initData);

    ReadCommandId(setEchoSettings, reader);

    setEchoSettings->settings.echo = reader->ReadInt8();

    

    return setEchoSettings;
}

} // namespace Game::Resources
