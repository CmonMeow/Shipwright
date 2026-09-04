#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void InitColViewer(void);
void ToggleColViewer(void);
void DrawColViewer(void);
void SetLocalCollisionPlayerId(int32_t playerId);
void BeginLocalRenderedPlayerCollision(void);
void RecordLocalRenderedPlayerCollisionLimb(int32_t limbIndex, float x,
                                            float y, float z);
void EndLocalRenderedPlayerCollision(void);
void BeginRenderedPlayerCollision(int32_t playerId);
void RecordRenderedPlayerCollisionLimb(int32_t playerId, int32_t limbIndex,
                                       float x, float y, float z);
void EndRenderedPlayerCollision(int32_t playerId);

#ifdef __cplusplus
}

#include "platform/simulation/PlayerSimulation.h"

// Supplies the real PvP collision representation for the current frame. F1
// remains the one collision viewer; this is not a separate skeleton overlay.
void RecordAuthoritativePlayerCollision(
    int32_t playerId, const Game::Simulation::PlayerSnapshot& snapshot);
void RemoveAuthoritativePlayerCollision(int32_t playerId);
void ClearAuthoritativePlayerCollision(void);
#endif
