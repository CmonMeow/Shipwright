#include "global.h"

uint8_t Message_ShouldAdvance(PlayState* play) {
    return CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A);
}

uint8_t Message_ShouldAdvanceSilent(PlayState* play) {
    return Message_ShouldAdvance(play);
}

void Message_CloseTextbox(PlayState* play) {
    play->msgCtx.msgLength = 0;
    play->msgCtx.msgMode = MSGMODE_NONE;
}

void Message_StartTextbox(PlayState* play, uint16_t textId, Actor* actor) {
    (void)textId;
    (void)actor;
    Message_CloseTextbox(play);
}

void Message_ContinueTextbox(PlayState* play, uint16_t textId) {
    Message_StartTextbox(play, textId, NULL);
}

void Message_OpenText(PlayState* play, uint16_t textId) {
    Message_StartTextbox(play, textId, NULL);
}

void func_8010BD58(PlayState* play, uint16_t textId) {
    Message_StartTextbox(play, textId, NULL);
}

void func_8010BD88(PlayState* play, uint16_t textId) {
    Message_StartTextbox(play, textId, NULL);
}

uint8_t Message_GetState(MessageContext* msgCtx) {
    (void)msgCtx;
    return TEXT_STATE_NONE;
}

void Message_UpdateOcarinaGame(PlayState* play) {
    (void)play;
}

void Message_Update(PlayState* play) {
    (void)play;
}

void Message_Draw(PlayState* play) {
    (void)play;
}

void Message_Decode(PlayState* play) {
    (void)play;
}

void Message_DrawText(PlayState* play, Gfx** gfxP) {
    (void)play;
    (void)gfxP;
}

void Message_SetTables(void) {
}
