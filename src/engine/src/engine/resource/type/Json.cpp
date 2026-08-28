#include "engine/resource/type/Json.h"

namespace Engine {
Json::Json() : Resource(std::shared_ptr<ResourceInitData>()) {
}

void* Json::GetPointer() {
    return &Data;
}

size_t Json::GetPointerSize() {
    return DataSize * sizeof(char);
}
} // namespace Engine
