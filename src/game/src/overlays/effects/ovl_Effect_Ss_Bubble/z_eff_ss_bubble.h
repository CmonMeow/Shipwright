#ifndef Z_EFF_SS_BUBBLE_H
#define Z_EFF_SS_BUBBLE_H

#include <runtime/libultra.h>
#include "global.h"

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ float yPosOffset;
    /* 0x10 */ float yPosRandScale;
    /* 0x14 */ float xzPosRandScale;
    /* 0x18 */ float scale;
} EffectSsBubbleInitParams; // size = 0x1C

#endif
