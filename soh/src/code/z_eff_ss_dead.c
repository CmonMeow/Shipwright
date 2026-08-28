#include "global.h"

void func_80026230(PlayState* play, Color_RGBA8* color, s16 arg2, s16 arg3) {

    OPEN_DISPS(play->state.gfxCtx);

    Gfx* displayListHead = POLY_OPA_DISP;
    f32 cos = Math_CosS((0x8000 / arg3) * arg2);
    f32 absCos = ABS(cos);

    gDPPipeSync(displayListHead++);

    if (color == NULL) {
        gDPSetFogColor(displayListHead++, 255, 0, 0, 0);
    } else {
        gDPSetFogColor(displayListHead++, color->r, color->g, color->b, color->a);
    }

    gSPFogPosition(displayListHead++, 0, (s16)(absCos * 3000.0f) + 1500);

    POLY_OPA_DISP = displayListHead;

    CLOSE_DISPS(play->state.gfxCtx);
}

void func_80026400(PlayState* play, Color_RGBA8* color, s16 arg2, s16 arg3) {

    if (arg3 != 0) {
        OPEN_DISPS(play->state.gfxCtx);

        f32 cos = Math_CosS((0x4000 / arg3) * arg2);
        Gfx* displayListHead = POLY_OPA_DISP;

        gDPPipeSync(displayListHead++);
        gDPSetFogColor(displayListHead++, color->r, color->g, color->b, color->a);
        gSPFogPosition(displayListHead++, 0, (s16)(2800.0f * ABS(cos)) + 1700);

        POLY_OPA_DISP = displayListHead;

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void func_80026608(PlayState* play) {

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

void func_80026690(PlayState* play, Color_RGBA8* color, s16 arg2, s16 arg3) {

    OPEN_DISPS(play->state.gfxCtx);

    Gfx* displayListHead = POLY_XLU_DISP;
    f32 cos = Math_CosS((0x8000 / arg3) * arg2);
    f32 absCos = ABS(cos);

    gDPPipeSync(displayListHead++);

    if (color == NULL) {
        gDPSetFogColor(displayListHead++, 255, 0, 0, 0);
    } else {
        gDPSetFogColor(displayListHead++, color->r, color->g, color->b, color->a);
    }

    gSPFogPosition(displayListHead++, 0, (s16)(absCos * 3000.0f) + 1500);

    POLY_XLU_DISP = displayListHead;

    CLOSE_DISPS(play->state.gfxCtx);
}

void func_80026860(PlayState* play, Color_RGBA8* color, s16 arg2, s16 arg3) {

    OPEN_DISPS(play->state.gfxCtx);

    Gfx* displayListHead = POLY_XLU_DISP;
    f32 cos = Math_CosS((0x4000 / arg3) * arg2);

    gDPPipeSync(displayListHead++);
    gDPSetFogColor(displayListHead++, color->r, color->g, color->b, color->a);
    gSPFogPosition(displayListHead++, 0, (s16)(2800.0f * ABS(cos)) + 1700);

    POLY_XLU_DISP = displayListHead;

    CLOSE_DISPS(play->state.gfxCtx);
}

void func_80026A6C(PlayState* play) {

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_XLU_DISP++);
    POLY_XLU_DISP = Play_SetFog(play, POLY_XLU_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}
