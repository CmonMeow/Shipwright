#include "soh/resource/importer/scenecommand/SetSpecialObjectsFactory.h"
#include "soh/resource/type/scenecommand/SetSpecialObjects.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Ship::IResource>
SetSpecialObjectsFactory::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                       std::shared_ptr<Ship::BinaryReader> reader) {
    auto setSpecialObjects = std::make_shared<SetSpecialObjects>(initData);

    ReadCommandId(setSpecialObjects, reader);

    reader->ReadInt8();
    setSpecialObjects->specialObjects.globalObject = reader->ReadInt16();

    

    return setSpecialObjects;
}

std::shared_ptr<Ship::IResource>
SetSpecialObjectsFactoryXML::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                          tinyxml2::XMLElement* reader) {
    auto setSpecialObjects = std::make_shared<SetSpecialObjects>(initData);

    setSpecialObjects->cmdId = SceneCommandID::SetSpecialObjects;

    setSpecialObjects->specialObjects.globalObject = reader->IntAttribute("GlobalObject");

    return setSpecialObjects;
}
} // namespace SOH
