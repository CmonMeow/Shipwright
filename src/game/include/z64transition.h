#ifndef Z64TRANSITION_H
#define Z64TRANSITION_H

#include <runtime/libultra.h>
#include <runtime/color.h>

typedef struct {
    /* 0x000 */ uint8_t fadeType;
    /* 0x001 */ uint8_t isDone;
    /* 0x002 */ uint8_t fadeDirection;
    /* 0x004 */ Color_RGBA8_u32 fadeColor;
    /* 0x008 */ uint16_t fadeTimer;
} TransitionFade; // size = 0xC

#endif
