#include "CollisionHeader.h"

namespace Game::Resources {
CollisionHeaderData* CollisionHeader::GetPointer() {
    return &collisionHeaderData;
}

size_t CollisionHeader::GetPointerSize() {
    return sizeof(collisionHeaderData);
}
} // namespace Game::Resources