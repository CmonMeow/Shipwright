#include "global.h"

void KaleidoSetup_Update(PlayState* play) {
    // The PC-only fixed loadout has no pause inventory or save screen.
    // Keep the context initialized for code that shares its storage, but do
    // not allow Start or the old L+C-Up debug shortcut to enter pause states.
    (void)play;
}

void KaleidoSetup_Init(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    u64 temp = 0; // Necessary to match

    pauseCtx->state = 0;
    pauseCtx->debugState = 0;
    pauseCtx->alpha = 0;
    pauseCtx->unk_1EA = 0;
    pauseCtx->unk_1E4 = 0;
    pauseCtx->mode = 0;
    pauseCtx->pageIndex = PAUSE_ITEM;

    pauseCtx->unk_1F4 = 160.0f;
    pauseCtx->unk_1F8 = 160.0f;
    pauseCtx->unk_1FC = 160.0f;
    pauseCtx->unk_200 = 160.0f;
    pauseCtx->eye.z = 64.0f;
    pauseCtx->unk_1F0 = 936.0f;
    pauseCtx->eye.x = pauseCtx->eye.y = 0.0f;
    pauseCtx->unk_204 = -314.0f;

    pauseCtx->cursorPoint[PAUSE_ITEM] = 0;
    pauseCtx->cursorPoint[PAUSE_MAP] = VREG(30) + 3;
    pauseCtx->cursorPoint[PAUSE_QUEST] = 0;
    pauseCtx->cursorPoint[PAUSE_EQUIP] = 1;
    pauseCtx->cursorPoint[PAUSE_WORLD_MAP] = 10;

    pauseCtx->cursorX[PAUSE_ITEM] = 0;
    pauseCtx->cursorY[PAUSE_ITEM] = 0;
    pauseCtx->cursorX[PAUSE_MAP] = 0;
    pauseCtx->cursorY[PAUSE_MAP] = 0;
    pauseCtx->cursorX[PAUSE_QUEST] = temp;
    pauseCtx->cursorY[PAUSE_QUEST] = temp;
    pauseCtx->cursorX[PAUSE_EQUIP] = 1;
    pauseCtx->cursorY[PAUSE_EQUIP] = 0;

    pauseCtx->cursorItem[PAUSE_ITEM] = PAUSE_ITEM_NONE;
    pauseCtx->cursorItem[PAUSE_MAP] = VREG(30) + 3;
    pauseCtx->cursorItem[PAUSE_QUEST] = PAUSE_ITEM_NONE;
    pauseCtx->cursorItem[PAUSE_EQUIP] = ITEM_SWORD_KOKIRI;

    pauseCtx->cursorSlot[PAUSE_ITEM] = 0;
    pauseCtx->cursorSlot[PAUSE_MAP] = VREG(30) + 3;
    pauseCtx->cursorSlot[PAUSE_QUEST] = 0;
    pauseCtx->cursorSlot[PAUSE_EQUIP] = pauseCtx->cursorPoint[PAUSE_EQUIP];

    pauseCtx->infoPanelOffsetY = -40;
    pauseCtx->nameDisplayTimer = 0;
    pauseCtx->nameColorSet = 0;
    pauseCtx->cursorColorSet = 4;
    pauseCtx->ocarinaSongIdx = -1;
    pauseCtx->cursorSpecialPos = 0;


    View_Init(&pauseCtx->view, play->state.gfxCtx);
}

void KaleidoSetup_Destroy(PlayState* play) {
}
