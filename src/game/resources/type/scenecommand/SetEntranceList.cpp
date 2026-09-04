#include "SetEntranceList.h"

namespace Game::Resources {
EntranceEntry* SetEntranceList::GetPointer() {
    return entrances.data();
}

size_t SetEntranceList::GetPointerSize() {
    return entrances.size() * sizeof(EntranceEntry);
}
} // namespace Game::Resources
