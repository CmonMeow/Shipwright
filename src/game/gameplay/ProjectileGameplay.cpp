#include "ProjectileGameplay.h"

namespace {

ProjectileGameplaySink sSink{};

} // namespace

extern "C" void ProjectileGameplay_SetSink(
    const ProjectileGameplaySink* sink) {
    sSink = sink ? *sink : ProjectileGameplaySink{};
}

extern "C" void ProjectileGameplay_ClearSink(void* context) {
    if (context != sSink.context) return;
    sSink = {};
}

extern "C" void ProjectileGameplay_BindPredictedArrow(
    const void* presentation, int32_t sceneId) {
    if (sSink.bindPredictedArrow) {
        sSink.bindPredictedArrow(presentation, sceneId, sSink.context);
    }
}

extern "C" int32_t ProjectileGameplay_CommitArrowFire(
    const void* presentation, int32_t sceneId, uint32_t clientTick,
    int16_t heading, int16_t aimPitch) {
    return sSink.commitArrowFire
               ? sSink.commitArrowFire(presentation, sceneId, clientTick,
                                       heading, aimPitch, sSink.context)
               : false;
}

extern "C" void ProjectileGameplay_UnbindPredictedArrow(
    const void* presentation) {
    if (sSink.unbindPredictedArrow) {
        sSink.unbindPredictedArrow(presentation, sSink.context);
    }
}
