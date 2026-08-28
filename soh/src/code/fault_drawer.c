#include "global.h"

#include <string.h>

extern const uint8_t* PathEngineFont_GetSerifGlyph(unsigned char character);

FaultDrawer sFaultDrawerDefault = {
    (uint16_t*)(0x80400000 - sizeof(uint16_t[SCREEN_HEIGHT][SCREEN_WIDTH])),
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    16,
    223,
    22,
    297,
    GPACK_RGBA5551(255, 255, 255, 255),
    GPACK_RGBA5551(0, 0, 0, 0),
    22,
    16,
    NULL,
    8,
    8,
    0,
    0,
    {
        GPACK_RGBA5551(0, 0, 0, 1),       GPACK_RGBA5551(255, 0, 0, 1),
        GPACK_RGBA5551(0, 255, 0, 1),     GPACK_RGBA5551(255, 255, 0, 1),
        GPACK_RGBA5551(0, 0, 255, 1),     GPACK_RGBA5551(255, 0, 255, 1),
        GPACK_RGBA5551(0, 255, 255, 1),   GPACK_RGBA5551(255, 255, 255, 1),
        GPACK_RGBA5551(120, 120, 120, 1), GPACK_RGBA5551(176, 176, 176, 1),
    },
    0,
    0,
    NULL,
};

FaultDrawer sFaultDrawerStruct;
char D_8016B6C0[0x20];

void FaultDrawer_SetOsSyncPrintfEnabled(uint32_t enabled) {
    sFaultDrawerStruct.osSyncPrintfEnabled = enabled;
}

void FaultDrawer_DrawRecImpl(int32_t xStart, int32_t yStart, int32_t xEnd, int32_t yEnd, uint16_t color) {
    int32_t xSize = MIN(sFaultDrawerStruct.w - xStart, xEnd - xStart + 1);
    int32_t ySize = MIN(sFaultDrawerStruct.h - yStart, yEnd - yStart + 1);
    int32_t x;
    int32_t y;
    uint16_t* framebuffer = { 0 };

    if (xSize <= 0 || ySize <= 0) {
        return;
    }

    framebuffer = sFaultDrawerStruct.fb + sFaultDrawerStruct.w * yStart + xStart;
    for (y = 0; y < ySize; y++) {
        for (x = 0; x < xSize; x++) {
            framebuffer[x] = color;
        }
        framebuffer += sFaultDrawerStruct.w;
    }
    osWritebackDCacheAll();
}

void FaultDrawer_DrawChar(char character) {
    const uint8_t* glyph = PathEngineFont_GetSerifGlyph((unsigned char)character);
    const int32_t cursorX = sFaultDrawerStruct.cursorX;
    const int32_t cursorY = sFaultDrawerStruct.cursorY;
    int32_t x;
    int32_t y;
    uint16_t* framebuffer = { 0 };

    if (cursorX < sFaultDrawerStruct.xStart || cursorY < sFaultDrawerStruct.yStart ||
        cursorX + 7 > sFaultDrawerStruct.xEnd || cursorY + 7 > sFaultDrawerStruct.yEnd) {
        return;
    }

    framebuffer = sFaultDrawerStruct.fb + sFaultDrawerStruct.w * cursorY + cursorX;
    for (y = 0; y < 8; y++) {
        const uint16_t row0 = (uint16_t)((glyph[y * 4] << 8) | glyph[y * 4 + 1]);
        const uint16_t row1 = (uint16_t)((glyph[y * 4 + 2] << 8) | glyph[y * 4 + 3]);

        for (x = 0; x < 8; x++) {
            const uint16_t mask = (uint16_t)(0xC000 >> (x * 2));
            if ((row0 | row1) & mask) {
                framebuffer[x] = sFaultDrawerStruct.foreColor;
            } else if (sFaultDrawerStruct.backColor & 1) {
                framebuffer[x] = sFaultDrawerStruct.backColor;
            }
        }
        framebuffer += sFaultDrawerStruct.w;
    }
}

int32_t FaultDrawer_ColorToPrintColor(uint16_t color) {
    int32_t i;
    for (i = 0; i < ARRAY_COUNT(sFaultDrawerStruct.printColors); i++) {
        if (color == sFaultDrawerStruct.printColors[i]) {
            return i;
        }
    }
    return -1;
}

void FaultDrawer_UpdatePrintColor(void) {
}

void FaultDrawer_SetForeColor(uint16_t color) {
    sFaultDrawerStruct.foreColor = color;
}

void FaultDrawer_SetBackColor(uint16_t color) {
    sFaultDrawerStruct.backColor = color;
}

void FaultDrawer_SetFontColor(uint16_t color) {
    FaultDrawer_SetForeColor(color | 1);
}

void FaultDrawer_SetCharPad(int8_t padW, int8_t padH) {
    sFaultDrawerStruct.charWPad = padW;
    sFaultDrawerStruct.charHPad = padH;
}

void FaultDrawer_SetCursor(int32_t x, int32_t y) {
    sFaultDrawerStruct.cursorX = (uint16_t)x;
    sFaultDrawerStruct.cursorY = (uint16_t)y;
}

void FaultDrawer_FillScreen(void) {
    FaultDrawer_DrawRecImpl(sFaultDrawerStruct.xStart, sFaultDrawerStruct.yStart, sFaultDrawerStruct.xEnd,
                            sFaultDrawerStruct.yEnd, sFaultDrawerStruct.backColor | 1);
    FaultDrawer_SetCursor(sFaultDrawerStruct.xStart, sFaultDrawerStruct.yStart);
}

void* FaultDrawer_FormatStringFunc(void* argument, const char* text, uint32_t count) {
    while (count-- != 0) {
        if (sFaultDrawerStruct.escCode) {
            sFaultDrawerStruct.escCode = false;
            if (*text >= '0' && *text <= '9') {
                FaultDrawer_SetForeColor(sFaultDrawerStruct.printColors[*text - '0']);
            }
        } else if (*text == '\x1A') {
            sFaultDrawerStruct.escCode = true;
        } else if (*text == '\n') {
            if (sFaultDrawerStruct.osSyncPrintfEnabled) {
                osSyncPrintf("\n");
            }
            sFaultDrawerStruct.cursorX = sFaultDrawerStruct.xEnd;
        } else {
            if (sFaultDrawerStruct.osSyncPrintfEnabled) {
                osSyncPrintf("%c", *text);
            }
            FaultDrawer_DrawChar(*text);
            sFaultDrawerStruct.cursorX += sFaultDrawerStruct.charW + sFaultDrawerStruct.charWPad;
        }

        if (sFaultDrawerStruct.cursorX >= sFaultDrawerStruct.xEnd - sFaultDrawerStruct.charW) {
            sFaultDrawerStruct.cursorX = sFaultDrawerStruct.xStart;
            sFaultDrawerStruct.cursorY += sFaultDrawerStruct.charH + sFaultDrawerStruct.charHPad;
            if (sFaultDrawerStruct.cursorY >= sFaultDrawerStruct.yEnd - sFaultDrawerStruct.charH) {
                if (sFaultDrawerStruct.inputCallback != NULL) {
                    sFaultDrawerStruct.inputCallback();
                    FaultDrawer_FillScreen();
                }
                sFaultDrawerStruct.cursorY = sFaultDrawerStruct.yStart;
            }
        }
        text++;
    }

    osWritebackDCacheAll();
    return argument;
}

void FaultDrawer_VPrintf(const char* format, va_list arguments) {
    _Printf(FaultDrawer_FormatStringFunc, (char*)&sFaultDrawerStruct, format, arguments);
}

void FaultDrawer_Printf(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    FaultDrawer_VPrintf(format, arguments);
    va_end(arguments);
}

void FaultDrawer_DrawText(int32_t x, int32_t y, const char* format, ...) {
    va_list arguments;
    FaultDrawer_SetCursor(x, y);
    va_start(arguments, format);
    FaultDrawer_VPrintf(format, arguments);
    va_end(arguments);
}

void FaultDrawer_SetDrawerFB(void* framebuffer, uint16_t width, uint16_t height) {
    sFaultDrawerStruct.fb = framebuffer;
    sFaultDrawerStruct.w = width;
    sFaultDrawerStruct.h = height;
}

void FaultDrawer_SetInputCallback(void (*callback)(void)) {
    sFaultDrawerStruct.inputCallback = callback;
}

void FaultDrawer_WritebackFBDCache(void) {
    osWritebackDCache(sFaultDrawerStruct.fb, sFaultDrawerStruct.w * sFaultDrawerStruct.h * sizeof(uint16_t));
}

void FaultDrawer_SetDefault(void) {
    memcpy(&sFaultDrawerStruct, &sFaultDrawerDefault, sizeof(sFaultDrawerStruct));
    sFaultDrawerStruct.fb = (uint16_t*)((osMemSize | 0x80000000) - sizeof(uint16_t[SCREEN_HEIGHT][SCREEN_WIDTH]));
}
