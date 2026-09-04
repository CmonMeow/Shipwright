#include "rendering/resource/factory/TextureFactory.h"
#include "rendering/resource/type/Texture.h"
namespace Engine::Rendering {

std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryTextureV0::ReadResource(std::shared_ptr<Engine::File> file,
                                             std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = reader->ReadUInt32();
    texture->Height = reader->ReadUInt32();
    texture->ImageDataSize = reader->ReadUInt32();
    texture->ImageData = new uint8_t[texture->ImageDataSize];

    reader->Read((char*)texture->ImageData, texture->ImageDataSize);

    return texture;
}

std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryTextureV1::ReadResource(std::shared_ptr<Engine::File> file,
                                             std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    texture->Type = (TextureType)reader->ReadUInt32();
    texture->Width = reader->ReadUInt32();
    texture->Height = reader->ReadUInt32();
    texture->Flags = reader->ReadUInt32();
    texture->HByteScale = reader->ReadFloat();
    texture->VPixelScale = reader->ReadFloat();
    texture->ImageDataSize = reader->ReadUInt32();
    texture->ImageData = new uint8_t[texture->ImageDataSize];

    reader->Read((char*)texture->ImageData, texture->ImageDataSize);

    return texture;
}
} // namespace Engine::Rendering
