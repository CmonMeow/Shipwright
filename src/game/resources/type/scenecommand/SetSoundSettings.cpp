#include "SetSoundSettings.h"

namespace Game::Resources {
SoundSettings* SetSoundSettings::GetPointer() {
    return &settings;
}

size_t SetSoundSettings::GetPointerSize() {
    return sizeof(SoundSettings);
}
} // namespace Game::Resources
