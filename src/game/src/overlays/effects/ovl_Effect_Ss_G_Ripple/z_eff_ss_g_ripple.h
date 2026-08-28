#ifndef Z_EFF_SS_G_RIPPLE_H
#define Z_EFF_SS_G_RIPPLE_H

#include <runtime/libultra.h>
#include "global.h"

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ int16_t radius;
    /* 0x0E */ int16_t radiusMax;
    /* 0x10 */ int16_t life;
} EffectSsGRippleInitParams; // size = 0x14

#endif
