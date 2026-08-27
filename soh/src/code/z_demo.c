#include "global.h"

/* Retained ABI used by the Deku Tree mouth and camera environment code. */
u16 D_8015FCC0 = 0;
u16 D_8015FCC2 = 0;
u16 D_8015FCC4 = 0;
u8 D_8015FCC8 = 0;

static void Cutscene_Reset(PlayState* play, CutsceneContext* csCtx) {
    s16 index;

    gSaveContext.cutsceneTrigger = 0;
    if (gSaveContext.cutsceneIndex >= 0xFFF0) {
        gSaveContext.cutsceneIndex = 0;
    }
    gSaveContext.cutsceneTransitionControl = 0;
    csCtx->state = CS_STATE_IDLE;
    csCtx->frames = 0;
    csCtx->segment = NULL;
    csCtx->linkAction = NULL;
    for (index = 0; index < 10; ++index) {
        csCtx->npcActions[index] = NULL;
    }
    Audio_SetCutsceneFlag(0);
    (void)play;
}

void func_8006450C(PlayState* play, CutsceneContext* csCtx) {
    Cutscene_Reset(play, csCtx);
    csCtx->unk_0C = 0.0f;
}

void func_80064520(PlayState* play, CutsceneContext* csCtx) {
    Cutscene_Reset(play, csCtx);
}

void func_80064534(PlayState* play, CutsceneContext* csCtx) {
    Cutscene_Reset(play, csCtx);
}

void func_80064558(PlayState* play, CutsceneContext* csCtx) {
    Cutscene_Reset(play, csCtx);
}

void func_800645A0(PlayState* play, CutsceneContext* csCtx) {
    Cutscene_Reset(play, csCtx);
}

void Cutscene_HandleEntranceTriggers(PlayState* play) {
    /* Entrance cinematics and their one-time progression gates are gone. */
    (void)play;
    gSaveContext.cutsceneTrigger = 0;
    if (gSaveContext.cutsceneIndex >= 0xFFF0) {
        gSaveContext.cutsceneIndex = 0;
    }
}

void Cutscene_HandleConditionalTriggers(PlayState* play) {
    (void)play;
    gSaveContext.cutsceneTrigger = 0;
    if (gSaveContext.cutsceneIndex >= 0xFFF0) {
        gSaveContext.cutsceneIndex = 0;
    }
}

void Cutscene_SetSegment(PlayState* play, void* segment) {
    (void)segment;
    play->csCtx.segment = NULL;
    gSaveContext.cutsceneTrigger = 0;
}
