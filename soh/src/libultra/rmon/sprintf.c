#include "global.h"

void* proutSprintf(void* dst, const char* fmt, size_t size) {
    return (void*)((uint32_t)memcpy(dst, fmt, size) + size);
}

int32_t vsprintf(char* dst, const char* fmt, va_list args) {
    int32_t ret = _Printf(proutSprintf, dst, fmt, args);
    if (ret > -1) {
        dst[ret] = '\0';
    }
    return ret;
}

int32_t sprintf(char* dst, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int32_t ret = _Printf(proutSprintf, dst, fmt, args);
    if (ret > -1) {
        dst[ret] = '\0';
    }

    va_end(args);

    return ret;
}
