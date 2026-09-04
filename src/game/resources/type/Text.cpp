#include "Text.h"

namespace Game::Resources {
MessageEntry* Text::GetPointer() {
    return messages.data();
}

size_t Text::GetPointerSize() {
    return messages.size() * sizeof(MessageEntry);
}
} // namespace Game::Resources
