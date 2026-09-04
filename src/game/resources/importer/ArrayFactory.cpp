#include "resources/importer/ArrayFactory.h"
#include "resources/type/Array.h"
#include <rendering/ResourceGbi.h>

namespace Game::Resources {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryArrayV0::ReadResource(std::shared_ptr<Engine::File> file,
                                           std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto array = std::make_shared<Array>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    array->ArrayType = (ArrayResourceType)reader->ReadUInt32();
    array->ArrayCount = reader->ReadUInt32();

    for (uint32_t i = 0; i < array->ArrayCount; i++) {
        if (array->ArrayType == ArrayResourceType::Vertex) {
            // TODO: Implement vertex arrays as a vertex resource.
            Engine::Rendering::F3DVtx data;
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
            array->Vertices.push_back(data);
        } else {
            array->ArrayScalarType = (ScalarType)reader->ReadUInt32();

            int iter = 1;

            if (array->ArrayType == ArrayResourceType::Vector) {
                iter = reader->ReadUInt32();
            }

            for (int k = 0; k < iter; k++) {
                ScalarData data;

                switch (array->ArrayScalarType) {
                    case ScalarType::ZSCALAR_S16:
                        data.signed16 = reader->ReadInt16();
                        break;
                    case ScalarType::ZSCALAR_U16:
                        data.unsigned16 = reader->ReadUInt16();
                        break;
                    default:
                        // TODO: Implement the remaining array element types.
                        break;
                }

                array->Scalars.push_back(data);
            }
        }
    }

    return array;
}
} // namespace Game::Resources
