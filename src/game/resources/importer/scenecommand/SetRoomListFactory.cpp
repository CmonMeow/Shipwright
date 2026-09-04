#include "resources/importer/scenecommand/SetRoomListFactory.h"
#include "resources/type/scenecommand/SetRoomList.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource> SetRoomListFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                  std::shared_ptr<Engine::BinaryReader> reader) {
    auto setRoomList = std::make_shared<SetRoomList>(initData);

    ReadCommandId(setRoomList, reader);

    setRoomList->numRooms = reader->ReadInt32();
    setRoomList->rooms.reserve(setRoomList->numRooms);
    for (uint32_t i = 0; i < setRoomList->numRooms; i++) {
        RomFile room;

        setRoomList->fileNames.push_back(reader->ReadString());

        room.fileName = (char*)setRoomList->fileNames.back().c_str();
        room.vromStart = reader->ReadInt32();
        room.vromEnd = reader->ReadInt32();

        setRoomList->rooms.push_back(room);
    }

    

    return setRoomList;
}

} // namespace Game::Resources
