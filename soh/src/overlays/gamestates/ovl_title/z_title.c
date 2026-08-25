/*
 * File: z_title.c
 * Overlay: ovl_title
 * Description: Performs boot-time save/audio initialization before file select
 */

#include "global.h"

void Title_Main(GameState* thisx) {
    TitleContext* this = (TitleContext*)thisx;

    // Keep the original game-state transition instead of bypassing TitleContext.
    // Title_Destroy initializes SRAM and applies the saved audio configuration.
    Gfx_SetupFrame(this->state.gfxCtx, 0, 0, 0);
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
    this->state.running = false;
    SET_NEXT_GAMESTATE(&this->state, Opening_Init, OpeningContext);
}

void Title_Destroy(GameState* thisx) {
    TitleContext* this = (TitleContext*)thisx;

    Sram_InitSram(&this->state);
}

void Title_Init(GameState* thisx) {
    TitleContext* this = (TitleContext*)thisx;

    osSyncPrintf("z_title.c\n");

    R_UPDATE_RATE = 1;
    Matrix_Init(&this->state);
    View_Init(&this->view, this->state.gfxCtx);
    this->state.main = Title_Main;
    this->state.destroy = Title_Destroy;
    gSaveContext.fileNum = 0xFF;
}
