#include "SetRoomBehavior.h"

namespace Game::Resources {
RoomBehavior* SetRoomBehavior::GetPointer() {
    return &roomBehavior;
}

size_t SetRoomBehavior::GetPointerSize() {
    return sizeof(RoomBehavior);
}
} // namespace Game::Resources
