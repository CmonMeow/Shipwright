#include "global.h"
#include "assets/scenes/test_levels/test01/test01_room_0.h"

#define TEST01_ENTRANCE_FIELD                                                                    \
    (ENTRANCE_INFO_DISPLAY_TITLE_CARD_FLAG |                                                     \
     ((TRANS_TYPE_FADE_BLACK << ENTRANCE_INFO_END_TRANS_TYPE_SHIFT) &                           \
      ENTRANCE_INFO_END_TRANS_TYPE_MASK) |                                                       \
     ((TRANS_TYPE_FADE_BLACK << ENTRANCE_INFO_START_TRANS_TYPE_SHIFT) &                         \
      ENTRANCE_INFO_START_TRANS_TYPE_MASK))

// Keep the original numeric entrance IDs because save, respawn, and network packets
// carry them. Every valid entrance in the reduced runtime resolves to test01.
EntranceInfo gEntranceTable[ENTR_MAX] = {
    [ENTR_TEST01_0] = { SCENE_TEST01, 0, TEST01_ENTRANCE_FIELD },
    [ENTR_TEST01_0_1] = { SCENE_TEST01, 0, TEST01_ENTRANCE_FIELD },
    [ENTR_TEST01_0_2] = { SCENE_TEST01, 0, TEST01_ENTRANCE_FIELD },
    [ENTR_TEST01_0_3] = { SCENE_TEST01, 0, TEST01_ENTRANCE_FIELD },
};

// Scene IDs are likewise kept stable for protocol compatibility. Only test01 has
// a resource path and can be loaded.
SceneTableEntry gSceneTable[SCENE_ID_MAX] = {
    [SCENE_TEST01] = { { 0, 0, "test01_scene" }, { 0, 0, "" }, 0, SDC_CALM_WATER, 0, 0 },
};

void Scene_SetTransitionForNextEntrance(PlayState* play) {
    play->nextEntranceIndex = ENTR_TEST01_0;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
}

void Scene_PrepareWater(PlayState* play) {
    u32 gameplayFrames;
    Gfx* waterScroll;

    OPEN_DISPS(play->state.gfxCtx);

    gameplayFrames = play->gameplayFrames;
    // test01's native water material calls display list 0x08000001. Keep
    // segment 8 populated even though the added top-facing WaterBox plane
    // binds the room texture directly.
    waterScroll = Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 127 - gameplayFrames % 128,
                                     gameplayFrames % 128, 32, 32, 1, gameplayFrames % 128,
                                     gameplayFrames % 128, 32, 32, -1, 1, 1, 1);
    gSPSegment(POLY_OPA_DISP++, 0x08, waterScroll);
    gSPSegment(POLY_XLU_DISP++, 0x08, waterScroll);

    CLOSE_DISPS(play->state.gfxCtx);
}

static void Scene_DrawCalmWater(PlayState* play) {
    CollisionHeader* colHeader = play->colCtx.colHeader;
    u32 gameplayFrames;
    Vtx* waterVertices;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gameplayFrames = play->gameplayFrames;

    if ((colHeader != NULL) && (colHeader->numWaterBoxes > 0)) {
        waterVertices = Graph_Alloc(play->state.gfxCtx, colHeader->numWaterBoxes * 4 * sizeof(Vtx));
        if (waterVertices != NULL) {
            Gfx_SetupDL_25Xlu(play->state.gfxCtx);
            gSPMatrix(POLY_XLU_DISP++, &gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPClearGeometryMode(POLY_XLU_DISP++, G_CULL_BOTH | G_LIGHTING);
            gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
            gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 210, 235, 255, 150);
            gDPLoadTextureBlock(POLY_XLU_DISP++, test01_room_0Tex_0090E8, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0,
                                G_TX_WRAP, G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);

            for (i = 0; i < colHeader->numWaterBoxes; i++) {
                WaterBox* waterBox = &colHeader->waterBoxes[i];
                Vtx* vtx = &waterVertices[i * 4];
                s32 room = WATERBOX_ROOM(waterBox->properties);
                s32 j;

                if ((waterBox->properties & 0x80000) ||
                    ((room != 0x3F) && (room != play->roomCtx.curRoom.num))) {
                    continue;
                }

                vtx[0].v.ob[0] = waterBox->xMin;
                vtx[0].v.ob[1] = waterBox->ySurface;
                vtx[0].v.ob[2] = waterBox->zMin;
                vtx[1].v.ob[0] = waterBox->xMin + waterBox->xLength;
                vtx[1].v.ob[1] = waterBox->ySurface;
                vtx[1].v.ob[2] = waterBox->zMin;
                vtx[2].v.ob[0] = waterBox->xMin;
                vtx[2].v.ob[1] = waterBox->ySurface;
                vtx[2].v.ob[2] = waterBox->zMin + waterBox->zLength;
                vtx[3].v.ob[0] = waterBox->xMin + waterBox->xLength;
                vtx[3].v.ob[1] = waterBox->ySurface;
                vtx[3].v.ob[2] = waterBox->zMin + waterBox->zLength;

                for (j = 0; j < 4; j++) {
                    vtx[j].v.flag = 0;
                    vtx[j].v.tc[0] = (s16)((vtx[j].v.ob[0] * 4) + (gameplayFrames * 2));
                    vtx[j].v.tc[1] = (s16)((vtx[j].v.ob[2] * 4) + gameplayFrames);
                    vtx[j].v.cn[0] = 255;
                    vtx[j].v.cn[1] = 255;
                    vtx[j].v.cn[2] = 255;
                    vtx[j].v.cn[3] = 184;
                }

                gSPVertex(POLY_XLU_DISP++, vtx, 4, 0);
                // Wind the replacement surface upward. The original order faces
                // down, which leaves the water visible only from below.
                gSP2Triangles(POLY_XLU_DISP++, 0, 2, 1, 0, 1, 2, 3, 0);
            }

        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Scene_Draw(PlayState* play) {
    Scene_DrawCalmWater(play);
}

#undef TEST01_ENTRANCE_FIELD
