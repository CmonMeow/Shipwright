#pragma once

struct PlayState;

#ifdef __cplusplus
extern "C" {
#endif

// Application lifecycle boundary for the native client. Retained Ocarina C
// code schedules the client runtime without knowing about transports, packets,
// replicas, or the concrete multiplayer session composition.
void ClientRuntime_RegisterActors(void);
void ClientRuntime_Initialize(int argc, char* argv[]);
void ClientRuntime_Shutdown(void);
void ClientRuntime_UpdateTransport(void);
void ClientRuntime_UpdateGameplay(struct PlayState* play);

#ifdef __cplusplus
}
#endif
