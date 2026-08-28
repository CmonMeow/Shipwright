#pragma once

#include "types.h"

typedef struct {
    /* 0x0 */ union {
        /* 0x0 */ int64_t ll;
        /* 0x0 */ double ld;
    } v;
    /* 0x8 */ char* s;
    /* 0xC */ int32_t n0;
    /* 0x10 */ int32_t nz0;
    /* 0x14 */ int32_t n1;
    /* 0x18 */ int32_t nz1;
    /* 0x1C */ int32_t n2;
    /* 0x20 */ int32_t nz2;
    /* 0x24 */ int32_t prec;
    /* 0x28 */ int32_t width;
    /* 0x2C */ uint32_t nchar;
    /* 0x30 */ uint32_t flags;
    /* 0x34 */ uint8_t qual;
} _Pft; // size = 0x38

typedef void* (*PrintCallback)(void*, const char*, uint32_t);

#define FLAGS_SPACE 1
#define FLAGS_PLUS 2
#define FLAGS_MINUS 4
#define FLAGS_HASH 8
#define FLAGS_ZERO 16
