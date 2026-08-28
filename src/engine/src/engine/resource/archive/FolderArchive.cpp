#include <runtime/log/Log.hpp>
#pragma once

#undef _DLL

#include "engine/resource/archive/FolderArchive.h"

#include "engine/Context.h"
#include "engine/utils/filesystemtools/FileHelper.h"
#include "engine/resource/ResourceManager.h"

namespace Engine {
FolderArchive::FolderArchive(const std::string& archivePath) : Archive(archivePath) {
    mArchiveBasePath = archivePath + "/";
}

Engine::FolderArchive::~FolderArchive() {
    WriteLog("destruct folderarchive: {}", GetPath());
}

bool FolderArchive::Open() {

    auto fileEntries = Directory::ListFiles(mArchiveBasePath);

    for (auto i = 0; i < fileEntries.size(); i++) {
        auto filePath = StringHelper::Split(fileEntries[i], mArchiveBasePath)[1];
        IndexFile(filePath);
    }

    return true;
}

bool FolderArchive::Close() {
    return true;
}

bool FolderArchive::WriteFile(const std::string& filename, const std::vector<uint8_t>& data) {
    Engine::FileHelper::WriteAllBytes(mArchiveBasePath + filename, data);
    return true;
}

std::shared_ptr<File> Engine::FolderArchive::LoadFile(const std::string& filePath) {
    return LoadFileRaw(filePath);
}

std::shared_ptr<File> Engine::FolderArchive::LoadFile(uint64_t hash) {
    const std::string& filePath =
        *Context::GetInstance()->GetResourceManager()->GetArchiveManager()->HashToString(hash);

    return LoadFileRaw(filePath);
}

std::shared_ptr<File> FolderArchive::LoadFileRaw(const std::string& filePath) {
    if (Engine::FileHelper::Exists(mArchiveBasePath + filePath)) {
        auto data = Engine::FileHelper::ReadAllBytes(mArchiveBasePath + filePath);
        auto fileToLoad = std::make_shared<File>();

        fileToLoad->Buffer = std::make_shared<std::vector<char>>(data.size());
        memcpy(fileToLoad->Buffer->data(), data.data(), data.size());

        fileToLoad->IsLoaded = true;

        return fileToLoad;
    } else {
        return nullptr;
    }
}

std::shared_ptr<File> FolderArchive::LoadFileRaw(uint64_t hash) {
    const std::string& filePath =
        *Context::GetInstance()->GetResourceManager()->GetArchiveManager()->HashToString(hash);

    return LoadFileRaw(filePath);
}
} // namespace Engine
