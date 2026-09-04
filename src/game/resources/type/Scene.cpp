#include "Scene.h"

namespace Game::Resources {
void* Scene::GetPointer() {
    // Scene is a special type that requries C++ processing. As such, we return nothing.
    return nullptr;
}

size_t Scene::GetPointerSize() {
    return 0;
}
} // namespace Game::Resources
