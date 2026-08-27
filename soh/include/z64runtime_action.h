#ifndef Z64RUNTIME_ACTION_H
#define Z64RUNTIME_ACTION_H

#include <libultraship/libultra.h>

/* Camera spline points remain part of the native camera implementation. */
typedef struct {
    s8 continueFlag;
    s8 cameraRoll;
    u16 nextPointFrame;
    f32 viewAngle;
    Vec3s pos;
} CutsceneCameraPoint;

/* Native player/fish choreography cue; no scene script consumes this type. */
typedef struct {
    u16 action;
    u16 startFrame;
    u16 endFrame;
    union {
        Vec3s rot;
        Vec3us urot;
    };
    Vec3i startPos;
    Vec3i endPos;
    Vec3i normal;
} CsCmdActorCue;

enum {
    CS_STATE_IDLE = 0,
    CS_STATE_UNSKIPPABLE_INIT = 3,
};

#endif
