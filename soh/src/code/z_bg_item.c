#include "global.h"

void DynaPolyActor_Init(DynaPolyActor* dynaActor, int32_t flags) {
    dynaActor->bgId = -1;
    dynaActor->transformFlags = flags;
    dynaActor->interactFlags = 0;
    dynaActor->unk_150 = 0.0f;
    dynaActor->unk_154 = 0.0f;
}

void DynaPolyActor_UnsetAllInteractFlags(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags = 0;
}

void DynaPolyActor_SetActorOnTop(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_ACTOR_ON_TOP;
}

void DynaPolyActor_SetPlayerOnTop(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_PLAYER_ON_TOP;
}

void DynaPoly_SetPlayerOnTop(CollisionContext* colCtx, int32_t floorBgId) {
    DynaPolyActor* dynaActor = DynaPoly_GetActor(colCtx, floorBgId);

    if (dynaActor != NULL) {
        DynaPolyActor_SetPlayerOnTop(dynaActor);
    }
}

void DynaPolyActor_SetPlayerAbove(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_PLAYER_ABOVE;
}

void DynaPoly_SetPlayerAbove(CollisionContext* colCtx, int32_t floorBgId) {
    DynaPolyActor* dynaActor = DynaPoly_GetActor(colCtx, floorBgId);

    if (dynaActor != NULL) {
        DynaPolyActor_SetPlayerAbove(dynaActor);
    }
}

void DynaPolyActor_SetSwitchPressed(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_ACTOR_SWITCH_PRESSED;
}

int32_t DynaPolyActor_IsActorOnTop(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_ACTOR_ON_TOP) {
        return true;
    } else {
        return false;
    }
}

int32_t DynaPolyActor_IsPlayerOnTop(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_PLAYER_ON_TOP) {
        return true;
    } else {
        return false;
    }
}

int32_t DynaPolyActor_IsPlayerAbove(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_PLAYER_ABOVE) {
        return true;
    } else {
        return false;
    }
}

int32_t DynaPolyActor_IsSwitchPressed(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_ACTOR_SWITCH_PRESSED) {
        return true;
    } else {
        return false;
    }
}

int32_t func_800435D8(PlayState* play, DynaPolyActor* dynaActor, int16_t arg2, int16_t arg3, int16_t arg4) {
    Vec3f posA;
    Vec3f posB;
    Vec3f posResult;
    float sin = Math_SinS(dynaActor->unk_158);
    float cos = Math_CosS(dynaActor->unk_158);
    int32_t bgId;
    CollisionPoly* poly;
    float a2 = { 0 };
    float a3 = { 0 };
    float sign = (0.0f <= dynaActor->unk_150) ? 1.0f : -1.0f;

    a2 = (float)arg2 - 0.1f;
    posA.x = dynaActor->actor.world.pos.x + (a2 * cos);
    posA.y = dynaActor->actor.world.pos.y + arg4;
    posA.z = dynaActor->actor.world.pos.z - (a2 * sin);

    a3 = (float)arg3 - 0.1f;
    posB.x = sign * a3 * sin + posA.x;
    posB.y = posA.y;
    posB.z = sign * a3 * cos + posA.z;

    if (BgCheck_EntityLineTest3(&play->colCtx, &posA, &posB, &posResult, &poly, true, false, false, true, &bgId,
                                &dynaActor->actor, 0.0f)) {
        return false;
    }
    posA.x = (dynaActor->actor.world.pos.x * 2) - posA.x;
    posA.z = (dynaActor->actor.world.pos.z * 2) - posA.z;
    posB.x = sign * a3 * sin + posA.x;
    posB.z = sign * a3 * cos + posA.z;
    if (BgCheck_EntityLineTest3(&play->colCtx, &posA, &posB, &posResult, &poly, true, false, false, true, &bgId,
                                &dynaActor->actor, 0.0f)) {
        return false;
    }
    return true;
}
