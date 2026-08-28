#ifndef Z_EN_BOX_H
#define Z_EN_BOX_H

#include <runtime/libultra.h>
#include "global.h"

#define ENBOX_PARAMS(type, itemId, treasureFlag) ((type) << 12 | (itemId) << 5 | (treasureFlag))
#define ENBOX_TREASURE_FLAG_UNK_MIN 20
#define ENBOX_TREASURE_FLAG_UNK_MAX 32

struct EnBox;

typedef void (*EnBoxActionFunc)(struct EnBox*, PlayState*);

typedef enum {
    /*
    only values 1-11 are used explicitly, other values (like 0) default to another separate behavior
    */
    /*  0 */ ENBOX_TYPE_BIG_DEFAULT,
    /*  1 */ ENBOX_TYPE_ROOM_CLEAR_BIG,         // appear on room clear, store temp clear as permanent clear
    /*  2 */ ENBOX_TYPE_DECORATED_BIG,          // boss key chest, different look, same as ENBOX_TYPE_BIG_DEFAULT otherwise
    /*  3 */ ENBOX_TYPE_SWITCH_FLAG_FALL_BIG,   // falling, appear on switch flag set
    /*  4 */ ENBOX_TYPE_4,                      // big, drawn differently
    /*  5 */ ENBOX_TYPE_SMALL,                  // same as ENBOX_TYPE_BIG_DEFAULT but small
    /*  6 */ ENBOX_TYPE_6,                      // small, drawn differently
    /*  7 */ ENBOX_TYPE_ROOM_CLEAR_SMALL,       // use room clear, store temp clear as perm clear
    /*  8 */ ENBOX_TYPE_SWITCH_FLAG_FALL_SMALL, // falling, appear on switch flag set
    /*  9 */ ENBOX_TYPE_9,                      // big, has something more to do with player and message context?
    /* 10 */ ENBOX_TYPE_10,                     // like 9
    /* 11 */ ENBOX_TYPE_SWITCH_FLAG_BIG         // big, appear on switch flag set
} EnBoxType;

typedef struct EnBox {
    /* 0x0000 */ DynaPolyActor dyna;
    /* 0x0164 */ SkelAnime skelanime;
    /* 0x01A8 */ int32_t unk_1A8; // related to animation delays for types 3 and 8
    /* 0x01AC */ int32_t unk_1AC;
    /* 0x01B0 */ float unk_1B0; // 0-1, rotation-related, apparently unused (in z_en_box.c at least)
    /* 0x01B4 */ EnBoxActionFunc actionFunc;
    /* 0x01B8 */ Vec3s jointTable[5];
    /* 0x01D6 */ Vec3s morphTable[5];
    /* 0x01F4 */ int16_t unk_1F4; // probably a frame count? set by player code
    /* 0x01F6 */ uint8_t movementFlags;
    /* 0x01F7 */ uint8_t alpha;
    /* 0x01F8 */ uint8_t switchFlag;
    /* 0x01F9 */ uint8_t type;
    /* 0x01FA */ uint8_t iceSmokeTimer;
    /* 0x01FB */ uint8_t unk_1FB;
    /*        */ GetItemEntry getItemEntry; // This is only to determine the Chest Style, randomzier item gives are handled elsewhere
    /*        */ Gfx* boxLidDL;
    /*        */ Gfx* boxBodyDL;
} EnBox; // size = 0x01FC

#endif
