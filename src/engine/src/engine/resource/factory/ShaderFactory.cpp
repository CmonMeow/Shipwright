#include "engine/resource/factory/ShaderFactory.h"
namespace Engine {
std::shared_ptr<IResource>
ResourceFactoryBinaryShaderV0::ReadResource(std::shared_ptr<File> file,
                                            std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto shader = std::make_shared<Shader>(initData);
    auto reader = std::get<std::shared_ptr<BinaryReader>>(file->Reader);

    shader->Data = reader->ReadCString();
    return shader;
}
} // namespace Engine
