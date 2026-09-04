#include "resources/importer/scenecommand/SetRoomBehaviorFactory.h"
#include "resources/type/scenecommand/SetRoomBehavior.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource> SetRoomBehaviorFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                      std::shared_ptr<Engine::BinaryReader> reader) {
    auto setRoomBehavior = std::make_shared<SetRoomBehavior>(initData);

    ReadCommandId(setRoomBehavior, reader);

    setRoomBehavior->roomBehavior.gameplayFlags = reader->ReadInt8();
    setRoomBehavior->roomBehavior.gameplayFlags2 = reader->ReadInt32();

    

    return setRoomBehavior;
}

} // namespace Game::Resources
