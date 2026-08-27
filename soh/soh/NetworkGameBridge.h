#pragma once

struct PlayState;
struct Actor;

enum {
    NETWORK_GAME_ACTOR_EVENT_GRASS_CUT = 1,
    NETWORK_GAME_ACTOR_EVENT_BOULDER_BREAK = 2,
    NETWORK_GAME_ACTOR_EVENT_OWL_DEPART = 3,
    NETWORK_GAME_ACTOR_EVENT_FISH_HOOK = 4,
    NETWORK_GAME_ACTOR_EVENT_FISH_RELEASE = 5,
    NETWORK_GAME_ACTOR_EVENT_GRASS_THROWN_BREAK = 6,
};

#ifdef __cplusplus
extern "C" {
#endif

void NetworkGame_Initialize(int argc, char* argv[]);
int NetworkGame_ShouldAutoStartTest(void);
void NetworkGame_Shutdown(void);
void NetworkGame_UpdateTransport(void);
void NetworkGame_Update(struct PlayState* play);
void NetworkGame_ShowNotification(const char* text);
void NetworkGame_ClearNotification(void);
int NetworkGame_IsObjectDestroyed(struct PlayState* play, struct Actor* actor);
void NetworkGame_NotifyActorEvent(struct PlayState* play, struct Actor* actor, unsigned char eventType);
int NetworkGame_ConsumeActorEvent(struct PlayState* play, struct Actor* actor, unsigned char eventType);
int NetworkGame_ConsumeActorEventSource(struct PlayState* play, struct Actor* actor, unsigned char eventType,
                                        int* sourcePlayerId);
int NetworkGame_GetRemoteFishingFishState(int playerId, float* x, float* y, float* z, short* rotationX,
                                          short* rotationY, short* rotationZ, short limbRot[8], float* length,
                                          unsigned char* isLoach);

#ifdef __cplusplus
}

void NetworkGame_RegisterActors();
#endif
