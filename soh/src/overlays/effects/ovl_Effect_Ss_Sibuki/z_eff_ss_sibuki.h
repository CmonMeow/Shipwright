#ifndef Z_EFF_SS_SIBUKI_H
#define Z_EFF_SS_SIBUKI_H

#include <libultraship/libultra.h>
#include "global.h"

typedef struct {
    /* 0x00 */ Vec3f pos;
    /* 0x0C */ Vec3f velocity;
    /* 0x18 */ Vec3f accel;
    /* 0x24 */ int16_t moveDelay;
    /* 0x26 */ int16_t direction;
    /* 0x28 */ int16_t scale;
} EffectSsSibukiInitParams; // size = 0x2C

#endif
