#include "resources/importer/scenecommand/SetSpecialObjectsFactory.h"
#include "resources/type/scenecommand/SetSpecialObjects.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource>
SetSpecialObjectsFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                       std::shared_ptr<Engine::BinaryReader> reader) {
    auto setSpecialObjects = std::make_shared<SetSpecialObjects>(initData);

    ReadCommandId(setSpecialObjects, reader);

    reader->ReadInt8();
    setSpecialObjects->specialObjects.globalObject = reader->ReadInt16();

    

    return setSpecialObjects;
}

} // namespace Game::Resources
