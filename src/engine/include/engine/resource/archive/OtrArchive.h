#ifdef INCLUDE_MPQ_SUPPORT

#pragma once

#undef _DLL

#include <string>

#include <stdint.h>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <StormLib.h>

#include "engine/resource/Resource.h"
#include "engine/resource/archive/Archive.h"

namespace Engine {
struct File;

class OtrArchive final : virtual public Archive {
  public:
    OtrArchive(const std::string& archivePath);
    ~OtrArchive();

    bool Open();
    bool Close();
    bool WriteFile(const std::string& filename, const std::vector<uint8_t>& data);

    std::shared_ptr<File> LoadFile(const std::string& filePath);
    std::shared_ptr<File> LoadFile(uint64_t hash);

  private:
    HANDLE mHandle;
};
} // namespace Engine

#endif // INCLUDE_MPQ_SUPPORT
