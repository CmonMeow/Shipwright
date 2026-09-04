#include "rendering/resource/type/Vertex.h"
#include "runtime/libultra/gbi.h"

namespace Engine::Rendering {
Vertex::Vertex() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
}

Vtx* Vertex::GetPointer() {
    return VertexList.data();
}

size_t Vertex::GetPointerSize() {
    return VertexList.size() * sizeof(Vtx);
}
} // namespace Engine::Rendering
