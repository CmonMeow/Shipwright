#pragma once

#undef _DLL

#include <mutex>
#include <stdint.h>
#include <string>

#include "zip.h"

#include "engine/resource/File.h"
#include "engine/resource/Resource.h"
#include "engine/resource/archive/Archive.h"

namespace Engine {
struct File;

class O2rArchive final : virtual public Archive {
  public:
    O2rArchive(const std::string& archivePath);
    ~O2rArchive();

    bool Open();
    bool Close();

    std::shared_ptr<File> LoadFile(const std::string& filePath);

  private:
    zip_t* mZipArchive = nullptr;
    std::mutex mZipMutex;
};
} // namespace Engine
