#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t PCInput_ConsumeMouseAimDelta(int32_t* deltaX, int32_t* deltaY);
int32_t PCInput_ConsumeToggleWeapon(void);
int32_t PCInput_ConsumeEvade(void);
int32_t PCInput_EvadeRequestedThisSample(void);
void PCInput_DiscardActionIntents(void);
int32_t PCInput_IsFishingReelHeld(void);
int32_t PCInput_IsShieldHeld(void);
int32_t PCInput_GetSelectedWeaponSlot(void);
int32_t PCInput_ConsumeWeaponSelection(void);
int32_t PCInput_IsBowAimHeld(void);
void PCInput_ConsumeBowUseIntent(void);
int32_t PCInput_HasBowUseIntent(void);

#ifdef __cplusplus
}
#endif
