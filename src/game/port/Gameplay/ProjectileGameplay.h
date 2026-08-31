#pragma once

#include <stdint.h>

typedef struct ProjectileGameplaySink {
    void (*bindPredictedArrow)(const void* presentation, int32_t sceneId,
                               void* context);
    int32_t (*commitArrowFire)(const void* presentation, int32_t sceneId,
                               void* context);
    void (*unbindPredictedArrow)(const void* presentation, void* context);
    void* context;
} ProjectileGameplaySink;

#ifdef __cplusplus
extern "C" {
#endif

void ProjectileGameplay_SetSink(const ProjectileGameplaySink* sink);
void ProjectileGameplay_ClearSink(void* context);
void ProjectileGameplay_BindPredictedArrow(const void* presentation,
                                            int32_t sceneId);
int32_t ProjectileGameplay_CommitArrowFire(const void* presentation,
                                           int32_t sceneId);
void ProjectileGameplay_UnbindPredictedArrow(const void* presentation);

#ifdef __cplusplus
}
#endif
