#ifndef Z_DOOR_SHUTTER_H
#define Z_DOOR_SHUTTER_H

#include <libultraship/libultra.h>
#include "global.h"

/**
 * Actor Parameters
 *
 * |                  |         |
 * | Transition Index | Type    | Switch Flag
 * |------------------|---------|-------------
 * | 0 0 0 0 0 0      | 0 0 0 0 | 0 0 0 0 0 0
 * | 6                | 4       | 6
 * |
 *
 * Transition Index     1111110000000000    Set by the actor engine when the door is spawned
 * Type                 0000001111000000
 * Switch Flag          0000000000111111
 *
 */

typedef enum {
    /* 0x00 */ SHUTTER,
    /* 0x01 */ SHUTTER_FRONT_CLEAR,
    /* 0x02 */ SHUTTER_FRONT_SWITCH,
    /* 0x03 */ SHUTTER_BACK_LOCKED,
    /* 0x04 */ SHUTTER_PG_BARS,
    /* 0x05 */ SHUTTER_BOSS,
    /* 0x06 */ SHUTTER_GOHMA_BLOCK,
    /* 0x07 */ SHUTTER_FRONT_SWITCH_BACK_CLEAR,
    /* 0x08 */ SHUTTER_8,
    /* 0x09 */ SHUTTER_9,
    /* 0x0A */ SHUTTER_A,
    /* 0x0B */ SHUTTER_KEY_LOCKED,
    /* 0x0C */ SHUTTER_C,
    /* 0x0D */ SHUTTER_D,
    /* 0x0E */ SHUTTER_E,
    /* 0x0F */ SHUTTER_F
} DoorShutterType;

struct DoorShutter;

typedef void (*DoorShutterActionFunc)(struct DoorShutter*, PlayState*);

typedef struct DoorShutter {
    /* 0x0000 */ DynaPolyActor dyna;
    /* 0x0164 */ int16_t unk_164;
    /* 0x0166 */ int16_t unk_166;
    /* 0x0168 */ int16_t unk_168;
    /* 0x016A */ uint8_t doorType;
    /* 0x016B */ uint8_t unk_16B;
    /* 0x016C */ uint8_t unk_16C;
    /* 0x016D */ int8_t requiredObjBankIndex;
    /* 0x016E */ int8_t unk_16E;
    /* 0x016F */ int8_t unk_16F;
    /* 0x0170 */ float unk_170;
    /* 0x0174 */ DoorShutterActionFunc actionFunc;
} DoorShutter; // size = 0x0178

#endif
