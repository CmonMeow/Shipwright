#include "soh/resource/importer/scenecommand/SetCutscenesFactory.h"
#include "soh/resource/type/scenecommand/SetCutscenes.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Ship::IResource> SetCutscenesFactory::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                                                   std::shared_ptr<Ship::BinaryReader> reader) {
    auto setCutscenes = std::make_shared<SetCutscenes>(initData);

    ReadCommandId(setCutscenes, reader);

    // Consume the archive field to keep the scene-command stream aligned, but
    // do not resolve or parse the cutscene resource.
    setCutscenes->fileName = reader->ReadString();

    return setCutscenes;
}

std::shared_ptr<Ship::IResource> SetCutscenesFactoryXML::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                                                      tinyxml2::XMLElement* reader) {
    auto setCutscenes = std::make_shared<SetCutscenes>(initData);

    setCutscenes->cmdId = SceneCommandID::SetCutscenes;

    const char* fileName = reader->Attribute("FileName");
    setCutscenes->fileName = fileName != nullptr ? fileName : "";

    return setCutscenes;
}
} // namespace SOH
