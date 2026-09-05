#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <platform/win32/Input.h>

#include "gameplay/Controls.h"
#include "platform/win32/App.h"
#include "platform/win32/PCInput.h"

namespace {

bool InputAvailable() {
    return !App.suppressWorldMouse;
}

void DiscardPress(uint8_t code) {
    input.keyPress[code] = false;
    input.framePress[code] = false;
}

} // namespace

extern "C" {

int32_t PCInput_ConsumeMouseAimDelta(int32_t* deltaX, int32_t* deltaY) {
    *deltaX = 0;
    *deltaY = 0;
    if (!InputAvailable()) {
        input.ConsumeMouseDelta();
        return false;
    }

    const vec2i delta = input.ConsumeMouseDelta();
    *deltaX = delta.x;
    *deltaY = App.invertCameraY ? -delta.y : delta.y;
    return true;
}

int32_t PCInput_ConsumeToggleWeapon(void) {
    return InputAvailable() && input.ConsumePress('X');
}

int32_t PCInput_ConsumeEvade(void) {
    return InputAvailable() && input.ConsumePress(VK_SPACE);
}

void PCInput_DiscardActionIntents(void) {
    for (uint8_t code = '1'; code <= '4'; ++code) {
        DiscardPress(code);
    }
    DiscardPress('X');
    DiscardPress(VK_SPACE);
    DiscardPress(VK_LBUTTON);
}

int32_t PCInput_IsFishingReelHeld(void) {
    return InputAvailable() && controls.weapon == 4 && input.key[VK_RBUTTON];
}

int32_t PCInput_IsFishingReelPressed(void) {
    return InputAvailable() && controls.weapon == 4 && input.framePress[VK_RBUTTON];
}

int32_t PCInput_IsBlockHeld(void) {
    return InputAvailable() && controls.weapon >= 1 && controls.weapon <= 2 && input.key[VK_RBUTTON];
}

int32_t PCInput_IsBlockPressed(void) {
    return InputAvailable() && controls.weapon >= 1 && controls.weapon <= 2 && input.framePress[VK_RBUTTON];
}

int32_t PCInput_GetSelectedWeaponSlot(void) {
    return controls.weapon;
}

int32_t PCInput_ConsumeWeaponSelection(void) {
    int32_t selected = 0;
    for (uint8_t slot = 1; slot <= 4; ++slot) {
        if (input.ConsumePress('0' + slot)) {
            selected = slot;
        }
    }
    return InputAvailable() ? selected : 0;
}

int32_t PCInput_IsBowAimHeld(void) {
    return InputAvailable() && controls.weapon == 3 && input.key[VK_RBUTTON];
}

int32_t PCInput_PrimaryHeld(void) {
    return InputAvailable() && input.key[VK_LBUTTON];
}

int32_t PCInput_PrimaryPressed(void) {
    return InputAvailable() && input.framePress[VK_LBUTTON];
}

void PCInput_ConsumeBowUseIntent(void) {
    input.ConsumePress(VK_LBUTTON);
}

int32_t PCInput_HasBowUseIntent(void) {
    return InputAvailable() && controls.weapon == 3 && input.keyPress[VK_LBUTTON];
}

void PCInput_ReadMovement(int8_t* x, int8_t* y) {
    *x = 0;
    *y = 0;
    if (!InputAvailable()) {
        PCInput_DiscardActionIntents();
        return;
    }

    *x = static_cast<int8_t>(controls.move.x * 85);
    *y = static_cast<int8_t>(controls.move.y * 85);
}

} // extern "C"
