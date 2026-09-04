#include "SetEchoSettings.h"

namespace Game::Resources {
EchoSettings* SetEchoSettings::GetPointer() {
    return &settings;
}

size_t SetEchoSettings::GetPointerSize() {
    return sizeof(EchoSettings);
}
} // namespace Game::Resources
