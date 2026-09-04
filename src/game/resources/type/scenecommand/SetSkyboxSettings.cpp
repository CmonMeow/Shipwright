#include "SetSkyboxSettings.h"

namespace Game::Resources {
SkyboxSettings* SetSkyboxSettings::GetPointer() {
    return &settings;
}

size_t SetSkyboxSettings::GetPointerSize() {
    return sizeof(SetSkyboxSettings);
}
} // namespace Game::Resources
