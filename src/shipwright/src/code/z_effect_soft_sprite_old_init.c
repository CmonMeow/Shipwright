#include "global.h"

#include "overlays/effects/ovl_Effect_Ss_Bubble/z_eff_ss_bubble.h"
#include "overlays/effects/ovl_Effect_Ss_Dust/z_eff_ss_dust.h"
#include "overlays/effects/ovl_Effect_Ss_G_Ripple/z_eff_ss_g_ripple.h"
#include "overlays/effects/ovl_Effect_Ss_G_Splash/z_eff_ss_g_splash.h"
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"
#include "overlays/effects/ovl_Effect_Ss_Sibuki/z_eff_ss_sibuki.h"
#include "overlays/effects/ovl_Effect_Ss_Sibuki2/z_eff_ss_sibuki2.h"

void EffectSs_DrawGEffect(PlayState* play, EffectSs* this, void* texture) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    MtxF mfTrans;
    MtxF mfScale;
    MtxF mfResult;
    MtxF mfTrans11DA0;
    void* object = play->objectCtx.status[this->rgObjBankIdx].segment;

    OPEN_DISPS(gfxCtx);
    float scale = this->rgScale * 0.0025f;
    SkinMatrix_SetTranslate(&mfTrans, this->pos.x, this->pos.y, this->pos.z);
    SkinMatrix_SetScale(&mfScale, scale, scale, scale);
    SkinMatrix_MtxFMtxFMult(&mfTrans, &play->billboardMtxF, &mfTrans11DA0);
    SkinMatrix_MtxFMtxFMult(&mfTrans11DA0, &mfScale, &mfResult);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(object);
    gSPSegment(POLY_XLU_DISP++, 0x06, object);
    Mtx* mtx = SkinMatrix_MtxFToNewMtx(gfxCtx, &mfResult);
    if (mtx != NULL) {
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(texture));
        Gfx_SetupDL_61Xlu(gfxCtx);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, this->rgPrimColorR, this->rgPrimColorG, this->rgPrimColorB,
                        this->rgPrimColorA);
        gDPSetEnvColor(POLY_XLU_DISP++, this->rgEnvColorR, this->rgEnvColorG, this->rgEnvColorB, this->rgEnvColorA);
        gSPDisplayList(POLY_XLU_DISP++, this->gfx);
    }
    CLOSE_DISPS(gfxCtx);
}

void EffectSsDust_Spawn(PlayState* play, uint16_t drawFlags, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                        Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t scale, int16_t scaleStep, int16_t life,
                        uint8_t updateMode) {
    EffectSsDustInitParams initParams;

    Math_Vec3f_Copy(&initParams.pos, pos);
    Math_Vec3f_Copy(&initParams.velocity, velocity);
    Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.primColor = *primColor;
    initParams.envColor = *envColor;
    initParams.drawFlags = drawFlags;
    initParams.scale = scale;
    initParams.scaleStep = scaleStep;
    initParams.life = life;
    initParams.updateMode = updateMode;
    EffectSs_Spawn(play, EFFECT_SS_DUST, 128, &initParams);
}

void func_8002836C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep, int16_t life) {
    EffectSsDust_Spawn(play, 0, pos, velocity, accel, primColor, envColor, scale, scaleStep, life, 0);
}

static Color_RGBA8 sDustBrownPrim = { 170, 130, 90, 255 };
static Color_RGBA8 sDustBrownEnv = { 100, 60, 20, 255 };

void func_8002857C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel) {
    EffectSsDust_Spawn(play, 4, pos, velocity, accel, &sDustBrownPrim, &sDustBrownEnv, 100, 5, 10, 0);
}

void func_8002865C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep) {
    EffectSsDust_Spawn(play, 4, pos, velocity, accel, &sDustBrownPrim, &sDustBrownEnv, scale, scaleStep, 10, 0);
}

void func_800286CC(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep) {
    EffectSsDust_Spawn(play, 5, pos, velocity, accel, &sDustBrownPrim, &sDustBrownEnv, scale, scaleStep, 10, 0);
}

void EffectSsBubble_Spawn(PlayState* play, Vec3f* pos, float yPosOffset, float yPosRandScale, float xzPosRandScale,
                          float scale) {
    EffectSsBubbleInitParams initParams;

    Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.yPosOffset = yPosOffset;
    initParams.yPosRandScale = yPosRandScale;
    initParams.xzPosRandScale = xzPosRandScale;
    initParams.scale = scale;
    EffectSs_Spawn(play, EFFECT_SS_BUBBLE, 128, &initParams);
}

void EffectSsGRipple_Spawn(PlayState* play, Vec3f* pos, int16_t radius, int16_t radiusMax, int16_t life) {
    EffectSsGRippleInitParams initParams;

    Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.radius = radius;
    initParams.radiusMax = radiusMax;
    initParams.life = life;
    EffectSs_Spawn(play, EFFECT_SS_G_RIPPLE, 128, &initParams);
}

void EffectSsGSplash_Spawn(PlayState* play, Vec3f* pos, Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t type,
                           int16_t scale) {
    EffectSsGSplashInitParams initParams;

    Math_Vec3f_Copy(&initParams.pos, pos);
    initParams.type = type;
    initParams.scale = scale;
    if (primColor != NULL) {
        initParams.primColor = *primColor;
        initParams.envColor = *envColor;
        initParams.customColor = true;
    } else {
        initParams.customColor = false;
    }
    EffectSs_Spawn(play, EFFECT_SS_G_SPLASH, 128, &initParams);
}

void EffectSsSibuki_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t moveDelay, int16_t direction,
                          int16_t scale) {
    EffectSsSibukiInitParams initParams;

    Math_Vec3f_Copy(&initParams.pos, pos);
    Math_Vec3f_Copy(&initParams.velocity, velocity);
    Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.moveDelay = moveDelay;
    initParams.direction = direction;
    initParams.scale = scale;
    EffectSs_Spawn(play, EFFECT_SS_SIBUKI, 128, &initParams);
}

void EffectSsSibuki_SpawnBurst(PlayState* play, Vec3f* pos) {
    int16_t i;
    Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    int16_t randDirection = Rand_ZeroOne() * 1.99f;

    for (i = 0; i < KREG(19) + 30; i++) {
        EffectSsSibuki_Spawn(play, pos, &zeroVec, &zeroVec, i / (KREG(27) + 6), randDirection, KREG(18) + 40);
    }
}

void EffectSsSibuki2_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale) {
    EffectSsSibuki2InitParams initParams;

    Math_Vec3f_Copy(&initParams.pos, pos);
    Math_Vec3f_Copy(&initParams.velocity, velocity);
    Math_Vec3f_Copy(&initParams.accel, accel);
    initParams.scale = scale;
    EffectSs_Spawn(play, EFFECT_SS_SIBUKI2, 128, &initParams);
}

void EffectSsHitMark_Spawn(PlayState* play, int32_t type, int16_t scale, Vec3f* pos) {
    EffectSsHitMarkInitParams initParams;

    initParams.type = type;
    initParams.scale = scale;
    Math_Vec3f_Copy(&initParams.pos, pos);
    EffectSs_Spawn(play, EFFECT_SS_HITMARK, 128, &initParams);
}

void EffectSsHitMark_SpawnFixedScale(PlayState* play, int32_t type, Vec3f* pos) {
    EffectSsHitMark_Spawn(play, type, 300, pos);
}

void EffectSsHitMark_SpawnCustomScale(PlayState* play, int32_t type, int16_t scale, Vec3f* pos) {
    EffectSsHitMark_Spawn(play, type, scale, pos);
}
