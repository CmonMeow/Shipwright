#pragma once

#undef _DLL

#include <string>
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
    bool WriteFile(const std::string& filename, const std::vector<uint8_t>& data);

    std::shared_ptr<File> LoadFile(const std::string& filePath);
    std::shared_ptr<File> LoadFile(uint64_t hash);

  private:
    zip_t* mZipArchive;
};
} // namespace Engine
