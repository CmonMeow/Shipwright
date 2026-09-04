#include "SetTimeSettings.h"

namespace Game::Resources {
TimeSettings* SetTimeSettings::GetPointer() {
    return &settings;
}

size_t SetTimeSettings::GetPointerSize() {
    return sizeof(TimeSettings);
}
} // namespace Game::Resources
