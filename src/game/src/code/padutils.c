#include "global.h"

#include <string.h>

void PadUtils_Init(Input* input) {
    memset(input, 0, sizeof(Input));
}

void func_800FCB70(void) {
}

void PadUtils_ResetPressRel(Input* input) {
    input->press.button = 0;
    input->rel.button = 0;
}

uint32_t PadUtils_CheckCurExact(Input* input, uint16_t value) {
    return value == input->cur.button;
}

uint32_t PadUtils_CheckCur(Input* input, uint16_t key) {
    return key == (input->cur.button & key);
}

uint32_t PadUtils_CheckPressed(Input* input, uint16_t key) {
    return key == (input->press.button & key);
}

uint32_t PadUtils_CheckReleased(Input* input, uint16_t key) {
    return key == (input->rel.button & key);
}

uint16_t PadUtils_GetCurButton(Input* input) {
    return input->cur.button;
}

uint16_t PadUtils_GetPressButton(Input* input) {
    return input->press.button;
}

int8_t PadUtils_GetCurX(Input* input) {
    return input->cur.stick_x;
}

int8_t PadUtils_GetCurY(Input* input) {
    return input->cur.stick_y;
}

void PadUtils_SetRelXY(Input* input, int32_t x, int32_t y) {
    input->rel.stick_x = x;
    input->rel.stick_y = y;
}

int8_t PadUtils_GetRelXImpl(Input* input) {
    return input->rel.stick_x;
}

int8_t PadUtils_GetRelYImpl(Input* input) {
    return input->rel.stick_y;
}

int8_t PadUtils_GetRelX(Input* input) {
    return PadUtils_GetRelXImpl(input);
}

int8_t PadUtils_GetRelY(Input* input) {
    return PadUtils_GetRelYImpl(input);
}

void PadUtils_UpdateRelXY(Input* input) {
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
int8_t PadUtils_GetCurRX(Input* input) {
    return input->cur.right_stick_x;
}

int8_t PadUtils_GetCurRY(Input* input) {
    return input->cur.right_stick_y;
}

void PadUtils_SetRelRXY(Input* input, int32_t x, int32_t y) {
    input->rel.right_stick_x = x;
    input->rel.right_stick_y = y;
}

int8_t PadUtils_GetRelRXImpl(Input* input) {
    return input->rel.right_stick_x;
}

int8_t PadUtils_GetRelRYImpl(Input* input) {
    return input->rel.right_stick_y;
}

int8_t PadUtils_GetRelRX(Input* input) {
    return PadUtils_GetRelRXImpl(input);
}

int8_t PadUtils_GetRelRY(Input* input) {
    return PadUtils_GetRelRYImpl(input);
}

void PadUtils_UpdateRelRXY(Input* input) {
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
