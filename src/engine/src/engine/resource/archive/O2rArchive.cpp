#include <runtime/log/Log.hpp>
#include "engine/resource/archive/O2rArchive.h"

namespace Engine {
O2rArchive::O2rArchive(const std::string& archivePath) : Archive(archivePath) {
}

O2rArchive::~O2rArchive() {
    WriteLog("destruct o2rarchive: {}", GetPath());
    Close();
}

std::shared_ptr<File> O2rArchive::LoadFile(const std::string& filePath) {
    const std::lock_guard<std::mutex> lock(mZipMutex);

    if (mZipArchive == nullptr) {
        WriteLog("Failed to open file {} from zip archive {}. Archive not open.", filePath, GetPath());
        return nullptr;
    }

    auto zipEntryIndex = zip_name_locate(mZipArchive, filePath.c_str(), 0);
    if (zipEntryIndex < 0) {
        WriteLog("Failed to find file {} in zip archive  {}.", filePath, GetPath());
        return nullptr;
    }

    struct zip_stat zipEntryStat;
    zip_stat_init(&zipEntryStat);
    if (zip_stat_index(mZipArchive, zipEntryIndex, 0, &zipEntryStat) != 0) {
        WriteLog("Failed to get entry information for file {} in zip archive  {}.", filePath, GetPath());
        return nullptr;
    }

    // Filesize 0, no logging needed
    if (zipEntryStat.size == 0) {
        WriteLog("Failed to load file {}; filesize 0", filePath, GetPath());
        return nullptr;
    }

    struct zip_file* zipEntryFile = zip_fopen_index(mZipArchive, zipEntryIndex, 0);
    if (!zipEntryFile) {
        WriteLog("Failed to open file {} in zip archive  {}.", filePath, GetPath());
        return nullptr;
    }

    auto fileToLoad = std::make_shared<File>();
    fileToLoad->Buffer = std::make_shared<std::vector<char>>(zipEntryStat.size);

    zip_uint64_t bytesRead = 0;
    while (bytesRead < zipEntryStat.size) {
        const zip_int64_t result =
            zip_fread(zipEntryFile, fileToLoad->Buffer->data() + bytesRead, zipEntryStat.size - bytesRead);
        if (result <= 0) {
            WriteLog("Error reading file {} in zip archive {}: got {} of {} bytes.", filePath, GetPath(), bytesRead,
                     zipEntryStat.size);
            zip_fclose(zipEntryFile);
            return nullptr;
        }
        bytesRead += static_cast<zip_uint64_t>(result);
    }

    if (zip_fclose(zipEntryFile) != 0) {
        WriteLog("Error closing file {} in zip archive  {}.", filePath, GetPath());
    }

    fileToLoad->IsLoaded = true;

    return fileToLoad;
}

bool O2rArchive::Open() {
    const std::lock_guard<std::mutex> lock(mZipMutex);

    mZipArchive = zip_open(GetPath().c_str(), ZIP_RDONLY, nullptr);
    if (mZipArchive == nullptr) {
        WriteLog("Failed to load zip file \"{}\"", GetPath());
        return false;
    }

    auto zipNumEntries = zip_get_num_entries(mZipArchive, 0);
    for (auto i = 0; i < zipNumEntries; i++) {
        auto zipEntryName = zip_get_name(mZipArchive, i, 0);

        // It is possible for directories to have entries in a zip
        // file, we don't want those indexed as files in the archive
        if (zipEntryName[strlen(zipEntryName) - 1] == '/') {
            continue;
        }

        IndexFile(zipEntryName);
    }

    return true;
}

bool O2rArchive::Close() {
    const std::lock_guard<std::mutex> lock(mZipMutex);

    if (mZipArchive == nullptr) {
        WriteLog("Cannot close zip file. Zip file not loaded. \"{}\"", GetPath());
        return false;
    }

    if (zip_close(mZipArchive) == -1) {
        WriteLog("Failed to close zip file \"{}\"", GetPath());
        return false;
    }

    mZipArchive = nullptr;
    return true;
}

} // namespace Engine
