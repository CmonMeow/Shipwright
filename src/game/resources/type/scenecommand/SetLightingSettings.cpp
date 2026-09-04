#include "SetLightingSettings.h"

namespace Game::Resources {
EnvLightSettings* SetLightingSettings::GetPointer() {
    return settings.data();
}

size_t SetLightingSettings::GetPointerSize() {
    return settings.size() * sizeof(EnvLightSettings);
}
} // namespace Game::Resources
