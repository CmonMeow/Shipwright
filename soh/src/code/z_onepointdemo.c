#include "global.h"
#include "z64camera.h"

/*
 * Cinematic one-point and attention cameras have been removed.  Keep the
 * public ABI as inert stubs because gameplay actors use failure from these
 * requests as the supported path for continuing without a cutscene camera.
 */

void OnePointCutscene_SetCsCamPoints(Camera* camera, s16 actionParameters, s16 initTimer,
                                     CutsceneCameraPoint* atPoints, CutsceneCameraPoint* eyePoints) {
    (void)camera;
    (void)actionParameters;
    (void)initTimer;
    (void)atPoints;
    (void)eyePoints;
}

s16 OnePointCutscene_Init(PlayState* play, s16 csId, s16 timer, Actor* actor, s16 camIdx) {
    (void)play;
    (void)csId;
    (void)timer;
    (void)actor;
    (void)camIdx;
    return SUBCAM_NONE;
}

s16 OnePointCutscene_EndCutscene(PlayState* play, s16 camIdx) {
    (void)play;
    return camIdx;
}

s32 OnePointCutscene_Attention(PlayState* play, Actor* actor) {
    (void)play;
    (void)actor;
    return SUBCAM_NONE;
}

s32 OnePointCutscene_AttentionSetSfx(PlayState* play, Actor* actor, s32 sfxId) {
    (void)play;
    (void)actor;
    (void)sfxId;
    return SUBCAM_NONE;
}

void OnePointCutscene_EnableAttention(void) {
}

void OnePointCutscene_DisableAttention(void) {
}

s32 OnePointCutscene_CheckForCategory(PlayState* play, s32 actorCategory) {
    (void)play;
    (void)actorCategory;
    return false;
}

void OnePointCutscene_Noop(PlayState* play, s32 arg1) {
    (void)play;
    (void)arg1;
}
