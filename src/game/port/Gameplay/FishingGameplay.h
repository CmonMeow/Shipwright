#pragma once

#include <stdint.h>

struct PlayState;

typedef struct FishingLocalVisual {
    float rodTipOffset[3];
    float lureDrawOffset[3];
    float rodBendY;
    float rodBendX;
    float rodTwist;
    float rodCastX;
    float lureRotation[3];
    float lureSpin;
    float lureZOffset;
    float lureHookOffsets[2][3];
    float lureHookRotations[2][2];
    float lineScale;
    float lineGravity;
    int16_t fishRotation[3];
    int16_t fishLimbRotation[8];
    uint8_t state;
    uint8_t lineSpooled;
    uint8_t sinkingLureSegmentIndex;
    uint8_t sinkingLureUnderwater;
} FishingLocalVisual;

typedef enum FishingGameplayAction {
    FISHING_GAMEPLAY_ACTION_HOOK,
    FISHING_GAMEPLAY_ACTION_RELEASE,
} FishingGameplayAction;

typedef int32_t (*FishingGameplayActionSink)(FishingGameplayAction action,
                                              void* context);

#ifdef __cplusplus
extern "C" {
#endif

// Native fishing owns extraction of its renderer state. Consumers receive a
// plain snapshot and never read the actor's file-static globals directly.
int32_t FishingGameplay_ReadLocalVisual(struct PlayState* play,
                                        FishingLocalVisual* visual);

// Installs one semantic gameplay consumer. The native actor emits hook/release
// events without depending on networking, packets, or client session state.
void FishingGameplay_SetActionSink(FishingGameplayActionSink sink,
                                   void* context);
void FishingGameplay_ClearActionSink(void* context);
int32_t FishingGameplay_SubmitAction(FishingGameplayAction action);

#ifdef __cplusplus
}
#endif
