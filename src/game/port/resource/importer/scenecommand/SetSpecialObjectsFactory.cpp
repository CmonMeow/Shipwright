#include "port/resource/importer/scenecommand/SetSpecialObjectsFactory.h"
#include "port/resource/type/scenecommand/SetSpecialObjects.h"

namespace SOH {
std::shared_ptr<Engine::IResource>
SetSpecialObjectsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setSpecialObjects = std::make_shared<SetSpecialObjects>(initData);

    ReadCommandId(setSpecialObjects, reader);

    reader->ReadInt8();
    setSpecialObjects->specialObjects.globalObject = reader->ReadInt16();

    

    return setSpecialObjects;
}

} // namespace SOH
