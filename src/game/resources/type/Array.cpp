#include "Array.h"
#include <rendering/ResourceGbi.h>
namespace Game::Resources {
Array::Array() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
}

void* Array::GetPointer() {
    void* dataPointer = nullptr;
    switch (ArrayType) {
        case ArrayResourceType::Vertex:
            dataPointer = Vertices.data();
            break;
        case ArrayResourceType::Scalar:
        default:
            dataPointer = Scalars.data();
            break;
    }

    return dataPointer;
}

size_t Array::GetPointerSize() {
    size_t typeSize = 0;
    switch (ArrayType) {
        case ArrayResourceType::Vertex:
            typeSize = sizeof(Engine::Rendering::F3DVtx);
            break;
        case ArrayResourceType::Scalar:
        default:
            switch (ArrayScalarType) {
                case ScalarType::ZSCALAR_S16:
                    typeSize = sizeof(int16_t);
                    break;
                case ScalarType::ZSCALAR_U16:
                    typeSize = sizeof(uint16_t);
                    break;
                default:
                    // TODO: Implement the remaining array element types.
                    break;
            }
            break;
    }
    return ArrayCount * typeSize;
}
} // namespace Game::Resources
