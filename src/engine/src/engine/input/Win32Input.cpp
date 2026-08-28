#include "engine/input/Win32Input.h"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Engine {

static Win32Input input;

Win32Input& GetWin32Input() {
    return input;
}

void Win32Input::Clear() {
    std::memset(mKeys, 0, sizeof(mKeys));
    std::memset(mKeyPresses, 0, sizeof(mKeyPresses));
    mTextCount = 0;
    mMouseWheel = 0;
    mMouseDelta = {};
    mTextInputReleasePending = false;
}

void Win32Input::KeyDown(uint8_t key) {
    if (!mKeys[key]) {
        mKeyPresses[key] = true;
    }
    mKeys[key] = true;
}

void Win32Input::KeyUp(uint8_t key) {
    mKeys[key] = false;
}

void Win32Input::TextInput(uint8_t character) {
    if (mTextCount < static_cast<int32_t>(sizeof(mText))) {
        mText[mTextCount++] = character;
    }
}

bool Win32Input::PopTextInput(uint8_t& character) {
    if (mTextCount <= 0) {
        return false;
    }
    character = mText[0];
    if (--mTextCount > 0) {
        std::memmove(mText, mText + 1, mTextCount);
    }
    return true;
}

void Win32Input::AddMouseWheel(int32_t delta) {
    mMouseWheel += delta;
}

int32_t Win32Input::ConsumeMouseWheel() {
    const int32_t result = mMouseWheel;
    mMouseWheel = 0;
    return result;
}

void Win32Input::AddMouseDelta(int32_t x, int32_t y) {
    mMouseDelta.x += x;
    mMouseDelta.y += y;
}

MousePosition Win32Input::ConsumeMouseDelta() {
    const MousePosition result = mMouseDelta;
    mMouseDelta = {};
    return result;
}

bool Win32Input::Pressed(uint8_t key) const {
    return mKeys[key];
}

bool Win32Input::ConsumePress(uint8_t key) {
    const bool result = mKeyPresses[key];
    mKeyPresses[key] = false;
    return result;
}

bool Win32Input::Toggle(uint8_t key, bool& value) {
    if (mKeys[key]) {
        value = !value;
        KeyUp(key);
    }
    return value;
}

void Win32Input::RefreshKeyboardState(bool gameWindowFocused) {
#ifdef _WIN32
    if (!gameWindowFocused || mGameInputBlocked) {
        for (uint16_t key = VK_BACK; key <= UINT8_MAX; ++key) {
            mKeys[key] = false;
        }
        return;
    }

    for (uint16_t key = VK_BACK; key <= UINT8_MAX; ++key) {
        if ((GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0) {
            KeyDown(static_cast<uint8_t>(key));
        } else {
            KeyUp(static_cast<uint8_t>(key));
        }
    }
    if (mTextInputReleasePending) {
        bool anyKeyboardKeyDown = false;
        for (uint16_t key = VK_BACK; key <= UINT8_MAX; ++key) {
            if (mKeys[key]) {
                anyKeyboardKeyDown = true;
                break;
            }
        }
        if (!anyKeyboardKeyDown) {
            mTextInputReleasePending = false;
        }
    }
#else
    (void)gameWindowFocused;
#endif
}

bool Win32Input::LeftClick() const {
#ifdef _WIN32
    return mKeys[VK_LBUTTON];
#else
    return false;
#endif
}

bool Win32Input::RightClick() const {
#ifdef _WIN32
    return mKeys[VK_RBUTTON];
#else
    return false;
#endif
}

void Win32Input::SetGameInputBlocked(bool blocked) {
    mGameInputBlocked = blocked;
    if (blocked) {
        std::memset(mKeys, 0, sizeof(mKeys));
        std::memset(mKeyPresses, 0, sizeof(mKeyPresses));
    }
}

bool Win32Input::IsGameInputBlocked() const {
    return mGameInputBlocked;
}

void Win32Input::SetTextInputCaptured(bool captured) {
    if (mTextInputCaptured && !captured) {
        mTextInputReleasePending = true;
        // Key-down edges accumulated while typing belong exclusively to the
        // text field. If retained, letters such as C are consumed by the N64
        // controller mapper after Enter closes chat and trigger game actions.
        std::memset(mKeyPresses, 0, sizeof(mKeyPresses));
    }
    mTextInputCaptured = captured;
    if (captured) {
        std::memset(mKeyPresses, 0, sizeof(mKeyPresses));
    }
}

bool Win32Input::IsTextInputCaptured() const {
    return mTextInputCaptured || mTextInputReleasePending;
}

} // namespace Engine
