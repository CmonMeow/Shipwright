#pragma once

#include <cstdint>
#include <string>
#include <ship/resource/Resource.h>
#include "soh/resource/type/scenecommand/SceneCommand.h"

namespace SOH {
// Scene archives retain command 0x17 for format compatibility. Cutscene data is
// deliberately not loaded by the PC runtime.
class SetCutscenes : public SceneCommand<uint8_t> {
  public:
    using SceneCommand::SceneCommand;

    uint8_t* GetPointer();
    size_t GetPointerSize();

    std::string fileName;
    uint8_t ignored = 0;
};
}; // namespace SOH
