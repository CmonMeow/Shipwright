#ifndef Z_EFF_SS_DUST_H
#define Z_EFF_SS_DUST_H

#include <libultraship/libultra.h>
#include "global.h"

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f velocity;
    /* 0x18 */ Vec3f accel;
    /* 0x24 */ Color_RGBA8 primColor;
    /* 0x28 */ Color_RGBA8 envColor;
    /* 0x2C */ int16_t scale;
    /* 0x2E */ int16_t scaleStep;
    /* 0x30 */ int16_t life;
    /* 0x32 */ uint16_t drawFlags;
    /* 0x34 */ uint8_t updateMode;
} EffectSsDustInitParams; // size = 0x38

#endif
