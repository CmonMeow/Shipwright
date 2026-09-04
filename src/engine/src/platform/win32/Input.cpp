#include "platform/win32/Input.h"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void Input::Clear() {
    std::memset(key, 0, sizeof(key));
    std::memset(keyPress, 0, sizeof(keyPress));
    std::memset(framePress, 0, sizeof(framePress));
    textCount = 0;
    mouseWheel = 0;
    mouseDelta = {};
    textInputReleasePending = false;
}

void Input::KeyDown(uint8_t code) {
    if (!key[code]) {
        keyPress[code] = true;
        framePress[code] = true;
    }
    key[code] = true;
}

void Input::KeyUp(uint8_t code) {
    key[code] = false;
}

void Input::TextInput(uint8_t character) {
    if (textCount < static_cast<int32_t>(sizeof(text))) {
        text[textCount++] = character;
    }
}

bool Input::PopTextInput(uint8_t& character) {
    if (textCount <= 0) {
        return false;
    }
    character = text[0];
    if (--textCount > 0) {
        std::memmove(text, text + 1, textCount);
    }
    return true;
}

void Input::AddMouseWheel(int32_t delta) {
    mouseWheel += delta;
}

int32_t Input::ConsumeMouseWheel() {
    const int32_t result = mouseWheel;
    mouseWheel = 0;
    return result;
}

void Input::AddMouseDelta(int32_t x, int32_t y) {
    mouseDelta.x += x;
    mouseDelta.y += y;
}

vec2i Input::ConsumeMouseDelta() {
    const vec2i result = mouseDelta;
    mouseDelta = {};
    return result;
}

bool Input::ConsumePress(uint8_t code) {
    const bool result = keyPress[code];
    keyPress[code] = false;
    return result;
}

void Input::EndFrame() {
    std::memset(keyPress, 0, sizeof(keyPress));
    std::memset(framePress, 0, sizeof(framePress));
}

void Input::RefreshKeyboardState(bool gameWindowFocused) {
#ifdef _WIN32
    if (!gameWindowFocused || gameInputBlocked) {
        for (uint16_t code = VK_BACK; code <= UINT8_MAX; ++code) {
            key[code] = false;
        }
        return;
    }

    for (uint16_t code = VK_BACK; code <= UINT8_MAX; ++code) {
        if ((GetAsyncKeyState(static_cast<int>(code)) & 0x8000) != 0) {
            KeyDown(static_cast<uint8_t>(code));
        } else {
            KeyUp(static_cast<uint8_t>(code));
        }
    }
    if (textInputReleasePending) {
        bool anyKeyboardKeyDown = false;
        for (uint16_t code = VK_BACK; code <= UINT8_MAX; ++code) {
            if (key[code]) {
                anyKeyboardKeyDown = true;
                break;
            }
        }
        if (!anyKeyboardKeyDown) {
            textInputReleasePending = false;
        }
    }
#else
    (void)gameWindowFocused;
#endif
}

void Input::SetGameInputBlocked(bool blocked) {
    gameInputBlocked = blocked;
    if (blocked) {
        std::memset(key, 0, sizeof(key));
        std::memset(keyPress, 0, sizeof(keyPress));
        std::memset(framePress, 0, sizeof(framePress));
    }
}

bool Input::IsGameInputBlocked() const {
    return gameInputBlocked;
}

void Input::SetTextInputCaptured(bool captured) {
    if (textInputCaptured && !captured) {
        textInputReleasePending = true;
        // Key-down edges accumulated while typing belong exclusively to the
        // text field. If retained, letters such as C are consumed by the N64
        // controller mapper after Enter closes chat and trigger game actions.
        std::memset(keyPress, 0, sizeof(keyPress));
        std::memset(framePress, 0, sizeof(framePress));
    }
    textInputCaptured = captured;
    if (captured) {
        std::memset(keyPress, 0, sizeof(keyPress));
        std::memset(framePress, 0, sizeof(framePress));
    }
}

bool Input::IsTextInputCaptured() const {
    return textInputCaptured || textInputReleasePending;
}
