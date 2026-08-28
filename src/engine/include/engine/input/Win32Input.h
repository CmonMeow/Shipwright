#pragma once

#include <cstdint>

namespace Engine {

struct MousePosition {
    int32_t x = 0;
    int32_t y = 0;
};

class Win32Input {
  public:
    void Clear();
    void KeyDown(uint8_t key);
    void KeyUp(uint8_t key);
    void TextInput(uint8_t character);
    bool PopTextInput(uint8_t& character);
    void AddMouseWheel(int32_t delta);
    int32_t ConsumeMouseWheel();
    void AddMouseDelta(int32_t x, int32_t y);
    MousePosition ConsumeMouseDelta();
    bool Pressed(uint8_t key) const;
    bool ConsumePress(uint8_t key);
    bool Toggle(uint8_t key, bool& value);
    void RefreshKeyboardState(bool gameWindowFocused);
    bool LeftClick() const;
    bool RightClick() const;
    void SetGameInputBlocked(bool blocked);
    bool IsGameInputBlocked() const;
    void SetTextInputCaptured(bool captured);
    bool IsTextInputCaptured() const;

    MousePosition mouse;

  private:
    bool mKeys[256]{};
    bool mKeyPresses[256]{};
    uint8_t mText[64]{};
    int32_t mTextCount = 0;
    int32_t mMouseWheel = 0;
    MousePosition mMouseDelta;
    bool mGameInputBlocked = false;
    bool mTextInputCaptured = false;
    bool mTextInputReleasePending = false;
};

Win32Input& GetWin32Input();

} // namespace Engine
