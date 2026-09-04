#include "PlayerAnimation.h"
#include <runtime/libultra/gbi.h>

namespace Game::Resources {
int16_t* PlayerAnimation::GetPointer() {
    return limbRotData.data();
}

size_t PlayerAnimation::GetPointerSize() {
    return limbRotData.size() * sizeof(int16_t);
}
} // namespace Game::Resources
