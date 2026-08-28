#include "fast/resource/type/Matrix.h"

namespace Fast {
Matrix::Matrix() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
}

Mtx* Matrix::GetPointer() {
    return &Matrx;
}

size_t Matrix::GetPointerSize() {
    return sizeof(Mtx);
}
} // namespace Fast
