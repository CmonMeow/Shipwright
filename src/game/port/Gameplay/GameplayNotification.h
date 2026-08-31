#pragma once

typedef struct GameplayNotificationSink {
    void (*show)(const char* text, void* context);
    void (*clear)(void* context);
    void* context;
} GameplayNotificationSink;

#ifdef __cplusplus
extern "C" {
#endif

void GameplayNotification_SetSink(const GameplayNotificationSink* sink);
void GameplayNotification_ClearSink(void* context);
void GameplayNotification_Show(const char* text);
void GameplayNotification_Clear(void);

#ifdef __cplusplus
}
#endif
