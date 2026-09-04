#include "resources/importer/scenecommand/SetSkyboxModifierFactory.h"
#include "resources/type/scenecommand/SetSkyboxModifier.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource>
SetSkyboxModifierFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setSkyboxModifier = std::make_shared<SetSkyboxModifier>(initData);

    ReadCommandId(setSkyboxModifier, reader);

    setSkyboxModifier->modifier.skyboxDisabled = reader->ReadInt8();
    setSkyboxModifier->modifier.sunMoonDisabled = reader->ReadInt8();

    

    return setSkyboxModifier;
}

} // namespace Game::Resources
