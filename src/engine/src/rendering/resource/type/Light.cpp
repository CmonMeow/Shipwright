#include "rendering/resource/type/Light.h"

namespace Engine::Rendering {
LightEntry* Light::GetPointer() {
    return &mLight;
}

size_t Light::GetPointerSize() {
    return sizeof(mLight);
}
} // namespace Engine::Rendering
