#include "global.h"

#include <string.h>

extern void PathEngineOverlay_QueueGameText(const char* text, float x, float y, float red, float green, float blue,
                                             float alpha);

static void GfxPrint_QueueSpan(GfxPrint* printer, const char* text, size_t length) {
    char line[256];

    while (length != 0) {
        size_t count = length < sizeof(line) - 1 ? length : sizeof(line) - 1;
        memcpy(line, text, count);
        line[count] = '\0';

        PathEngineOverlay_QueueGameText(line, printer->posX * 0.25f, printer->posY * 0.25f,
                                        printer->color.r / 255.0f, printer->color.g / 255.0f,
                                        printer->color.b / 255.0f, printer->color.a / 255.0f);
        printer->posX += (u16)(count * 32);
        text += count;
        length -= count;
    }
}

void GfxPrint_SetColor(GfxPrint* printer, u32 r, u32 g, u32 b, u32 a) {
    printer->color.r = (u8)r;
    printer->color.g = (u8)g;
    printer->color.b = (u8)b;
    printer->color.a = (u8)a;
}

void GfxPrint_SetPosPx(GfxPrint* printer, s32 x, s32 y) {
    printer->posX = printer->baseX + (u16)(x * 4);
    printer->posY = printer->baseY + (u16)(y * 4);
}

void GfxPrint_SetPos(GfxPrint* printer, s32 x, s32 y) {
    GfxPrint_SetPosPx(printer, x * 8, y * 8);
}

void GfxPrint_SetBasePosPx(GfxPrint* printer, s32 x, s32 y) {
    printer->baseX = (u16)(x * 4);
    printer->baseY = (u8)(y * 4);
}

void GfxPrint_PrintStringWithSize(GfxPrint* printer, const void* buffer, u32 charSize, u32 charCount) {
    const char* cursor = (const char*)buffer;
    size_t remaining = (size_t)charSize * charCount;

    while (remaining != 0) {
        const char* newline = memchr(cursor, '\n', remaining);
        size_t length = newline != NULL ? (size_t)(newline - cursor) : remaining;

        if (length != 0) {
            GfxPrint_QueueSpan(printer, cursor, length);
        }
        if (newline == NULL) {
            break;
        }

        printer->posX = printer->baseX;
        printer->posY += 32;
        remaining -= length + 1;
        cursor = newline + 1;
    }
}

void GfxPrint_PrintString(GfxPrint* printer, const char* text) {
    if (text != NULL) {
        GfxPrint_PrintStringWithSize(printer, text, sizeof(char), (u32)strlen(text));
    }
}

static void* GfxPrint_Callback(void* argument, const char* text, size_t size) {
    GfxPrint_PrintStringWithSize((GfxPrint*)argument, text, sizeof(char), (u32)size);
    return argument;
}

void GfxPrint_Init(GfxPrint* printer) {
    memset(printer, 0, sizeof(*printer));
    printer->callback = GfxPrint_Callback;
    printer->flags = GFXP_FLAG_SHADOW | GFXP_FLAG_UPDATE;
    printer->color.r = 255;
    printer->color.g = 255;
    printer->color.b = 255;
    printer->color.a = 255;
}

void GfxPrint_Destroy(GfxPrint* printer) {
    (void)printer;
}

void GfxPrint_Open(GfxPrint* printer, Gfx* displayList) {
    printer->flags |= GFXP_FLAG_OPEN;
    printer->dList = displayList;
}

Gfx* GfxPrint_Close(GfxPrint* printer) {
    Gfx* displayList = printer->dList;
    printer->flags &= ~GFXP_FLAG_OPEN;
    printer->dList = NULL;
    return displayList;
}

s32 GfxPrint_VPrintf(GfxPrint* printer, const char* format, va_list arguments) {
    return PrintUtils_VPrintf(&printer->callback, format, arguments);
}

s32 GfxPrint_Printf(GfxPrint* printer, const char* format, ...) {
    va_list arguments;

    va_start(arguments, format);
    s32 result = GfxPrint_VPrintf(printer, format, arguments);
    va_end(arguments);
    return result;
}
