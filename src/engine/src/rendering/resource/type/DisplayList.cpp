#include "rendering/resource/type/DisplayList.h"
#include <memory>

namespace Engine::Rendering {
DisplayList::DisplayList() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
}

DisplayList::~DisplayList() {
    for (char* string : Strings) {
        free(string);
    }
}

Gfx* DisplayList::GetPointer() {
    return (Gfx*)Instructions.data();
}

size_t DisplayList::GetPointerSize() {
    return Instructions.size() * sizeof(Gfx);
}
} // namespace Engine::Rendering
