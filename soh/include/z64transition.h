#ifndef Z64TRANSITION_H
#define Z64TRANSITION_H

#include <libultraship/libultra.h>
#include <libultraship/color.h>

typedef struct {
    /* 0x000 */ u8 fadeType;
    /* 0x001 */ u8 isDone;
    /* 0x002 */ u8 fadeDirection;
    /* 0x004 */ Color_RGBA8_u32 fadeColor;
    /* 0x008 */ u16 fadeTimer;
} TransitionFade; // size = 0xC

#endif
