#ifndef Z_EN_ARROW_H
#define Z_EN_ARROW_H

#include <libultraship/libultra.h>
#include "global.h"

struct EnArrow;

typedef void (*EnArrowActionFunc)(struct EnArrow*, PlayState*);

typedef struct EnArrow {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ SkelAnime skelAnime;
    /* 0x0190 */ ColliderQuad collider;
    /* 0x0210 */ Vec3f unk_210;
    /* 0x021C */ WeaponInfo weaponInfo;
    /* 0x0238 */ uint8_t timer; // used for disappearing while flying or after an enemy arrow hits a wall
    /* 0x0239 */ uint8_t hitFlags;
    /* 0x023A */ uint8_t touchedPoly;
    /* 0x023B */ uint8_t isCsNut;
    /* 0x023C */ Actor* hitActor;
    /* 0x0240 */ Vec3f unk_250;
    /* 0x024C */ EnArrowActionFunc actionFunc;
} EnArrow; // size = 0x0250

typedef enum {
    /* -10 */ ARROW_CS_NUT = -10, // cutscene deku nuts are allowed to update in blocking mode
    /* -1  */ ARROW_NORMAL_SILENT = -1, // normal arrow that does not make a sound when being shot
    /*  0  */ ARROW_NORMAL_LIT, // normal arrow lit on fire
    /*  1  */ ARROW_NORMAL_HORSE, // normal arrow shot while riding a horse
    /*  2  */ ARROW_NORMAL,
    /*  3  */ ARROW_FIRE,
    /*  4  */ ARROW_ICE,
    /*  5  */ ARROW_LIGHT,
    /*  6  */ ARROW_0C,
    /*  7  */ ARROW_0D,
    /*  8  */ ARROW_0E,
    /*  9  */ ARROW_SEED,
    /*  10 */ ARROW_NUT
} ArrowType;

#endif
