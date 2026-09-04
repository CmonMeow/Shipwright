#include "EndMarker.h"

namespace Game::Resources {
Marker* EndMarker::GetPointer() {
    return &endMarker;
}

size_t EndMarker::GetPointerSize() {
    return sizeof(Marker);
}
} // namespace Game::Resources
