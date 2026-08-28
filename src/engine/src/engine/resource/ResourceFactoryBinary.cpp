#include <runtime/log/Log.hpp>
#include "engine/resource/ResourceFactoryBinary.h"
#include <variant>
namespace Engine {
bool ResourceFactoryBinary::FileHasValidFormatAndReader(std::shared_ptr<File> file,
                                                        std::shared_ptr<Engine::ResourceInitData> initData) {
    if (initData->Format != RESOURCE_FORMAT_BINARY) {
        WriteLog("resource file format does not match factory format.");
        return false;
    }

    if (!std::holds_alternative<std::shared_ptr<BinaryReader>>(file->Reader)) {
        WriteLog("Failed to load resource: File has Reader ({} - {})", initData->Type, initData->Path);
        return false;
    }

    return true;
};
} // namespace Engine
