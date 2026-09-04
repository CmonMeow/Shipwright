#pragma once

#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <stdint.h>
#include "engine/resource/ResourceType.h"
#include "engine/utils/binarytools/BinaryReader.h"

namespace Engine {
class Archive;
class ResourceManager;

#define RESOURCE_FORMAT_BINARY 0

struct ResourceInitData {
    ResourceManager* Manager = nullptr;
    std::shared_ptr<Archive> Parent;
    std::string Path;
    Endianness ByteOrder;
    uint32_t Type;
    int32_t ResourceVersion;
    uint64_t Id;
    bool IsCustom;
    uint32_t Format;
};

struct File {
    std::shared_ptr<std::vector<char>> Buffer;
    std::variant<std::shared_ptr<BinaryReader>> Reader;
    bool IsLoaded = false;
};
} // namespace Engine
