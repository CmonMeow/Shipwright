#include "port/resource/importer/scenecommand/SetCollisionHeaderFactory.h"
#include "port/resource/type/scenecommand/SetCollisionHeader.h"
#include "runtime/runtime.h"

namespace SOH {
std::shared_ptr<Engine::IResource>
SetCollisionHeaderFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                        std::shared_ptr<Engine::BinaryReader> reader) {
    auto setCollisionHeader = std::make_shared<SetCollisionHeader>(initData);

    ReadCommandId(setCollisionHeader, reader);

    setCollisionHeader->fileName = reader->ReadString();
    setCollisionHeader->collisionHeader = std::static_pointer_cast<CollisionHeader>(
        Engine::Context::GetInstance()->GetResourceManager()->LoadResourceProcess(setCollisionHeader->fileName.c_str()));

    

    return setCollisionHeader;
}

} // namespace SOH
