#include "engine/resource/factory/BlobFactory.h"
#include "engine/resource/type/Blob.h"
namespace Engine {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryBlobV0::ReadResource(std::shared_ptr<Engine::File> file,
                                          std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto blob = std::make_shared<Blob>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    uint32_t dataSize = reader->ReadUInt32();

    blob->Data.reserve(dataSize);

    for (uint32_t i = 0; i < dataSize; i++) {
        blob->Data.push_back(reader->ReadUByte());
    }

    return blob;
}
} // namespace Engine
