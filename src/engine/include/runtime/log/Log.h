#pragma once

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
#define LOG_INLINE inline
#else
#define LOG_INLINE static inline
#endif

#ifdef _MSC_VER
#define LOG_CALL __cdecl
#else
#define LOG_CALL
#endif

LOG_INLINE void ClearLog(void) {
    FILE* logfile = fopen("Log", "w");
    if (logfile) {
        fclose(logfile);
    }
}

LOG_INLINE void LOG_CALL Error(const char* format, ...) {
    FILE* logfile = fopen("Log", "a");
    if (logfile) {
        va_list arglist;
        va_start(arglist, format);
        vfprintf(logfile, format, arglist);
        fprintf(logfile, "\n");
        fclose(logfile);
        va_end(arglist);
    }
}

#undef LOG_INLINE
#undef LOG_CALL
