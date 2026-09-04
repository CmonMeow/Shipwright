#include "rendering/resource/type/Matrix.h"

namespace Engine::Rendering {
Matrix::Matrix() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
}

Mtx* Matrix::GetPointer() {
    return &Matrx;
}

size_t Matrix::GetPointerSize() {
    return sizeof(Mtx);
}
} // namespace Engine::Rendering
