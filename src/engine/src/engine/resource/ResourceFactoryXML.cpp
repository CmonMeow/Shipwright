#include <runtime/log/Log.hpp>
#include "engine/resource/ResourceFactoryXML.h"
namespace Engine {
bool ResourceFactoryXML::FileHasValidFormatAndReader(std::shared_ptr<File> file,
                                                     std::shared_ptr<Engine::ResourceInitData> initData) {
    if (initData->Format != RESOURCE_FORMAT_XML) {
        WriteLog("resource file format does not match factory format.");
        return false;
    }

    if (!std::holds_alternative<std::shared_ptr<tinyxml2::XMLDocument>>(file->Reader)) {
        WriteLog("Failed to load resource: File has no XML document ({} - {})", initData->Type, initData->Path);
        return false;
    }

    return true;
};
} // namespace Engine
