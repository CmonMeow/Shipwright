#include "SetCameraSettings.h"

namespace Game::Resources {
CameraSettings* SetCameraSettings::GetPointer() {
    return &settings;
}

size_t SetCameraSettings::GetPointerSize() {
    return sizeof(CameraSettings);
}
} // namespace Game::Resources
