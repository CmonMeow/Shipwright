#include "global.h"

#include <string.h>

extern const u8* PathEngineFont_GetSerifGlyph(unsigned char character);

FaultDrawer sFaultDrawerDefault = {
    (u16*)(0x80400000 - sizeof(u16[SCREEN_HEIGHT][SCREEN_WIDTH])),
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

void FaultDrawer_SetOsSyncPrintfEnabled(u32 enabled) {
    sFaultDrawerStruct.osSyncPrintfEnabled = enabled;
}

void FaultDrawer_DrawRecImpl(s32 xStart, s32 yStart, s32 xEnd, s32 yEnd, u16 color) {
    s32 xSize = MIN(sFaultDrawerStruct.w - xStart, xEnd - xStart + 1);
    s32 ySize = MIN(sFaultDrawerStruct.h - yStart, yEnd - yStart + 1);
    s32 x;
    s32 y;
    u16* framebuffer;

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
    const u8* glyph = PathEngineFont_GetSerifGlyph((unsigned char)character);
    const s32 cursorX = sFaultDrawerStruct.cursorX;
    const s32 cursorY = sFaultDrawerStruct.cursorY;
    s32 x;
    s32 y;
    u16* framebuffer;

    if (cursorX < sFaultDrawerStruct.xStart || cursorY < sFaultDrawerStruct.yStart ||
        cursorX + 7 > sFaultDrawerStruct.xEnd || cursorY + 7 > sFaultDrawerStruct.yEnd) {
        return;
    }

    framebuffer = sFaultDrawerStruct.fb + sFaultDrawerStruct.w * cursorY + cursorX;
    for (y = 0; y < 8; y++) {
        const u16 row0 = (u16)((glyph[y * 4] << 8) | glyph[y * 4 + 1]);
        const u16 row1 = (u16)((glyph[y * 4 + 2] << 8) | glyph[y * 4 + 3]);

        for (x = 0; x < 8; x++) {
            const u16 mask = (u16)(0xC000 >> (x * 2));
            if ((row0 | row1) & mask) {
                framebuffer[x] = sFaultDrawerStruct.foreColor;
            } else if (sFaultDrawerStruct.backColor & 1) {
                framebuffer[x] = sFaultDrawerStruct.backColor;
            }
        }
        framebuffer += sFaultDrawerStruct.w;
    }
}

s32 FaultDrawer_ColorToPrintColor(u16 color) {
    s32 i;
    for (i = 0; i < ARRAY_COUNT(sFaultDrawerStruct.printColors); i++) {
        if (color == sFaultDrawerStruct.printColors[i]) {
            return i;
        }
    }
    return -1;
}

void FaultDrawer_UpdatePrintColor(void) {
}

void FaultDrawer_SetForeColor(u16 color) {
    sFaultDrawerStruct.foreColor = color;
}

void FaultDrawer_SetBackColor(u16 color) {
    sFaultDrawerStruct.backColor = color;
}

void FaultDrawer_SetFontColor(u16 color) {
    FaultDrawer_SetForeColor(color | 1);
}

void FaultDrawer_SetCharPad(s8 padW, s8 padH) {
    sFaultDrawerStruct.charWPad = padW;
    sFaultDrawerStruct.charHPad = padH;
}

void FaultDrawer_SetCursor(s32 x, s32 y) {
    sFaultDrawerStruct.cursorX = (u16)x;
    sFaultDrawerStruct.cursorY = (u16)y;
}

void FaultDrawer_FillScreen(void) {
    FaultDrawer_DrawRecImpl(sFaultDrawerStruct.xStart, sFaultDrawerStruct.yStart, sFaultDrawerStruct.xEnd,
                            sFaultDrawerStruct.yEnd, sFaultDrawerStruct.backColor | 1);
    FaultDrawer_SetCursor(sFaultDrawerStruct.xStart, sFaultDrawerStruct.yStart);
}

void* FaultDrawer_FormatStringFunc(void* argument, const char* text, u32 count) {
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

void FaultDrawer_DrawText(s32 x, s32 y, const char* format, ...) {
    va_list arguments;
    FaultDrawer_SetCursor(x, y);
    va_start(arguments, format);
    FaultDrawer_VPrintf(format, arguments);
    va_end(arguments);
}

void FaultDrawer_SetDrawerFB(void* framebuffer, u16 width, u16 height) {
    sFaultDrawerStruct.fb = framebuffer;
    sFaultDrawerStruct.w = width;
    sFaultDrawerStruct.h = height;
}

void FaultDrawer_SetInputCallback(void (*callback)(void)) {
    sFaultDrawerStruct.inputCallback = callback;
}

void FaultDrawer_WritebackFBDCache(void) {
    osWritebackDCache(sFaultDrawerStruct.fb, sFaultDrawerStruct.w * sFaultDrawerStruct.h * sizeof(u16));
}

void FaultDrawer_SetDefault(void) {
    memcpy(&sFaultDrawerStruct, &sFaultDrawerDefault, sizeof(sFaultDrawerStruct));
    sFaultDrawerStruct.fb = (u16*)((osMemSize | 0x80000000) - sizeof(u16[SCREEN_HEIGHT][SCREEN_WIDTH]));
}
