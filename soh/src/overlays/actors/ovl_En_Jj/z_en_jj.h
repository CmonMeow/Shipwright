#ifndef Z_EN_JJ_H
#define Z_EN_JJ_H

#include <libultraship/libultra.h>
#include "global.h"

struct EnJj;

typedef void (*EnJjActionFunc)(struct EnJj*, PlayState*);

typedef struct EnJj {
    /* 0x0000 */ DynaPolyActor dyna;
    /* 0x0164 */ SkelAnime skelAnime;
    /* 0x01A8 */ Vec3s jointTable[22];
    /* 0x022C */ Vec3s morphTable[22];
    /* 0x02B0 */ EnJjActionFunc actionFunc;
    /* 0x02B8 */ DynaPolyActor* bodyCollisionActor;
    /* 0x02C0 */ s16 mouthOpenAngle;
    /* 0x02C2 */ u8 eyeIndex;
    /* 0x02C3 */ u8 blinkTimer;
    /* 0x02C4 */ u8 extraBlinkCounter;
    /* 0x02C5 */ u8 extraBlinkTotal;
} EnJj;

typedef enum {
    /* -1 */ JABUJABU_MAIN = -1, // Head, drawn body, handles updating
    /*  0 */ JABUJABU_COLLISION, // Static collision for body
    /*  1 */ JABUJABU_UNUSED_COLLISION // Shaped like a screen
} EnJjType;


#endif
