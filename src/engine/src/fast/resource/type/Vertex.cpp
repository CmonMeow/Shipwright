#include "fast/resource/type/Vertex.h"
#include "runtime/libultra/gbi.h"

namespace Fast {
Vertex::Vertex() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
}

Vtx* Vertex::GetPointer() {
    return VertexList.data();
}

size_t Vertex::GetPointerSize() {
    return VertexList.size() * sizeof(Vtx);
}
} // namespace Fast
