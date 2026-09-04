#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <engine/resource/Resource.h>
#include "SceneCommand.h"
#include <runtime/libultra/types.h>

namespace Game::Resources {
typedef struct {
    /* 0x00 */ uint8_t spawn;
    /* 0x01 */ uint8_t room;
} EntranceEntry;

class SetEntranceList : public SceneCommand<EntranceEntry> {
  public:
    using SceneCommand::SceneCommand;

    EntranceEntry* GetPointer();
    size_t GetPointerSize();

    uint32_t numEntrances;

    std::vector<EntranceEntry> entrances;
};
}; // namespace Game::Resources
