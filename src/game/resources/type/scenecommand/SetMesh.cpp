#include "SetMesh.h"

namespace Game::Resources {
MeshHeader* SetMesh::GetPointer() {
    return &meshHeader;
}

size_t SetMesh::GetPointerSize() {
    return sizeof(MeshHeader);
}
} // namespace Game::Resources
