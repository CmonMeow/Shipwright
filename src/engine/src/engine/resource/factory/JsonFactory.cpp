#include "engine/resource/factory/JsonFactory.h"
#include "engine/resource/type/Json.h"
namespace Engine {
std::shared_ptr<IResource> ResourceFactoryBinaryJsonV0::ReadResource(std::shared_ptr<File> file,
                                                                     std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto json = std::make_shared<Json>(initData);
    auto reader = std::get<std::shared_ptr<BinaryReader>>(file->Reader);

    json->DataSize = file->Buffer->size();
    json->Data = nlohmann::json::parse(reader->ReadCString(), nullptr, true, true);

    return json;
}
} // namespace Engine
