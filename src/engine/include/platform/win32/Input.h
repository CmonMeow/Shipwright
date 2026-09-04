#pragma once

#include "maff.h"

#include <cstdint>

struct Input {
    bool key[256]{};
    bool keyPress[256]{};
    bool framePress[256]{};
    uint8_t text[64]{};
    int32_t textCount = 0;
    int32_t mouseWheel = 0;
    vec2i mouse;
    vec2i mouseDelta;
    bool gameInputBlocked = false;
    bool textInputCaptured = false;
    bool textInputReleasePending = false;

    void Clear();
    void KeyDown(uint8_t key);
    void KeyUp(uint8_t key);
    void TextInput(uint8_t character);
    bool PopTextInput(uint8_t& character);
    void AddMouseWheel(int32_t delta);
    int32_t ConsumeMouseWheel();
    void AddMouseDelta(int32_t x, int32_t y);
    vec2i ConsumeMouseDelta();
    bool ConsumePress(uint8_t key);
    void EndFrame();
    void RefreshKeyboardState(bool gameWindowFocused);
    void SetGameInputBlocked(bool blocked);
    bool IsGameInputBlocked() const;
    void SetTextInputCaptured(bool captured);
    bool IsTextInputCaptured() const;
};

extern Input input;
