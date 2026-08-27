#include "global.h"

void TitleSetup_InitImpl(GameState* gameState) {
    s32 buttonIndex;

    SaveContext_Init();
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    gSaveContext.healthCapacity = STARTING_HEALTH;
    gSaveContext.health = STARTING_HEALTH;
    gSaveContext.inventory.equipment = 0x1100;
    gSaveContext.equips.equipment = 0x1100;
    for (buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.equips.buttonItems); ++buttonIndex) {
        gSaveContext.equips.buttonItems[buttonIndex] = ITEM_NONE;
    }
    for (buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.equips.cButtonSlots); ++buttonIndex) {
        gSaveContext.equips.cButtonSlots[buttonIndex] = SLOT_NONE;
    }
    for (buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.inventory.items); ++buttonIndex) {
        gSaveContext.inventory.items[buttonIndex] = ITEM_NONE;
    }
    for (buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.inventory.dungeonKeys); ++buttonIndex) {
        gSaveContext.inventory.dungeonKeys[buttonIndex] = 0xFF;
    }
    gSaveContext.equips.buttonItems[0] = ITEM_NONE;
    for (buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); ++buttonIndex) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }
    gSaveContext.buttonStatus[0] = BTN_DISABLED;
    gSaveContext.forceRisingButtonAlphas = 0;
    gSaveContext.unk_13E8 = 0;
    gSaveContext.unk_13EA = 0;
    gSaveContext.unk_13EC = 0;
    gSaveContext.entranceIndex = ENTR_TEST01_0;
    gSaveContext.savedSceneNum = SCENE_TEST01;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_TEST01_0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
    gSaveContext.showTitleCard = false;
    gWeatherMode = 0;
    gameState->running = false;
    SET_NEXT_GAMESTATE(gameState, Play_Init, PlayState);
}

void TitleSetup_Destroy(GameState* gameState) {
}

void TitleSetup_Init(GameState* gameState) {
    gameState->destroy = TitleSetup_Destroy;
    TitleSetup_InitImpl(gameState);
}
