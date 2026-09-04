#include "SetRoomList.h"

namespace Game::Resources {
RomFile* SetRoomList::GetPointer() {
    return rooms.data();
}

size_t SetRoomList::GetPointerSize() {
    return rooms.size() * sizeof(RomFile);
}
} // namespace Game::Resources
