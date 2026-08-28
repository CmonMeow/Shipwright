#include "global.h"

int32_t PrintUtils_VPrintf(PrintCallback* pfn, const char* fmt, va_list args) {
    return _Printf(*pfn, pfn, fmt, args);
}

int32_t PrintUtils_Printf(PrintCallback* pfn, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int32_t ret = PrintUtils_VPrintf(pfn, fmt, args);

    va_end(args);

    return ret;
}
