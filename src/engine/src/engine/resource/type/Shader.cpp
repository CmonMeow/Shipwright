#include "engine/resource/type/Shader.h"

namespace Engine {
Shader::Shader() : Resource(std::shared_ptr<ResourceInitData>()) {
}

void* Shader::GetPointer() {
    return &Data;
}

size_t Shader::GetPointerSize() {
    return Data.size();
}
} // namespace Engine
