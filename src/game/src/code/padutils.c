#include "global.h"

#include <string.h>

void PadUtils_Init(ControllerInput* input) {
    memset(input, 0, sizeof(ControllerInput));
}

void func_800FCB70(void) {
}

void PadUtils_ResetPressRel(ControllerInput* input) {
    input->press.button = 0;
    input->rel.button = 0;
}

uint32_t PadUtils_CheckCurExact(ControllerInput* input, uint16_t value) {
    return value == input->cur.button;
}

uint32_t PadUtils_CheckCur(ControllerInput* input, uint16_t key) {
    return key == (input->cur.button & key);
}

uint32_t PadUtils_CheckPressed(ControllerInput* input, uint16_t key) {
    return key == (input->press.button & key);
}

uint32_t PadUtils_CheckReleased(ControllerInput* input, uint16_t key) {
    return key == (input->rel.button & key);
}

uint16_t PadUtils_GetCurButton(ControllerInput* input) {
    return input->cur.button;
}

uint16_t PadUtils_GetPressButton(ControllerInput* input) {
    return input->press.button;
}

int8_t PadUtils_GetCurX(ControllerInput* input) {
    return input->cur.stick_x;
}

int8_t PadUtils_GetCurY(ControllerInput* input) {
    return input->cur.stick_y;
}

void PadUtils_SetRelXY(ControllerInput* input, int32_t x, int32_t y) {
    input->rel.stick_x = x;
    input->rel.stick_y = y;
}

int8_t PadUtils_GetRelXImpl(ControllerInput* input) {
    return input->rel.stick_x;
}

int8_t PadUtils_GetRelYImpl(ControllerInput* input) {
    return input->rel.stick_y;
}

int8_t PadUtils_GetRelX(ControllerInput* input) {
    return PadUtils_GetRelXImpl(input);
}

int8_t PadUtils_GetRelY(ControllerInput* input) {
    return PadUtils_GetRelYImpl(input);
}

void PadUtils_UpdateRelXY(ControllerInput* input) {
    int32_t curX = PadUtils_GetCurX(input);
    int32_t curY = PadUtils_GetCurY(input);
    int32_t relX = { 0 };
    int32_t relY = { 0 };

    if (curX > 7) {
        relX = (curX < 0x43) ? curX - 7 : 0x43 - 7;
    } else if (curX < -7) {
        relX = (curX > -0x43) ? curX + 7 : -0x43 + 7;
    } else {
        relX = 0;
    }

    if (curY > 7) {
        relY = (curY < 0x43) ? curY - 7 : 0x43 - 7;

    } else if (curY < -7) {
        relY = (curY > -0x43) ? curY + 7 : -0x43 + 7;
    } else {
        relY = 0;
    }

    PadUtils_SetRelXY(input, relX, relY);
}

// PC controller right-stick helpers.
int8_t PadUtils_GetCurRX(ControllerInput* input) {
    return input->cur.right_stick_x;
}

int8_t PadUtils_GetCurRY(ControllerInput* input) {
    return input->cur.right_stick_y;
}

void PadUtils_SetRelRXY(ControllerInput* input, int32_t x, int32_t y) {
    input->rel.right_stick_x = x;
    input->rel.right_stick_y = y;
}

int8_t PadUtils_GetRelRXImpl(ControllerInput* input) {
    return input->rel.right_stick_x;
}

int8_t PadUtils_GetRelRYImpl(ControllerInput* input) {
    return input->rel.right_stick_y;
}

int8_t PadUtils_GetRelRX(ControllerInput* input) {
    return PadUtils_GetRelRXImpl(input);
}

int8_t PadUtils_GetRelRY(ControllerInput* input) {
    return PadUtils_GetRelRYImpl(input);
}

void PadUtils_UpdateRelRXY(ControllerInput* input) {
    int32_t curX = PadUtils_GetCurRX(input);
    int32_t curY = PadUtils_GetCurRY(input);
    int32_t relX = { 0 };
    int32_t relY = { 0 };

    if (curX > 7) {
        relX = (curX < 0x43) ? curX - 7 : 0x43 - 7;
    } else if (curX < -7) {
        relX = (curX > -0x43) ? curX + 7 : -0x43 + 7;
    } else {
        relX = 0;
    }

    if (curY > 7) {
        relY = (curY < 0x43) ? curY - 7 : 0x43 - 7;

    } else if (curY < -7) {
        relY = (curY > -0x43) ? curY + 7 : -0x43 + 7;
    } else {
        relY = 0;
    }

    PadUtils_SetRelRXY(input, relX, relY);
}
