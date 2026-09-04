#include "SetSkyboxModifier.h"

namespace Game::Resources {
SkyboxModifier* SetSkyboxModifier::GetPointer() {
    return &modifier;
}

size_t SetSkyboxModifier::GetPointerSize() {
    return sizeof(SkyboxModifier);
}
} // namespace Game::Resources
