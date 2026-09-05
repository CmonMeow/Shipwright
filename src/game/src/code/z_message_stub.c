#include "global.h"

uint8_t Message_ShouldAdvance(PlayState* play) {
    (void)play;
    return false;
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

void func_8010BD58(PlayState* play, uint16_t textId) {
    Message_StartTextbox(play, textId, NULL);
}

uint8_t Message_GetState(MessageContext* msgCtx) {
    (void)msgCtx;
    return TEXT_STATE_NONE;
}
