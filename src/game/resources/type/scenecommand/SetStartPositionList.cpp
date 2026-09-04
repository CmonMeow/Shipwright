#include "SetStartPositionList.h"

namespace Game::Resources {
ActorEntry* SetStartPositionList::GetPointer() {
    return startPositions.data();
}

size_t SetStartPositionList::GetPointerSize() {
    return startPositions.size() * sizeof(ActorEntry);
}
} // namespace Game::Resources
