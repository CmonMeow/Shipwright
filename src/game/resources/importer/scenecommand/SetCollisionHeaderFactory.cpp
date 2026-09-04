#include <engine/resource/ResourceManager.h>
#include "resources/importer/scenecommand/SetCollisionHeaderFactory.h"
#include "resources/type/scenecommand/SetCollisionHeader.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource>
SetCollisionHeaderFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                        std::shared_ptr<Engine::BinaryReader> reader) {
    auto setCollisionHeader = std::make_shared<SetCollisionHeader>(initData);

    ReadCommandId(setCollisionHeader, reader);

    setCollisionHeader->fileName = reader->ReadString();
    setCollisionHeader->collisionHeader = std::static_pointer_cast<CollisionHeader>(
        initData->Manager->LoadResourceProcess(setCollisionHeader->fileName.c_str()));

    return setCollisionHeader;
}

} // namespace Game::Resources
