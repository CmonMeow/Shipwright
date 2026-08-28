#include "global.h"
#include "vt.h"

float LogUtils_CheckFloatRange(const char* exp, int32_t line, const char* valueName, float value, const char* minName, float min,
                             const char* maxName, float max) {
    if (value < min || max < value) {
        osSyncPrintf("%s %d: range error %s(%f) < %s(%f) < %s(%f)\n", exp, line, minName, min, valueName, value,
                     maxName, max);
    }
    return value;
}

int32_t LogUtils_CheckIntRange(const char* exp, int32_t line, const char* valueName, int32_t value, const char* minName, int32_t min,
                           const char* maxName, int32_t max) {
    if (value < min || max < value) {
        osSyncPrintf("%s %d: range error %s(%d) < %s(%d) < %s(%d)\n", exp, line, minName, min, valueName, value,
                     maxName, max);
    }
    return value;
}

void LogUtils_LogHexDump(void* ptr, ptrdiff_t size0) {
    uint8_t* addr = (uint8_t*)ptr;
    ptrdiff_t size = size0;
    uint32_t off;

    osSyncPrintf("dump(%08x, %u)\n", addr, size);
    osSyncPrintf("address  off  +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +a +b +c +d +e +f   0123456789abcdef\n");

    off = 0;
    while (size > 0) {

        osSyncPrintf("%08x %04x", addr, off);
        int32_t rest = (size < 0x10) ? size : 0x10;

        int32_t i = 0;
        while (true) {
            if (i < rest) {
                osSyncPrintf(" %02x", *((uint8_t*)addr + i));
            } else {
                osSyncPrintf("   ");
            }

            i++;
            if (i > 0xF) {
                break;
            }
        }
        osSyncPrintf("   ");

        i = 0;
        while (true) {
            if (i < rest) {
                uint8_t a = *(addr + i);

                osSyncPrintf("%c", (a >= 0x20 && a < 0x7F) ? a : '.');
            } else {
                osSyncPrintf(" ");
            }

            i++;
            if (i > 0xF) {
                break;
            }
        }
        osSyncPrintf("\n");
        size -= rest;
        addr += rest;
        off += rest;
    }
}

void LogUtils_LogPointer(int32_t value, uint32_t max, void* ptr, const char* name, const char* file, int32_t line) {
    osSyncPrintf(VT_COL(RED, WHITE) "%s %d %s[%d] max=%u ptr=%08x\n" VT_RST, file, line, name, value, max, ptr);
}

void LogUtils_CheckBoundary(const char* name, int32_t value, int32_t unk, const char* file, int32_t line) {
    uint32_t mask = (unk - 1);

    if (value & mask) {
        osSyncPrintf(VT_COL(RED, WHITE) "%s %d:%s(%08x) は バウンダリ(%d)違反です\n" VT_RST, file, line, name, value,
                     unk);
    }
}

void LogUtils_CheckNullPointer(const char* exp, void* ptr, const char* file, int32_t line) {
    if (ptr == NULL) {
        osSyncPrintf(VT_COL(RED, WHITE) "%s %d:%s は はヌルポインタです\n" VT_RST, file, line, exp);
    }
}

void LogUtils_CheckValidPointer(const char* exp, void* ptr, const char* file, int32_t line) {
    if (ptr == NULL || (uintptr_t)ptr < 0x80000000 || (0x80000000 + osMemSize) <= (uintptr_t)ptr) {
        osSyncPrintf(VT_COL(RED, WHITE) "%s %d:ポインタ %s(%08x) が異常です\n" VT_RST, file, line, exp, ptr);
    }
}

void LogUtils_LogThreadId(const char* name, int32_t line) {
    osSyncPrintf("<%d %s %d>", osGetThreadId(NULL), name, line);
}

void LogUtils_HungupThread(const char* name, int32_t line) {
    osSyncPrintf("*** HungUp in thread %d, [%s:%d] ***\n", osGetThreadId(NULL), name, line);
    Fault_AddHungupAndCrash(name, line);
}

void LogUtils_ResetHungup(void) {
    osSyncPrintf("*** Reset ***\n");
    Fault_AddHungupAndCrash("Reset", 0);
}
