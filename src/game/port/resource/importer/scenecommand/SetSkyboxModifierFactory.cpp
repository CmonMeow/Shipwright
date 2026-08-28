#include "port/resource/importer/scenecommand/SetSkyboxModifierFactory.h"
#include "port/resource/type/scenecommand/SetSkyboxModifier.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Engine::IResource>
SetSkyboxModifierFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setSkyboxModifier = std::make_shared<SetSkyboxModifier>(initData);

    ReadCommandId(setSkyboxModifier, reader);

    setSkyboxModifier->modifier.skyboxDisabled = reader->ReadInt8();
    setSkyboxModifier->modifier.sunMoonDisabled = reader->ReadInt8();

    

    return setSkyboxModifier;
}

std::shared_ptr<Engine::IResource>
SetSkyboxModifierFactoryXML::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                          tinyxml2::XMLElement* reader) {
    auto setSkyboxModifier = std::make_shared<SetSkyboxModifier>(initData);

    setSkyboxModifier->cmdId = SceneCommandID::SetSkyboxModifier;

    setSkyboxModifier->modifier.skyboxDisabled = reader->IntAttribute("SkyboxDisabled");
    setSkyboxModifier->modifier.sunMoonDisabled = reader->IntAttribute("SunMoonDisabled");

    return setSkyboxModifier;
}
} // namespace SOH
