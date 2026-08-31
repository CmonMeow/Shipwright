#include "fast/resource/factory/VertexFactory.h"
#include "fast/resource/type/Vertex.h"
#include "runtime/libultra/gbi.h"

namespace Fast {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryVertexV0::ReadResource(std::shared_ptr<Engine::File> file,
                                            std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto vertex = std::make_shared<Vertex>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    uint32_t count = reader->ReadUInt32();
    vertex->VertexList.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        Vtx data;
        data.v.ob[0] = reader->ReadInt16();
        data.v.ob[1] = reader->ReadInt16();
        data.v.ob[2] = reader->ReadInt16();
        data.v.flag = reader->ReadUInt16();
        data.v.tc[0] = reader->ReadInt16();
        data.v.tc[1] = reader->ReadInt16();
        data.v.cn[0] = reader->ReadUByte();
        data.v.cn[1] = reader->ReadUByte();
        data.v.cn[2] = reader->ReadUByte();
        data.v.cn[3] = reader->ReadUByte();
        vertex->VertexList.push_back(data);
    }

    return vertex;
}

} // namespace Fast
