#include "GameplayNotification.h"

namespace {

GameplayNotificationSink sSink{};

} // namespace

extern "C" void GameplayNotification_SetSink(
    const GameplayNotificationSink* sink) {
    sSink = sink ? *sink : GameplayNotificationSink{};
}

extern "C" void GameplayNotification_ClearSink(void* context) {
    if (context != sSink.context) return;
    sSink = {};
}

extern "C" void GameplayNotification_Show(const char* text) {
    if (sSink.show) sSink.show(text, sSink.context);
}

extern "C" void GameplayNotification_Clear(void) {
    if (sSink.clear) sSink.clear(sSink.context);
}
