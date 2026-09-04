#include <runtime/log/Log.hpp>
#include "engine/resource/ResourceLoader.h"
#include "engine/resource/ResourceFactory.h"
#include "engine/resource/ResourceManager.h"
#include "engine/resource/Resource.h"
#include "engine/resource/File.h"
#include "engine/utils/binarytools/MemoryStream.h"
#include "engine/utils/binarytools/BinaryReader.h"
#include "engine/resource/factory/JsonFactory.h"
#include "engine/resource/factory/ShaderFactory.h"
#include "nlohmann/json.hpp"

namespace Engine {
ResourceLoader::ResourceLoader(ResourceManager& resources) : mResources(resources) {
    RegisterGlobalResourceFactories();
}

ResourceLoader::~ResourceLoader() {
    WriteLog("destruct resource loader");
}

void ResourceLoader::RegisterGlobalResourceFactories() {
    RegisterResourceFactory(std::make_shared<ResourceFactoryBinaryJsonV0>(), RESOURCE_FORMAT_BINARY, "Json",
                            static_cast<uint32_t>(ResourceType::Json), 0);
    RegisterResourceFactory(std::make_shared<ResourceFactoryBinaryShaderV0>(), RESOURCE_FORMAT_BINARY, "Shader",
                            static_cast<uint32_t>(ResourceType::Shader), 0);
}

bool ResourceLoader::RegisterResourceFactory(std::shared_ptr<ResourceFactory> factory, uint32_t format,
                                             std::string typeName, uint32_t type, uint32_t version) {
    if (mResourceTypes.contains(typeName)) {
        if (mResourceTypes[typeName] != type) {
            WriteLog("Failed to register resource factory: conflicting types for name {}", typeName);
            return false;
        }
    } else {
        mResourceTypes[typeName] = type;
    }

    ResourceFactoryKey key{ .resourceFormat = format, .resourceType = type, .resourceVersion = version };
    if (mFactories.contains(key)) {
        WriteLog("Failed to register resource factory: factory with key {}{}{} already exists", format, type,
                     version);
        return false;
    }
    mFactories[key] = factory;

    return true;
}

std::string ResourceLoader::DecodeASCII(uint32_t value) {
    std::string str(4, '\0'); // Initialize the string with 4 null characters

    str[0] = (value >> 24) & 0xFF;
    str[1] = (value >> 16) & 0xFF;
    str[2] = (value >> 8) & 0xFF;
    str[3] = value & 0xFF;

    return str;
}

std::shared_ptr<ResourceFactory> ResourceLoader::GetFactory(uint32_t format, uint32_t type, uint32_t version) {
    ResourceFactoryKey key{ .resourceFormat = format, .resourceType = type, .resourceVersion = version };
    if (!mFactories.contains(key)) {
        WriteLog("GetFactory failed to find an import factory for resource of type {} as hex: 0x{:X}. Format: {}, "
                     "Version: {}",
                     DecodeASCII(type), type, format, version);
        return nullptr;
    }

    return mFactories[key];
}

std::shared_ptr<ResourceFactory> ResourceLoader::GetFactory(uint32_t format, std::string typeName, uint32_t version) {
    if (!mResourceTypes.contains(typeName)) {
        WriteLog("Could not find resource type for name {}", typeName);
        return nullptr;
    }

    return GetFactory(format, mResourceTypes[typeName], version);
}

std::shared_ptr<ResourceInitData> ResourceLoader::ReadResourceInitDataLegacy(const std::string& filePath,
                                                                             std::shared_ptr<File> fileToLoad) {
    // Runtime resources are binary O2R entries. XML import was an exporter-era
    // compatibility path and is intentionally unsupported.
    if (fileToLoad->Buffer->at(0) == '<') {
        WriteLog("XML resources are unsupported: {}", filePath);
        return nullptr;
    } else {
        auto headerBuffer = std::make_shared<std::vector<char>>(fileToLoad->Buffer->begin(),
                                                                fileToLoad->Buffer->begin() + RESOURCE_HEADER_SIZE);

        if (headerBuffer->size() < RESOURCE_HEADER_SIZE) {
            WriteLog("Failed to parse ResourceInitData, buffer size too small. File: {}. Got {} bytes and "
                         "needed {} bytes.",
                         filePath, headerBuffer->size(), RESOURCE_HEADER_SIZE);
            return nullptr;
        }

        // Factories expect the buffer to not include the header,
        // so we need to remove it from the buffer on the file
        fileToLoad->Buffer = std::make_shared<std::vector<char>>(fileToLoad->Buffer->begin() + RESOURCE_HEADER_SIZE,
                                                                 fileToLoad->Buffer->end());

        // Create a reader for the header buffer
        auto headerStream = std::make_shared<MemoryStream>(headerBuffer);
        auto headerReader = std::make_shared<BinaryReader>(headerStream);
        return ReadResourceInitDataBinary(filePath, headerReader);
    }
}

std::shared_ptr<BinaryReader> ResourceLoader::CreateBinaryReader(std::shared_ptr<File> fileToLoad,
                                                                 std::shared_ptr<ResourceInitData> initData) {
    auto stream = std::make_shared<MemoryStream>(fileToLoad->Buffer);
    auto reader = std::make_shared<BinaryReader>(stream);
    reader->SetEndianness(initData->ByteOrder);
    return reader;
}

std::shared_ptr<ResourceInitData> ResourceLoader::CreateDefaultResourceInitData() {
    auto resourceInitData = std::make_shared<ResourceInitData>();
    resourceInitData->Id = 0xDEADBEEFDEADBEEF;
    resourceInitData->Type = static_cast<uint32_t>(ResourceType::None);
    resourceInitData->ResourceVersion = -1;
    resourceInitData->IsCustom = false;
    resourceInitData->Format = RESOURCE_FORMAT_BINARY;
    resourceInitData->ByteOrder = Endianness::Native;
    return resourceInitData;
}

std::shared_ptr<ResourceInitData> ResourceLoader::ReadResourceInitData(const std::string& filePath,
                                                                       std::shared_ptr<File> metaFileToLoad) {
    auto initData = CreateDefaultResourceInitData();

    // just using metaFileToLoad->Buffer->data() leads to garbage at the end
    // that causes nlohmann to fail parsing, following the pattern used for
    // xml resolves that issue
    auto stream = std::make_shared<MemoryStream>(metaFileToLoad->Buffer);
    auto binaryReader = std::make_shared<BinaryReader>(stream);
    auto parsed = nlohmann::json::parse(binaryReader->ReadCString());

    if (parsed.contains("path")) {
        initData->Path = parsed["path"];
    } else {
        initData->Path = filePath;
    }

    initData->Type = GetResourceType(parsed["type"]);
    initData->ResourceVersion = parsed["version"];

    return initData;
}

std::shared_ptr<IResource> ResourceLoader::LoadResource(std::string filePath, std::shared_ptr<File> fileToLoad,
                                                        std::shared_ptr<ResourceInitData> initData) {
    if (fileToLoad == nullptr) {
        WriteLog("Failed to load resource: File not loaded");
        return nullptr;
    }

    if (initData == nullptr) {
        auto metaFilePath = filePath + ".meta";
        auto metaFileToLoad = mResources.LoadFileProcess(metaFilePath);

        if (metaFileToLoad != nullptr) {
            auto initDataFromMetaFile = ReadResourceInitData(filePath, metaFileToLoad);
            fileToLoad = mResources.LoadFileProcess(initDataFromMetaFile->Path);
            initData = initDataFromMetaFile;
        } else {
            initData = ReadResourceInitDataLegacy(filePath, fileToLoad);
        }
    }

    if (fileToLoad == nullptr) {
        WriteLog("Failed to load file at path {}.", filePath);
        return nullptr;
    }

    initData->Manager = &mResources;
    if (initData->Format != RESOURCE_FORMAT_BINARY) {
        WriteLog("Unsupported resource format {} at path {}", initData->Format, initData->Path);
        return nullptr;
    }
    fileToLoad->Reader = CreateBinaryReader(fileToLoad, initData);

    // initData->Parent = shared_from_this();

    auto factory = GetFactory(initData->Format, initData->Type, initData->ResourceVersion);
    if (factory == nullptr) {
        WriteLog("LoadResource failed to find factory for the resource at path: {}", initData->Path);
        return nullptr;
    }

    return factory->ReadResource(fileToLoad, initData);
}

uint32_t ResourceLoader::GetResourceType(const std::string& type) {
    return mResourceTypes.contains(type) ? mResourceTypes[type] : static_cast<uint32_t>(ResourceType::None);
}

std::shared_ptr<ResourceInitData>
ResourceLoader::ReadResourceInitDataBinary(const std::string& filePath, std::shared_ptr<BinaryReader> headerReader) {
    auto resourceInitData = CreateDefaultResourceInitData();
    resourceInitData->Path = filePath;

    if (headerReader == nullptr) {
        WriteLog("Error reading resource header: no header document for file {}", filePath);
        return resourceInitData;
    }

    // Resource header begin
    // Byte Order
    resourceInitData->ByteOrder = (Endianness)headerReader->ReadInt8();
    headerReader->SetEndianness(resourceInitData->ByteOrder);
    // Is this asset custom?
    resourceInitData->IsCustom = (bool)headerReader->ReadInt8();
    // Unused two bytes
    for (int i = 0; i < 2; i++) {
        headerReader->ReadInt8();
    }
    // The type of the resource
    resourceInitData->Type = headerReader->ReadUInt32();
    // Resource version
    resourceInitData->ResourceVersion = headerReader->ReadUInt32();
    // Unique asset ID
    resourceInitData->Id = headerReader->ReadUInt64();
    // Reserved
    // reader->ReadUInt32();
    // ROM CRC
    // reader->ReadUInt64();
    // ROM Enum
    // reader->ReadUInt32();
    // Reserved for future file format versions...
    // reader->Seek(RESOURCE_HEADER_SIZE, SeekOffsetType::Start);
    // Resource header end

    headerReader->Seek(0, SeekOffsetType::Start);
    return resourceInitData;
}

} // namespace Engine
