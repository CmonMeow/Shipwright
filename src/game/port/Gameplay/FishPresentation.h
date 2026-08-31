#pragma once

#include <stdint.h>

typedef struct FishPresentationIdentity {
    int32_t sceneId;
    uint32_t spawnKey;
} FishPresentationIdentity;

typedef struct FishPresentationState {
    float position[3];
    int16_t rotation[3];
    int16_t limbRotation[8];
    float length;
    uint8_t isLoach;
} FishPresentationState;

typedef struct FishPresentationSink {
    int32_t (*read)(const FishPresentationIdentity* identity,
                    FishPresentationState* state, void* context);
    void* context;
} FishPresentationSink;

#ifdef __cplusplus
extern "C" {
#endif

void FishPresentation_SetSink(const FishPresentationSink* sink);
void FishPresentation_ClearSink(void* context);
uint32_t FishPresentation_MakeSpawnKey(
    int32_t sceneId, int32_t roomId, int32_t homeX, int32_t homeY,
    int32_t homeZ);
int32_t FishPresentation_Read(const FishPresentationIdentity* identity,
                              FishPresentationState* state);

#ifdef __cplusplus
}
#endif
