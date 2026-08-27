#include "global.h"

static s16 sRespawnFreezeTimer;

void GameOver_Init(PlayState* play) {
    play->gameOverCtx.state = GAMEOVER_INACTIVE;
}

void GameOver_Update(PlayState* play) {
    switch (play->gameOverCtx.state) {
        case GAMEOVER_DEATH_START:
            Message_CloseTextbox(play);
            sRespawnFreezeTimer = 20;
            play->gameOverCtx.state = GAMEOVER_DEATH_WAIT_GROUND;
            break;

        case GAMEOVER_DEATH_DELAY_MENU:
            if (sRespawnFreezeTimer > 0) {
                sRespawnFreezeTimer--;
            }
            if (sRespawnFreezeTimer == 0) {
                play->gameOverCtx.state = GAMEOVER_DEATH_WAIT_RESPAWN;
            }
            break;

        case GAMEOVER_INACTIVE:
        case GAMEOVER_DEATH_WAIT_GROUND:
        case GAMEOVER_DEATH_WAIT_RESPAWN:
            break;
    }
}
