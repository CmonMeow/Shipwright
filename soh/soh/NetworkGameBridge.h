#pragma once

struct PlayState;
struct Actor;

#ifdef __cplusplus
extern "C" {
#endif

void NetworkGame_Initialize(int argc, char* argv[]);
void NetworkGame_Shutdown(void);
void NetworkGame_UpdateTransport(void);
void NetworkGame_Update(struct PlayState* play);
int NetworkGame_IsObjectDestroyed(struct PlayState* play, struct Actor* actor);
void NetworkGame_NotifyObjectDestroyed(struct PlayState* play, struct Actor* actor);
void NetworkGame_NotifyObjectRestored(struct PlayState* play, struct Actor* actor);
void NetworkGame_NotifyObjectActivated(struct PlayState* play, struct Actor* actor);
int NetworkGame_ConsumeObjectActivation(struct PlayState* play, struct Actor* actor);
int NetworkGame_IsExplosionNear(struct PlayState* play, struct Actor* actor, float radius);

#ifdef __cplusplus
}

void NetworkGame_RegisterActors();
#endif
