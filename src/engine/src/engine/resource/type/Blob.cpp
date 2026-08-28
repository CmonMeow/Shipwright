#include "engine/resource/type/Blob.h"

namespace Engine {
Blob::Blob() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
}

void* Blob::GetPointer() {
    return Data.data();
}

size_t Blob::GetPointerSize() {
    return Data.size() * sizeof(uint8_t);
}
} // namespace Engine
