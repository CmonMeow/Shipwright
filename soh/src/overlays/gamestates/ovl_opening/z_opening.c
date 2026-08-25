/*
 * File: z_opening.c
 * Overlay: ovl_opening
 * Description: Hands off from boot initialization to file select
 */

#include "global.h"

void Opening_SetupFileSelect(OpeningContext* this) {
    // The former horseback title sequence eventually made this same mode change.
    // Do it here without constructing a title-demo save or gameplay scene.
    gSaveContext.gameMode = GAMEMODE_FILE_SELECT;
    gSaveContext.fileNum = 0xFF;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.sceneSetupIndex = 0;
    this->state.running = false;
    SET_NEXT_GAMESTATE(&this->state, FileChoose_Init, FileChooseContext);
}

void Opening_Main(GameState* thisx) {
    OpeningContext* this = (OpeningContext*)thisx;

    Gfx_SetupFrame(this->state.gfxCtx, 0, 0, 0);
    Opening_SetupFileSelect(this);
}

void Opening_Destroy(GameState* thisx) {
}

void Opening_Init(GameState* thisx) {
    OpeningContext* this = (OpeningContext*)thisx;

    R_UPDATE_RATE = 1;
    Matrix_Init(&this->state);
    View_Init(&this->view, this->state.gfxCtx);
    this->state.main = Opening_Main;
    this->state.destroy = Opening_Destroy;
}
