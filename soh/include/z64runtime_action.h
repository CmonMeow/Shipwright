#ifndef Z64RUNTIME_ACTION_H
#define Z64RUNTIME_ACTION_H

#include <libultraship/libultra.h>

/* Camera spline points remain part of the native camera implementation. */
typedef struct {
    int8_t continueFlag;
    int8_t cameraRoll;
    uint16_t nextPointFrame;
    float viewAngle;
    Vec3s pos;
} CutsceneCameraPoint;

/* Native player/fish choreography cue; no scene script consumes this type. */
typedef struct {
    uint16_t action;
    uint16_t startFrame;
    uint16_t endFrame;
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
