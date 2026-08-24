#ifndef Z_BG_SPOT03_TAKI_H
#define Z_BG_SPOT03_TAKI_H

#include <libultraship/libultra.h>
#include "global.h"

typedef struct BgSpot03Taki {
    /* 0x0000 */ DynaPolyActor dyna;
    /* 0x0164 */ f32 openingAlpha;
    /* 0x0168 */ u8 bufferIndex;
} BgSpot03Taki; // size = 0x016C

#endif
