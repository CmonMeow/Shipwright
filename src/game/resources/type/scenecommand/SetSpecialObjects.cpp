#include "SetSpecialObjects.h"

namespace Game::Resources {
SpecialObjects* SetSpecialObjects::GetPointer() {
    return &specialObjects;
}

size_t SetSpecialObjects::GetPointerSize() {
    return sizeof(SpecialObjects);
}
} // namespace Game::Resources
