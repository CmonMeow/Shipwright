#include "global.h"

void PlayerAction_Reset(PlayState* play) {
    s16 index;

    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.cutsceneTransitionControl = 0;
    play->playerActionCtx.state = CS_STATE_IDLE;
    play->playerActionCtx.frames = 0;
    play->playerActionCtx.linkAction = NULL;
    for (index = 0; index < ARRAY_COUNT(play->playerActionCtx.npcActions); ++index) {
        play->playerActionCtx.npcActions[index] = NULL;
    }
    Audio_SetCutsceneFlag(0);
}
