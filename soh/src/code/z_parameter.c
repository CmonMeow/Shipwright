#include "global.h"
#include "vt.h"
#include "textures/parameter_static/parameter_static.h"
#include "textures/do_action_static/do_action_static.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "soh_assets.h"

#include "soh/ShipUtils.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"

#include "message_data_static.h"
extern MessageTableEntry* sNesMessageEntryTablePtr;
extern MessageTableEntry* sGerMessageEntryTablePtr;
extern MessageTableEntry* sFraMessageEntryTablePtr;
extern MessageTableEntry* sJpnMessageEntryTablePtr;

#define DO_ACTION_TEX_WIDTH() 48
#define DO_ACTION_TEX_HEIGHT() 16
#define DO_ACTION_TEX_SIZE() ((DO_ACTION_TEX_WIDTH() * DO_ACTION_TEX_HEIGHT()) / 2)

// The button statuses include the A button when most things are only the equip item buttons
// So, when indexing into it with a item button index, we need to adjust
#define BUTTON_STATUS_INDEX(button) ((button) >= 4) ? ((button) + 1) : (button)

s16 Top_HUD_Margin = 0;
s16 Left_HUD_Margin = 0;
s16 Right_HUD_Margin = 0;
s16 Bottom_HUD_Margin = 0;

typedef struct {
    /* 0x00 */ u8 scene;
    /* 0x01 */ u8 flags1;
    /* 0x02 */ u8 flags2;
    /* 0x03 */ u8 flags3;
} RestrictionFlags;

static RestrictionFlags sRestrictionFlags[] = {
    { SCENE_HYRULE_FIELD, 0x00, 0x00, 0x10 },
    { SCENE_KAKARIKO_VILLAGE, 0x00, 0x00, 0x10 },
    { SCENE_GRAVEYARD, 0x00, 0x00, 0x10 },
    { SCENE_ZORAS_RIVER, 0x00, 0x00, 0x10 },
    { SCENE_KOKIRI_FOREST, 0x00, 0x00, 0x10 },
    { SCENE_SACRED_FOREST_MEADOW, 0x00, 0x00, 0x10 },
    { SCENE_LAKE_HYLIA, 0x00, 0x00, 0x10 },
    { SCENE_ZORAS_DOMAIN, 0x00, 0x00, 0x10 },
    { SCENE_ZORAS_FOUNTAIN, 0x00, 0x00, 0x10 },
    { SCENE_GERUDO_VALLEY, 0x00, 0x00, 0x10 },
    { SCENE_LOST_WOODS, 0x00, 0x00, 0x10 },
    { SCENE_DESERT_COLOSSUS, 0x00, 0x00, 0x10 },
    { SCENE_GERUDOS_FORTRESS, 0x00, 0x00, 0x10 },
    { SCENE_HAUNTED_WASTELAND, 0x00, 0x00, 0x10 },
    { SCENE_HYRULE_CASTLE, 0x00, 0x00, 0x10 },
    { SCENE_OUTSIDE_GANONS_CASTLE, 0x00, 0x00, 0x10 },
    { SCENE_DEATH_MOUNTAIN_TRAIL, 0x00, 0x00, 0x10 },
    { SCENE_DEATH_MOUNTAIN_CRATER, 0x00, 0x00, 0x10 },
    { SCENE_GORON_CITY, 0x00, 0x00, 0x10 },
    { SCENE_LON_LON_RANCH, 0x00, 0x00, 0x10 },
    { SCENE_TEMPLE_OF_TIME, 0x00, 0x10, 0x15 },
    { SCENE_CHAMBER_OF_THE_SAGES, 0xA2, 0xAA, 0xAA },
    { SCENE_SHOOTING_GALLERY, 0x11, 0x55, 0x55 },
    { SCENE_CASTLE_COURTYARD_GUARDS_DAY, 0x11, 0x55, 0x55 },
    { SCENE_CASTLE_COURTYARD_GUARDS_NIGHT, 0x11, 0x55, 0x55 },
    { SCENE_REDEAD_GRAVE, 0x00, 0x00, 0xD0 },
    { SCENE_GRAVE_WITH_FAIRYS_FOUNTAIN, 0x00, 0x00, 0xD0 },
    { SCENE_ROYAL_FAMILYS_TOMB, 0x00, 0x00, 0xD0 },
    { SCENE_GREAT_FAIRYS_FOUNTAIN_MAGIC, 0x00, 0x00, 0x10 },
    { SCENE_FAIRYS_FOUNTAIN, 0x00, 0x00, 0xD0 },
    { SCENE_GREAT_FAIRYS_FOUNTAIN_SPELLS, 0x00, 0x00, 0x10 },
    { SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR, 0x00, 0x05, 0x50 },
    { SCENE_CASTLE_COURTYARD_ZELDA, 0x00, 0x05, 0x54 },
    { SCENE_FISHING_POND, 0x11, 0x55, 0x55 },
    { SCENE_BOMBCHU_BOWLING_ALLEY, 0x11, 0x55, 0x55 },
    { SCENE_LON_LON_BUILDINGS, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_GUARD_HOUSE, 0x00, 0x10, 0x14 },
    { SCENE_POTION_SHOP_GRANNY, 0x10, 0x15, 0x55 },
    { SCENE_TREASURE_BOX_SHOP, 0x10, 0x15, 0x55 },
    { SCENE_HOUSE_OF_SKULLTULA, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_ENTRANCE_DAY, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_ENTRANCE_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_ENTRANCE_RUINS, 0x00, 0x10, 0xD5 },
    { SCENE_MARKET_DAY, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_MARKET_RUINS, 0x00, 0x10, 0xD5 },
    { SCENE_BACK_ALLEY_DAY, 0x00, 0x10, 0x15 },
    { SCENE_BACK_ALLEY_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_TEMPLE_OF_TIME_EXTERIOR_DAY, 0x00, 0x10, 0x15 },
    { SCENE_TEMPLE_OF_TIME_EXTERIOR_NIGHT, 0x00, 0x10, 0x15 },
    { SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS, 0x00, 0x10, 0xD5 },
    { SCENE_LINKS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_KAKARIKO_CENTER_GUEST_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_BACK_ALLEY_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_KNOW_IT_ALL_BROS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_TWINS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_MIDOS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_SARIAS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_STABLE, 0x10, 0x10, 0x15 },
    { SCENE_GRAVEKEEPERS_HUT, 0x10, 0x10, 0x15 },
    { SCENE_DOG_LADY_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_IMPAS_HOUSE, 0x10, 0x10, 0x15 },
    { SCENE_LAKESIDE_LABORATORY, 0x00, 0x10, 0x15 },
    { SCENE_CARPENTERS_TENT, 0x10, 0x10, 0x15 },
    { SCENE_BAZAAR, 0x10, 0x10, 0x15 },
    { SCENE_KOKIRI_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_GORON_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_ZORA_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_POTION_SHOP_KAKARIKO, 0x10, 0x10, 0x15 },
    { SCENE_POTION_SHOP_MARKET, 0x10, 0x10, 0x15 },
    { SCENE_BOMBCHU_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_HAPPY_MASK_SHOP, 0x10, 0x10, 0x15 },
    { SCENE_GERUDO_TRAINING_GROUND, 0x00, 0x03, 0x10 },
    { SCENE_DEKU_TREE, 0x00, 0x00, 0x00 },
    { SCENE_DEKU_TREE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_DODONGOS_CAVERN, 0x00, 0x00, 0x00 },
    { SCENE_DODONGOS_CAVERN_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_JABU_JABU, 0x00, 0x00, 0x00 },
    { SCENE_JABU_JABU_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_FOREST_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_FOREST_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_BOTTOM_OF_THE_WELL, 0x00, 0x00, 0x00 },
    { SCENE_SHADOW_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_SHADOW_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_FIRE_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_FIRE_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_WATER_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_WATER_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_SPIRIT_TEMPLE, 0x00, 0x00, 0x00 },
    { SCENE_SPIRIT_TEMPLE_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_GANONS_TOWER, 0x00, 0x00, 0x00 },
    { SCENE_GANONDORF_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_ICE_CAVERN, 0x00, 0x00, 0xC0 },
    { SCENE_WINDMILL_AND_DAMPES_GRAVE, 0x00, 0x03, 0x14 },
    { SCENE_INSIDE_GANONS_CASTLE, 0x00, 0x03, 0x10 },
    { SCENE_GANON_BOSS, 0x00, 0x45, 0x50 },
    { SCENE_GANONS_TOWER_COLLAPSE_INTERIOR, 0x00, 0x05, 0x50 },
    { SCENE_INSIDE_GANONS_CASTLE_COLLAPSE, 0x00, 0x05, 0x50 },
    { SCENE_THIEVES_HIDEOUT, 0x00, 0x00, 0x10 },
    { SCENE_GROTTOS, 0x00, 0x00, 0xD0 },
    { 0xFF, 0x00, 0x00, 0x00 },
};

static s16 sHBAScoreTier = 0;
static u16 sHBAScoreDigits[] = { 0, 0, 0, 0 };

static u16 sCUpInvisible = 0;
static u16 sCUpTimer = 0;

s16 gSpoilingItems[] = { ITEM_ODD_MUSHROOM, ITEM_FROG, ITEM_EYEDROPS };
s16 gSpoilingItemReverts[] = { ITEM_COJIRO, ITEM_PRESCRIPTION, ITEM_PRESCRIPTION };

static Color_RGB8 sMagicBorder = { 255, 255, 255 };
static Color_RGB8 sMagicBorder_ori = { 255, 255, 255 };

static s16 sExtraItemBases[] = {
    ITEM_STICK, ITEM_STICK, ITEM_NUT,   ITEM_NUT,     ITEM_BOMB,    ITEM_BOMB,  ITEM_BOMB,  ITEM_BOMB, ITEM_BOW,
    ITEM_BOW,   ITEM_BOW,   ITEM_SEEDS, ITEM_BOMBCHU, ITEM_BOMBCHU, ITEM_STICK, ITEM_STICK, ITEM_NUT,  ITEM_NUT,
};

static s16 sEnvHazard = PLAYER_ENV_HAZARD_NONE;
static s16 sEnvHazardActive = false;

static Gfx sSetupDL_80125A60[] = {
    gsDPPipeSync(),
    gsSPClearGeometryMode(G_ZBUFFER | G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN |
                          G_TEXTURE_GEN_LINEAR | G_SHADING_SMOOTH | G_LOD),
    gsDPSetOtherMode(G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                         G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_1PRIMITIVE,
                     G_AC_NONE | G_ZS_PIXEL | G_RM_CLD_SURF | G_RM_CLD_SURF2),
    gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE),
    gsSPEndDisplayList(),
};

static const char* actionsTbl[] = {
    gAttackDoActionENGTex, gCheckDoActionENGTex, gEnterDoActionENGTex,      gReturnDoActionENGTex,
    gOpenDoActionENGTex,   gJumpDoActionENGTex,  gDecideDoActionENGTex,     gDiveDoActionENGTex,
    gFasterDoActionENGTex, gThrowDoActionENGTex, gUnusedNaviDoActionENGTex, gClimbDoActionENGTex,
    gDropDoActionENGTex,   gDownDoActionENGTex,  gSaveDoActionENGTex,       gSpeakDoActionENGTex,
    gNextDoActionENGTex,   gGrabDoActionENGTex,  gStopDoActionENGTex,       gPutAwayDoActionENGTex,
    gReelDoActionENGTex,   gNum1DoActionENGTex,  gNum2DoActionENGTex,       gNum3DoActionENGTex,
    gNum4DoActionENGTex,   gNum5DoActionENGTex,  gNum6DoActionENGTex,       gNum7DoActionENGTex,
    gNum8DoActionENGTex,
};

// original name: "alpha_change"
void Interface_ChangeAlpha(u16 alphaType) {
    if (alphaType != gSaveContext.unk_13EA) {
        osSyncPrintf("ＡＬＰＨＡーＴＹＰＥ＝%d  LAST_TIME_TYPE=%d\n", alphaType, gSaveContext.unk_13EE);
        gSaveContext.unk_13EA = gSaveContext.unk_13E8 = alphaType;
        gSaveContext.unk_13EC = 1;
    }
}

void func_80082644(PlayState* play, s16 alpha) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (gSaveContext.buttonStatus[0] == BTN_DISABLED) {
        if (interfaceCtx->bAlpha != 70) {
            interfaceCtx->bAlpha = 70;
        }
    } else {
        if (interfaceCtx->bAlpha != 255) {
            interfaceCtx->bAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[1] == BTN_DISABLED) {
        if (interfaceCtx->cLeftAlpha != 70) {
            interfaceCtx->cLeftAlpha = 70;
        }
    } else {
        if (interfaceCtx->cLeftAlpha != 255) {
            interfaceCtx->cLeftAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[2] == BTN_DISABLED) {
        if (interfaceCtx->cDownAlpha != 70) {
            interfaceCtx->cDownAlpha = 70;
        }
    } else {
        if (interfaceCtx->cDownAlpha != 255) {
            interfaceCtx->cDownAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[3] == BTN_DISABLED) {
        if (interfaceCtx->cRightAlpha != 70) {
            interfaceCtx->cRightAlpha = 70;
        }
    } else {
        if (interfaceCtx->cRightAlpha != 255) {
            interfaceCtx->cRightAlpha = alpha;
        }
    }

    if (gSaveContext.buttonStatus[4] == BTN_DISABLED) {
        if (interfaceCtx->aAlpha != 70) {
            interfaceCtx->aAlpha = 70;
        }
    } else {
        if (interfaceCtx->aAlpha != 255) {
            interfaceCtx->aAlpha = alpha;
        }
    }

}

void func_8008277C(PlayState* play, s16 maxAlpha, s16 alpha) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (gSaveContext.forceRisingButtonAlphas != 0) {
        func_80082644(play, alpha);
        return;
    }

    if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
        interfaceCtx->bAlpha = maxAlpha;
    }

    if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
        interfaceCtx->aAlpha = maxAlpha;
    }

    if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
        interfaceCtx->cLeftAlpha = maxAlpha;
    }

    if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
        interfaceCtx->cDownAlpha = maxAlpha;
    }

    if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
        interfaceCtx->cRightAlpha = maxAlpha;
    }




}

void func_80082850(PlayState* play, s16 maxAlpha) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 alpha = 255 - maxAlpha;

    switch (gSaveContext.unk_13E8) {
        case 1:
        case 2:
        case 8:
            osSyncPrintf("a_alpha=%d, c_alpha=%d   →   ", interfaceCtx->aAlpha, interfaceCtx->cLeftAlpha);

            if (gSaveContext.unk_13E8 == 8) {
                if (interfaceCtx->bAlpha != 255) {
                    interfaceCtx->bAlpha = alpha;
                }
            } else {
                if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                    interfaceCtx->bAlpha = maxAlpha;
                }
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }





            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            osSyncPrintf("a_alpha=%d, c_alpha=%d\n", interfaceCtx->aAlpha, interfaceCtx->cLeftAlpha);

            break;
        case 3:
            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            func_8008277C(play, maxAlpha, alpha);

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            break;
        case 4:
            if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                interfaceCtx->bAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }





            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            break;
        case 5:
            func_8008277C(play, maxAlpha, alpha);

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
        case 6:
            func_8008277C(play, maxAlpha, alpha);

            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            switch (play->sceneNum) {
                case SCENE_HYRULE_FIELD:
                case SCENE_KAKARIKO_VILLAGE:
                case SCENE_GRAVEYARD:
                case SCENE_ZORAS_RIVER:
                case SCENE_KOKIRI_FOREST:
                case SCENE_SACRED_FOREST_MEADOW:
                case SCENE_LAKE_HYLIA:
                case SCENE_ZORAS_DOMAIN:
                case SCENE_ZORAS_FOUNTAIN:
                case SCENE_GERUDO_VALLEY:
                case SCENE_LOST_WOODS:
                case SCENE_DESERT_COLOSSUS:
                case SCENE_GERUDOS_FORTRESS:
                case SCENE_HAUNTED_WASTELAND:
                case SCENE_HYRULE_CASTLE:
                case SCENE_DEATH_MOUNTAIN_TRAIL:
                case SCENE_DEATH_MOUNTAIN_CRATER:
                case SCENE_GORON_CITY:
                case SCENE_LON_LON_RANCH:
                case SCENE_OUTSIDE_GANONS_CASTLE:
                    if (interfaceCtx->minimapAlpha < 170) {
                        interfaceCtx->minimapAlpha = alpha;
                    } else {
                        interfaceCtx->minimapAlpha = 170;
                    }
                    break;
                default:
                    if (interfaceCtx->minimapAlpha != 255) {
                        interfaceCtx->minimapAlpha = alpha;
                    }
                    break;
            }
            break;
        case 7:
            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            func_80082644(play, alpha);

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
        case 9:
            if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                interfaceCtx->bAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }





            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
        case 10:
            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }





            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if (interfaceCtx->bAlpha != 255) {
                interfaceCtx->bAlpha = alpha;
            }

            break;
        case 11:
            if ((interfaceCtx->bAlpha != 0) && (interfaceCtx->bAlpha > maxAlpha)) {
                interfaceCtx->bAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }





            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            break;
        case 12:
            if (interfaceCtx->aAlpha != 255) {
                interfaceCtx->aAlpha = alpha;
            }

            if (interfaceCtx->bAlpha != 255) {
                interfaceCtx->bAlpha = alpha;
            }

            if (interfaceCtx->minimapAlpha != 255) {
                interfaceCtx->minimapAlpha = alpha;
            }

            if ((interfaceCtx->cLeftAlpha != 0) && (interfaceCtx->cLeftAlpha > maxAlpha)) {
                interfaceCtx->cLeftAlpha = maxAlpha;
            }

            if ((interfaceCtx->cDownAlpha != 0) && (interfaceCtx->cDownAlpha > maxAlpha)) {
                interfaceCtx->cDownAlpha = maxAlpha;
            }

            if ((interfaceCtx->cRightAlpha != 0) && (interfaceCtx->cRightAlpha > maxAlpha)) {
                interfaceCtx->cRightAlpha = maxAlpha;
            }





            if ((interfaceCtx->magicAlpha != 0) && (interfaceCtx->magicAlpha > maxAlpha)) {
                interfaceCtx->magicAlpha = maxAlpha;
            }

            if ((interfaceCtx->healthAlpha != 0) && (interfaceCtx->healthAlpha > maxAlpha)) {
                interfaceCtx->healthAlpha = maxAlpha;
            }

            break;
        case 13:
            func_8008277C(play, maxAlpha, alpha);

            if ((interfaceCtx->minimapAlpha != 0) && (interfaceCtx->minimapAlpha > maxAlpha)) {
                interfaceCtx->minimapAlpha = maxAlpha;
            }

            if ((interfaceCtx->aAlpha != 0) && (interfaceCtx->aAlpha > maxAlpha)) {
                interfaceCtx->aAlpha = maxAlpha;
            }

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha;
            }

            break;
    }

    if ((play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_1) && (interfaceCtx->minimapAlpha >= 255)) {
        interfaceCtx->minimapAlpha = 255;
    }
}

// buttonStatus[0] is used to represent if the B button is disabled, but also tracks
// the last active B button item during mini-games/epona (temp B)
// Since ITEM_NONE is the same as BTN_DISABLED (255), we need a different value to help us track
// that the player was swordless before like ITEM_NONE_FE (254)
#define SWORDLESS_STATUS ITEM_NONE_FE

void func_80083108(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;
    Player* player = GET_PLAYER(play);
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 i;
    s16 sp28 = 0;

    if ((gSaveContext.cutsceneIndex < 0xFFF0) ||
        ((play->sceneNum == SCENE_LON_LON_RANCH) && (gSaveContext.cutsceneIndex == 0xFFF0))) {
        gSaveContext.forceRisingButtonAlphas = 0;

        if ((player->stateFlags1 & PLAYER_STATE1_ON_HORSE) || (play->shootingGalleryStatus > 1) ||
            ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38))) {
            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                gSaveContext.forceRisingButtonAlphas = 1;

                if (gSaveContext.buttonStatus[0] == BTN_DISABLED) {
                    gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
                        gSaveContext.buttonStatus[3] = BTN_ENABLED;
                }

                if ((gSaveContext.equips.buttonItems[0] != ITEM_SLINGSHOT) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_BOW) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_BOMBCHU) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_NONE)) {
                    gSaveContext.buttonStatus[0] = gSaveContext.equips.buttonItems[0];

                    if ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38)) {
                        gSaveContext.equips.buttonItems[0] = ITEM_BOMBCHU;
                        Interface_LoadItemIcon1(play, 0);
                    } else {
                        gSaveContext.equips.buttonItems[0] = ITEM_BOW;
                        if (play->shootingGalleryStatus > 1) {
                            if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
                                gSaveContext.equips.buttonItems[0] = ITEM_SLINGSHOT;
                            }

                            Interface_LoadItemIcon1(play, 0);
                        } else {
                            if (gSaveContext.inventory.items[SLOT_BOW] == ITEM_NONE) {
                                gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                            } else {
                                Interface_LoadItemIcon1(play, 0);
                            }
                        }
                    }

                    gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] = gSaveContext.buttonStatus[3] =
                        BTN_DISABLED;
                    Interface_ChangeAlpha(6);
                }

                if (play->transitionMode != TRANS_MODE_OFF) {
                    Interface_ChangeAlpha(1);
                } else if (gSaveContext.minigameState == 1) {
                    Interface_ChangeAlpha(8);
                } else if (play->shootingGalleryStatus > 1) {
                    Interface_ChangeAlpha(8);
                } else if ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38)) {
                    Interface_ChangeAlpha(8);
                } else if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
                    Interface_ChangeAlpha(12);
                }
            } else {
                if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
                    Interface_ChangeAlpha(12);
                }
            }
        } else if (play->sceneNum == SCENE_CHAMBER_OF_THE_SAGES) {
            Interface_ChangeAlpha(1);
        } else if (play->sceneNum == SCENE_FISHING_POND) {
            gSaveContext.forceRisingButtonAlphas = 2;
            if (play->interfaceCtx.unk_260 != 0) {
                if (gSaveContext.equips.buttonItems[0] != ITEM_FISHING_POLE) {
                    gSaveContext.buttonStatus[0] = gSaveContext.equips.buttonItems[0];

                    gSaveContext.equips.buttonItems[0] = ITEM_FISHING_POLE;
                    gSaveContext.unk_13EA = 0;
                    Interface_LoadItemIcon1(play, 0);
                    Interface_ChangeAlpha(12);
                }

                if (gSaveContext.unk_13EA != 12) {
                    Interface_ChangeAlpha(12);
                }
            } else if (gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE) {
                gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                gSaveContext.unk_13EA = 0;


                if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                    Interface_LoadItemIcon1(play, 0);
                }

                gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
                    gSaveContext.buttonStatus[3] = BTN_DISABLED;
                Interface_ChangeAlpha(50);
            } else {
                if (gSaveContext.buttonStatus[0] == BTN_ENABLED) {
                    gSaveContext.unk_13EA = 0;
                }

                gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
                    gSaveContext.buttonStatus[3] = BTN_DISABLED;
                Interface_ChangeAlpha(50);
            }
        } else if (msgCtx->msgMode == MSGMODE_NONE) {
            if ((Player_GetEnvironmentalHazard(play) >= 2) && (Player_GetEnvironmentalHazard(play) < 5)) {
                if (gSaveContext.buttonStatus[0] != BTN_DISABLED) {
                    sp28 = 1;
                }

                gSaveContext.buttonStatus[0] = BTN_DISABLED;

                for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if ((gSaveContext.equips.buttonItems[i] >= ITEM_SHIELD_DEKU) &&
                        (gSaveContext.equips.buttonItems[i] <= ITEM_BOOTS_HOVER)) {
                        // Equipment on c-buttons is always enabled
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                    } else if (Player_GetEnvironmentalHazard(play) == 2) {
                        if ((gSaveContext.equips.buttonItems[i] != ITEM_HOOKSHOT) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_LONGSHOT)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        } else {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    } else {
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                    }
                }

                if (sp28) {
                    gSaveContext.unk_13EA = 0;
                }

                Interface_ChangeAlpha(50);
            } else if ((player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) ||
                       (player->stateFlags2 & PLAYER_STATE2_CRAWLING)) {
                if (gSaveContext.buttonStatus[0] != BTN_DISABLED) {
                    gSaveContext.buttonStatus[0] = BTN_DISABLED;
                    gSaveContext.buttonStatus[1] = BTN_DISABLED;
                    gSaveContext.buttonStatus[2] = BTN_DISABLED;
                    gSaveContext.buttonStatus[3] = BTN_DISABLED;
                    gSaveContext.unk_13EA = 0;
                    Interface_ChangeAlpha(50);
                }
            } else if ((gSaveContext.eventInf[0] & 0xF) == 1) {
                if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
                    if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) &&
                        (gSaveContext.equips.buttonItems[0] != ITEM_BOW)) {
                        if (gSaveContext.inventory.items[SLOT_BOW] == ITEM_NONE) {
                            gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                        } else {
                            gSaveContext.equips.buttonItems[0] = ITEM_BOW;
                            sp28 = 1;
                        }
                    }
                } else {
                    sp28 = 1;

                    if ((gSaveContext.equips.buttonItems[0] == ITEM_NONE) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOW)) {

                        if ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI) &&
                            (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER) &&
                            (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_BGS) &&
                            (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KNIFE)) {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];

                        } else {
                            gSaveContext.buttonStatus[0] = gSaveContext.equips.buttonItems[0];
                        }
                    }
                }

                if (sp28) {
                    Interface_LoadItemIcon1(play, 0);
                    sp28 = 0;
                }

                for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if ((gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_FAIRY) &&
                        (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_TIME)) {
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                    } else {
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                    }
                }

                if (sp28) {
                    gSaveContext.unk_13EA = 0;
                }

                Interface_ChangeAlpha(50);
            } else {
                if (interfaceCtx->restrictions.bButton == 0) {
                    if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOW) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_NONE)) {
                        if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) || (gSaveContext.infTable[29] == 0)) {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];

                            sp28 = 1;

                            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                                Interface_LoadItemIcon1(play, 0);
                            }
                        }
                    } else if ((gSaveContext.buttonStatus[0] & 0xFF) == BTN_DISABLED) {
                        sp28 = 1;

                        if (((gSaveContext.buttonStatus[0] & 0xFF) == BTN_DISABLED) ||
                            ((gSaveContext.buttonStatus[0] & 0xFF) == BTN_ENABLED)) {
                            gSaveContext.buttonStatus[0] = BTN_ENABLED;
                        } else {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0] & 0xFF;
                        }
                    }
                } else if (interfaceCtx->restrictions.bButton == 1) {
                    if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOW) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
                        (gSaveContext.equips.buttonItems[0] == ITEM_NONE)) {
                        if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) || (gSaveContext.infTable[29] == 0)) {
                            gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];

                            sp28 = 1;

                            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                                Interface_LoadItemIcon1(play, 0);
                            }
                        }
                    } else {
                        if (gSaveContext.buttonStatus[0] == BTN_ENABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[0] = BTN_DISABLED;
                    }
                }

                for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                    if ((gSaveContext.equips.buttonItems[i] >= ITEM_SHIELD_DEKU) &&
                        (gSaveContext.equips.buttonItems[i] <= ITEM_BOOTS_HOVER)) {
                        // Equipment on c-buttons is always enabled
                        if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                            sp28 = 1;
                        }

                        gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                    }
                }

                if (interfaceCtx->restrictions.bottles != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                            (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.bottles == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                            (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.tradeItems != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                                   (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.tradeItems == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                            (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.hookshot != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_HOOKSHOT) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_LONGSHOT)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.hookshot == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_HOOKSHOT) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_LONGSHOT)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.ocarina != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_FAIRY) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_TIME)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.ocarina == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_FAIRY) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_TIME)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.farores != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if (gSaveContext.equips.buttonItems[i] == ITEM_FARORES_WIND) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                            osSyncPrintf("***(i=%d)***  ", i);
                        }
                    }
                } else if (interfaceCtx->restrictions.farores == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if (gSaveContext.equips.buttonItems[i] == ITEM_FARORES_WIND) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.dinsNayrus != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_DINS_FIRE) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_NAYRUS_LOVE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                        }
                    }
                } else if (interfaceCtx->restrictions.dinsNayrus == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] == ITEM_DINS_FIRE) ||
                            (gSaveContext.equips.buttonItems[i] == ITEM_NAYRUS_LOVE)) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }

                if (interfaceCtx->restrictions.all != 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_FAIRY) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_TIME) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_SHIELD_DEKU) && // Never disable equipment
                              (gSaveContext.equips.buttonItems[i] <=
                               ITEM_BOOTS_HOVER)) && // (tunics/boots) on C-buttons
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                            if ((play->sceneNum != SCENE_TREASURE_BOX_SHOP) ||
                                (gSaveContext.equips.buttonItems[i] != ITEM_LENS)) {
                                if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_ENABLED) {
                                    sp28 = 1;
                                }

                                gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_DISABLED;
                            } else {
                                if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                    sp28 = 1;
                                }

                                gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                            }
                        }
                    }
                } else if (interfaceCtx->restrictions.all == 0) {
                    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                        if ((gSaveContext.equips.buttonItems[i] != ITEM_DINS_FIRE) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_HOOKSHOT) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_LONGSHOT) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_FARORES_WIND) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_NAYRUS_LOVE) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_FAIRY) &&
                            (gSaveContext.equips.buttonItems[i] != ITEM_OCARINA_TIME) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) &&
                            !((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                              (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                            if (gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] == BTN_DISABLED) {
                                sp28 = 1;
                            }

                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(i)] = BTN_ENABLED;
                        }
                    }
                }
            }
        }
    }

    if (sp28) {
        gSaveContext.unk_13EA = 0;
        if ((play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF)) {
            Interface_ChangeAlpha(50);
            osSyncPrintf("????????  alpha_change( 50 );  ?????\n");
        } else {
            osSyncPrintf("game_play->fade_direction || game_play->fbdemo_wipe_modem");
        }
    }
}

void Interface_SetSceneRestrictions(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 i;
    u8 currentScene;

    // clang-format off
    interfaceCtx->restrictions.hGauge = interfaceCtx->restrictions.bButton =
    interfaceCtx->restrictions.aButton = interfaceCtx->restrictions.bottles =
    interfaceCtx->restrictions.tradeItems = interfaceCtx->restrictions.hookshot =
    interfaceCtx->restrictions.ocarina = interfaceCtx->restrictions.warpSongs =
    interfaceCtx->restrictions.sunsSong = interfaceCtx->restrictions.farores =
    interfaceCtx->restrictions.dinsNayrus = interfaceCtx->restrictions.all = 0;
    // clang-format on

    i = 0;

    // "Data settings related to button display scene_data_ID=%d\n"
    osSyncPrintf("ボタン表示関係データ設定 scene_data_ID=%d\n", play->sceneNum);

    do {
        currentScene = (u8)play->sceneNum;
        if (sRestrictionFlags[i].scene == currentScene) {
            interfaceCtx->restrictions.hGauge = (sRestrictionFlags[i].flags1 & 0xC0) >> 6;
            interfaceCtx->restrictions.bButton = (sRestrictionFlags[i].flags1 & 0x30) >> 4;
            interfaceCtx->restrictions.aButton = (sRestrictionFlags[i].flags1 & 0x0C) >> 2;
            interfaceCtx->restrictions.bottles = (sRestrictionFlags[i].flags1 & 0x03) >> 0;
            interfaceCtx->restrictions.tradeItems = (sRestrictionFlags[i].flags2 & 0xC0) >> 6;
            interfaceCtx->restrictions.hookshot = (sRestrictionFlags[i].flags2 & 0x30) >> 4;
            interfaceCtx->restrictions.ocarina = (sRestrictionFlags[i].flags2 & 0x0C) >> 2;
            interfaceCtx->restrictions.warpSongs = (sRestrictionFlags[i].flags2 & 0x03) >> 0;
            interfaceCtx->restrictions.sunsSong = (sRestrictionFlags[i].flags3 & 0xC0) >> 6;
            interfaceCtx->restrictions.farores = (sRestrictionFlags[i].flags3 & 0x30) >> 4;
            interfaceCtx->restrictions.dinsNayrus = (sRestrictionFlags[i].flags3 & 0x0C) >> 2;
            interfaceCtx->restrictions.all = (sRestrictionFlags[i].flags3 & 0x03) >> 0;

            osSyncPrintf(VT_FGCOL(YELLOW));
            osSyncPrintf("parameter->button_status = %x,%x,%x\n", sRestrictionFlags[i].flags1,
                         sRestrictionFlags[i].flags2, sRestrictionFlags[i].flags3);
            osSyncPrintf("h_gage=%d, b_button=%d, a_button=%d, c_bottle=%d\n", interfaceCtx->restrictions.hGauge,
                         interfaceCtx->restrictions.bButton, interfaceCtx->restrictions.aButton,
                         interfaceCtx->restrictions.bottles);
            osSyncPrintf("c_warasibe=%d, c_hook=%d, c_ocarina=%d, c_warp=%d\n", interfaceCtx->restrictions.tradeItems,
                         interfaceCtx->restrictions.hookshot, interfaceCtx->restrictions.ocarina,
                         interfaceCtx->restrictions.warpSongs);
            osSyncPrintf("c_sunmoon=%d, m_wind=%d, m_magic=%d, another=%d\n", interfaceCtx->restrictions.sunsSong,
                         interfaceCtx->restrictions.farores, interfaceCtx->restrictions.dinsNayrus,
                         interfaceCtx->restrictions.all);
            osSyncPrintf(VT_RST);
            return;
        }
        i++;
    } while (sRestrictionFlags[i].scene != 0xFF);
}

Gfx* Gfx_TextureIA8(Gfx* displayListHead, void* texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                    s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy) {
    gDPLoadTextureBlock(displayListHead++, texture, G_IM_FMT_IA, G_IM_SIZ_8b, textureWidth, textureHeight, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gSPWideTextureRectangle(displayListHead++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                            (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, dsdx, dtdy);

    return displayListHead;
}

Gfx* Gfx_TextureI8(Gfx* displayListHead, void* texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                   s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy) {
    gDPLoadTextureBlock(displayListHead++, texture, G_IM_FMT_I, G_IM_SIZ_8b, textureWidth, textureHeight, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gSPWideTextureRectangle(displayListHead++, rectLeft << 2, rectTop << 2, (rectLeft + rectWidth) << 2,
                            (rectTop + rectHeight) << 2, G_TX_RENDERTILE, 0, 0, dsdx, dtdy);

    return displayListHead;
}

void Inventory_SwapAgeEquipment(void) {
    s16 i;
    u16 shieldEquipValue;

    if (LINK_AGE_IN_YEARS == YEARS_CHILD) {
        for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (i != 0) {
                gSaveContext.childEquips.buttonItems[i] = gSaveContext.equips.buttonItems[i];
            } else {
                gSaveContext.childEquips.buttonItems[i] = ITEM_SWORD_KOKIRI;
            }

            if (i != 0) {
                gSaveContext.childEquips.cButtonSlots[i - 1] = gSaveContext.equips.cButtonSlots[i - 1];
            }
        }

        gSaveContext.childEquips.equipment = gSaveContext.equips.equipment;

        if (gSaveContext.adultEquips.buttonItems[0] == ITEM_NONE) {
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;

            if (gSaveContext.inventory.items[SLOT_NUT] != ITEM_NONE) {
                gSaveContext.equips.buttonItems[1] = ITEM_NUT;
                gSaveContext.equips.cButtonSlots[0] = SLOT_NUT;
            } else {
                gSaveContext.equips.buttonItems[1] = gSaveContext.equips.cButtonSlots[0] = ITEM_NONE;
            }

            gSaveContext.equips.buttonItems[2] = ITEM_BOMB;
            gSaveContext.equips.buttonItems[3] = gSaveContext.inventory.items[SLOT_OCARINA];
            gSaveContext.equips.cButtonSlots[1] = SLOT_BOMB;
            gSaveContext.equips.cButtonSlots[2] = SLOT_OCARINA;
            gSaveContext.equips.equipment = (EQUIP_VALUE_SWORD_MASTER << (EQUIP_TYPE_SWORD * 4)) |
                                            (EQUIP_VALUE_SHIELD_HYLIAN << (EQUIP_TYPE_SHIELD * 4)) |
                                            (EQUIP_VALUE_TUNIC_KOKIRI << (EQUIP_TYPE_TUNIC * 4)) |
                                            (EQUIP_VALUE_BOOTS_KOKIRI << (EQUIP_TYPE_BOOTS * 4));
        } else {
            for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = gSaveContext.adultEquips.buttonItems[i];

                if (i != 0) {
                    gSaveContext.equips.cButtonSlots[i - 1] = gSaveContext.adultEquips.cButtonSlots[i - 1];
                }

                if (((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) ||
                    ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                    osSyncPrintf("Register_Item_Pt(%d)=%d\n", i, gSaveContext.equips.cButtonSlots[i - 1]);
                    
                        gSaveContext.equips.buttonItems[i] =
                            gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[i - 1]];
                    
                }
            }

            gSaveContext.equips.equipment = gSaveContext.adultEquips.equipment;
        }
    } else {
        for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            gSaveContext.adultEquips.buttonItems[i] = gSaveContext.equips.buttonItems[i];

            if (i != 0) {
                gSaveContext.adultEquips.cButtonSlots[i - 1] = gSaveContext.equips.cButtonSlots[i - 1];
            }
        }

        gSaveContext.adultEquips.equipment = gSaveContext.equips.equipment;

        if (gSaveContext.childEquips.buttonItems[0] != ITEM_NONE) {
            for (i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = gSaveContext.childEquips.buttonItems[i];

                if (i != 0) {
                    gSaveContext.equips.cButtonSlots[i - 1] = gSaveContext.childEquips.cButtonSlots[i - 1];
                }

                if (((gSaveContext.equips.buttonItems[i] >= ITEM_BOTTLE) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_POE)) ||
                    ((gSaveContext.equips.buttonItems[i] >= ITEM_WEIRD_EGG) &&
                     (gSaveContext.equips.buttonItems[i] <= ITEM_CLAIM_CHECK))) {
                    osSyncPrintf("Register_Item_Pt(%d)=%d\n", i, gSaveContext.equips.cButtonSlots[i - 1]);
                    gSaveContext.equips.buttonItems[i] =
                        gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[i - 1]];
                }
            }

            gSaveContext.equips.equipment = gSaveContext.childEquips.equipment;
            gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.equipment |= EQUIP_VALUE_SWORD_KOKIRI << (EQUIP_TYPE_SWORD * 4);
        }
    }

    shieldEquipValue = gEquipMasks[EQUIP_TYPE_SHIELD] & gSaveContext.equips.equipment;
    if (shieldEquipValue) {
        shieldEquipValue >>= gEquipShifts[EQUIP_TYPE_SHIELD];
        if (!CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SHIELD, shieldEquipValue - 1)) {
            gSaveContext.equips.equipment &= gEquipNegMasks[EQUIP_TYPE_SHIELD];
        }
    }
}

void Interface_InitHorsebackArchery(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    gSaveContext.minigameState = 1;
    interfaceCtx->unk_23C = interfaceCtx->unk_240 = interfaceCtx->unk_242 = 0;
    gSaveContext.minigameScore = sHBAScoreTier = 0;
    interfaceCtx->hbaAmmo = 20;
}

void func_800849EC(PlayState* play) {
    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
    gSaveContext.inventory.equipment ^= OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);

    if (CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)) {
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
    } else {
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
    }

    Interface_LoadItemIcon1(play, 0);
}

void Interface_LoadItemIcon1(PlayState* play, u16 button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    osCreateMesgQueue(&interfaceCtx->loadQueue, &interfaceCtx->loadMsg, OS_MESG_BLOCK);
    DmaMgr_SendRequest2(&interfaceCtx->dmaRequest_160, interfaceCtx->iconItemSegment + button * 0x1000,
                        (uintptr_t)_icon_item_staticSegmentRomStart +
                            (gSaveContext.equips.buttonItems[button] * 0x1000),
                        0x1000, 0, &interfaceCtx->loadQueue, OS_MESG_PTR(NULL), __FILE__, __LINE__);
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);
}

void Interface_LoadItemIcon2(PlayState* play, u16 button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    osCreateMesgQueue(&interfaceCtx->loadQueue, &interfaceCtx->loadMsg, OS_MESG_BLOCK);
    DmaMgr_SendRequest2(&interfaceCtx->dmaRequest_180, interfaceCtx->iconItemSegment + button * 0x1000,
                        (uintptr_t)_icon_item_staticSegmentRomStart +
                            (gSaveContext.equips.buttonItems[button] * 0x1000),
                        0x1000, 0, &interfaceCtx->loadQueue, OS_MESG_PTR(NULL), __FILE__, __LINE__);
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);
}

void func_80084BF4(PlayState* play, u16 flag) {
    if (flag) {
        if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
            (gSaveContext.equips.buttonItems[0] == ITEM_BOW) || (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
            (gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE) ||
            (gSaveContext.buttonStatus[0] == BTN_DISABLED)) {
            if ((gSaveContext.equips.buttonItems[0] == ITEM_SLINGSHOT) ||
                (gSaveContext.equips.buttonItems[0] == ITEM_BOW) ||
                (gSaveContext.equips.buttonItems[0] == ITEM_BOMBCHU) ||
                (gSaveContext.equips.buttonItems[0] == ITEM_FISHING_POLE)) {
                gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                Interface_LoadItemIcon1(play, 0);
            }
        } else if (gSaveContext.equips.buttonItems[0] == ITEM_NONE) {
            if ((gSaveContext.equips.buttonItems[0] != ITEM_NONE) || (gSaveContext.infTable[29] == 0)) {
                gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                Interface_LoadItemIcon1(play, 0);
            }
        }

        gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
            gSaveContext.buttonStatus[3] = BTN_ENABLED;
        Interface_ChangeAlpha(7);
    } else {
        gSaveContext.buttonStatus[0] = gSaveContext.buttonStatus[1] = gSaveContext.buttonStatus[2] =
            gSaveContext.buttonStatus[3] = BTN_ENABLED;
        func_80083108(play);
    }
}

u8 Return_Item_Entry(GetItemEntry itemEntry, u8 returnItem) {
    return returnItem;
}

// Processes Item_Give returns
u8 Return_Item(u8 itemID, u8 modId, ItemID returnItem) {
    // ITEM_SOLD_OUT doesn't have an ItemTable entry, so pass custom entry instead
    if (itemID == ITEM_SOLD_OUT) {
        GetItemEntry gie = { ITEM_SOLD_OUT, 0, 0, 0, 0, 0, 0, 0, 0, false, ITEM_FROM_NPC, ITEM_CATEGORY_LESSER, NULL };
        return Return_Item_Entry(gie, returnItem);
    }

    GetItemID getItemID = RetrieveGetItemIDFromItemID(itemID);
    if (getItemID != GI_MAX) {
        // Vanilla ItemID with an associated GetItemID
        return Return_Item_Entry(ItemTable_RetrieveEntry(modId, getItemID), returnItem);
    }

    assert(false);
    return Return_Item_Entry(ItemTable_RetrieveEntry(modId, itemID), returnItem);
}

/**
 * @brief Adds the given item to Link's inventory.
 *
 * NOTE: This function has been edited to be safe to use with a NULL play.
 * If you need to add to this function, be sure you check if the play is not
 * NULL before doing any operations requiring it.
 *
 * @param play
 * @param item
 * @return u8
 */
u8 Item_Give(PlayState* play, u8 item) {
    Error("Item Give - item: %#x", item);
    static s16 sAmmoRefillCounts[] = { 5, 10, 20, 30, 5, 10, 30, 0, 5, 20, 1, 5, 10, 20, 50, 10 };
    s16 i;
    s16 slot;
    s16 temp;

    GetItemID returnItem = ITEM_NONE;

    slot = SLOT(item);
    if (item >= ITEM_STICKS_5) {
        slot = SLOT(sExtraItemBases[item - ITEM_STICKS_5]);
    }

    osSyncPrintf(VT_FGCOL(YELLOW));
    osSyncPrintf("item_get_setting=%d  pt=%d  z=%x\n", item, slot, gSaveContext.inventory.items[slot]);
    osSyncPrintf(VT_RST);

    if ((item >= ITEM_MEDALLION_FOREST) && (item <= ITEM_MEDALLION_LIGHT)) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_MEDALLION_FOREST + QUEST_MEDALLION_FOREST];

        osSyncPrintf(VT_FGCOL(YELLOW));
        osSyncPrintf("封印 = %x\n", gSaveContext.inventory.questItems); // "Seals = %x"
        osSyncPrintf(VT_RST);

        if (item == ITEM_MEDALLION_WATER) {
            func_8006D0AC(play);
        }

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_SONG_MINUET) && (item <= ITEM_SONG_STORMS)) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_SONG_MINUET + QUEST_SONG_MINUET];

        osSyncPrintf(VT_FGCOL(YELLOW));
        osSyncPrintf("楽譜 = %x\n", gSaveContext.inventory.questItems); // "Musical scores = %x"
        // "Musical scores = %x (%x) (%x)"
        osSyncPrintf("楽譜 = %x (%x) (%x)\n", gSaveContext.inventory.questItems,
                     gBitFlags[item - ITEM_SONG_MINUET + QUEST_SONG_MINUET], gBitFlags[item - ITEM_SONG_MINUET]);
        osSyncPrintf(VT_RST);

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_KOKIRI_EMERALD) && (item <= ITEM_ZORA_SAPPHIRE)) {
        return ITEM_NONE;
    } else if ((item == ITEM_STONE_OF_AGONY) || (item == ITEM_GERUDO_CARD)) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_STONE_OF_AGONY + QUEST_STONE_OF_AGONY];

        osSyncPrintf(VT_FGCOL(YELLOW));
        osSyncPrintf("アイテム = %x\n", gSaveContext.inventory.questItems); // "Items = %x"
        osSyncPrintf(VT_RST);

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_SKULL_TOKEN) {
        gSaveContext.inventory.questItems |= gBitFlags[item - ITEM_SKULL_TOKEN + QUEST_SKULL_TOKEN];
        gSaveContext.inventory.gsTokens++;

        osSyncPrintf(VT_FGCOL(YELLOW));
        // "N Coins = %x(%d)"
        osSyncPrintf("Ｎコイン = %x(%d)\n", gSaveContext.inventory.questItems, gSaveContext.inventory.gsTokens);
        osSyncPrintf(VT_RST);

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_BGS)) {
        gSaveContext.inventory.equipment |=
            OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, item - ITEM_SWORD_KOKIRI + EQUIP_INV_SWORD_KOKIRI);

        // Both Giant's Knife and Biggoron Sword have the same Item ID, so this part handles both of them
        if (item == ITEM_SWORD_BGS) {
            gSaveContext.swordHealth = 8;

            if (ALL_EQUIP_VALUE(EQUIP_TYPE_SWORD) ==
                ((1 << EQUIP_INV_SWORD_KOKIRI) | (1 << EQUIP_INV_SWORD_MASTER) | (1 << EQUIP_INV_SWORD_BIGGORON) |
                 (1 << EQUIP_INV_SWORD_BROKENGIANTKNIFE))) {

                gSaveContext.inventory.equipment ^=
                    OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);

                if (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KNIFE) {
                    gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
                    if (play != NULL) {
                        Interface_LoadItemIcon1(play, 0);
                    }
                }
            }

        } else if (item == ITEM_SWORD_MASTER) {
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;
            gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.equipment |= EQUIP_VALUE_SWORD_MASTER << (EQUIP_TYPE_SWORD * 4);
            if (play != NULL) {
                Interface_LoadItemIcon1(play, 0);
            }
        }

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_SHIELD_DEKU) && (item <= ITEM_SHIELD_MIRROR)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, item - ITEM_SHIELD_DEKU);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_TUNIC_KOKIRI) && (item <= ITEM_TUNIC_ZORA)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_TUNIC, item - ITEM_TUNIC_KOKIRI);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_BOOTS_KOKIRI) && (item <= ITEM_BOOTS_HOVER)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_BOOTS, item - ITEM_BOOTS_KOKIRI);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item == ITEM_KEY_BOSS) || (item == ITEM_COMPASS) || (item == ITEM_DUNGEON_MAP)) {
        gSaveContext.inventory.dungeonItems[gSaveContext.mapIndex] |= gBitFlags[item - ITEM_KEY_BOSS];
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_KEY_SMALL) {
        if (gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] < 0) {
            gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] = 1;
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        } else {
            gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex]++;
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }
    } else if ((item == ITEM_QUIVER_30) || (item == ITEM_BOW)) {
        if (CUR_UPG_VALUE(UPG_QUIVER) == 0) {
            Inventory_ChangeUpgrade(UPG_QUIVER, 1);
            INV_CONTENT(ITEM_BOW) = ITEM_BOW;
            AMMO(ITEM_BOW) = CAPACITY(UPG_QUIVER, 1);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        } else {
            AMMO(ITEM_BOW)++;
            if (AMMO(ITEM_BOW) > CUR_CAPACITY(UPG_QUIVER)) {
                AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
            }
        }
    } else if (item == ITEM_QUIVER_40) {
        Inventory_ChangeUpgrade(UPG_QUIVER, 2);
        AMMO(ITEM_BOW) = CAPACITY(UPG_QUIVER, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_QUIVER_50) {
        Inventory_ChangeUpgrade(UPG_QUIVER, 3);
        AMMO(ITEM_BOW) = CAPACITY(UPG_QUIVER, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BULLET_BAG_40) {
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, 2);
        AMMO(ITEM_SLINGSHOT) = CAPACITY(UPG_BULLET_BAG, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BULLET_BAG_50) {
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, 3);
        AMMO(ITEM_SLINGSHOT) = CAPACITY(UPG_BULLET_BAG, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOMB_BAG_20) {
        if (CUR_UPG_VALUE(UPG_BOMB_BAG) == 0) {
            Inventory_ChangeUpgrade(UPG_BOMB_BAG, 1);
            INV_CONTENT(ITEM_BOMB) = ITEM_BOMB;
            AMMO(ITEM_BOMB) = CAPACITY(UPG_BOMB_BAG, 1);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        } else {
            AMMO(ITEM_BOMB)++;
            if (AMMO(ITEM_BOMB) > CUR_CAPACITY(UPG_BOMB_BAG)) {
                AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
            }
        }
    } else if (item == ITEM_BOMB_BAG_30) {
        Inventory_ChangeUpgrade(UPG_BOMB_BAG, 2);
        AMMO(ITEM_BOMB) = CAPACITY(UPG_BOMB_BAG, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOMB_BAG_40) {
        Inventory_ChangeUpgrade(UPG_BOMB_BAG, 3);
        AMMO(ITEM_BOMB) = CAPACITY(UPG_BOMB_BAG, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BRACELET) {
        Inventory_ChangeUpgrade(UPG_STRENGTH, 1);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_GAUNTLETS_SILVER) {
        Inventory_ChangeUpgrade(UPG_STRENGTH, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_GAUNTLETS_GOLD) {
        Inventory_ChangeUpgrade(UPG_STRENGTH, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_WALLET_ADULT) {
        Inventory_ChangeUpgrade(UPG_WALLET, 1);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_WALLET_GIANT) {
        Inventory_ChangeUpgrade(UPG_WALLET, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_STICK_UPGRADE_20) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_STICK) = ITEM_STICK;
        }
        Inventory_ChangeUpgrade(UPG_STICKS, 2);
        AMMO(ITEM_STICK) = CAPACITY(UPG_STICKS, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_STICK_UPGRADE_30) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_STICK) = ITEM_STICK;
        }
        Inventory_ChangeUpgrade(UPG_STICKS, 3);
        AMMO(ITEM_STICK) = CAPACITY(UPG_STICKS, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_NUT_UPGRADE_30) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_NUT) = ITEM_NUT;
        }
        Inventory_ChangeUpgrade(UPG_NUTS, 2);
        AMMO(ITEM_NUT) = CAPACITY(UPG_NUTS, 2);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_NUT_UPGRADE_40) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_NUT) = ITEM_NUT;
        }
        Inventory_ChangeUpgrade(UPG_NUTS, 3);
        AMMO(ITEM_NUT) = CAPACITY(UPG_NUTS, 3);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_LONGSHOT) {
        INV_CONTENT(item) = item;
        // always update "equips" as this is what is currently on the c-buttons
        for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (gSaveContext.equips.buttonItems[i] == ITEM_HOOKSHOT) {
                gSaveContext.equips.buttonItems[i] = ITEM_LONGSHOT;
                if (play != NULL) {
                    Interface_LoadItemIcon1(play, i);
                }
            }
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_STICK) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_STICKS, 1);
            AMMO(ITEM_STICK) = 1;
        } else {
            AMMO(ITEM_STICK)++;
            if (AMMO(ITEM_STICK) > CUR_CAPACITY(UPG_STICKS)) {
                AMMO(ITEM_STICK) = CUR_CAPACITY(UPG_STICKS);
            }
        }
    } else if ((item == ITEM_STICKS_5) || (item == ITEM_STICKS_10)) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_STICKS, 1);
            AMMO(ITEM_STICK) = sAmmoRefillCounts[item - ITEM_STICKS_5];
        } else {
            AMMO(ITEM_STICK) += sAmmoRefillCounts[item - ITEM_STICKS_5];
            if (AMMO(ITEM_STICK) > CUR_CAPACITY(UPG_STICKS)) {
                AMMO(ITEM_STICK) = CUR_CAPACITY(UPG_STICKS);
            }
        }
        // [SOH] This results in the same behavior as the original code, but also allows us to get an accurate
        // ReceivedItemEntry hook
        INV_CONTENT(ITEM_STICK) = ITEM_STICK;
        return Return_Item(item, MOD_NONE, returnItem);
    } else if (item == ITEM_NUT) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_NUTS, 1);
            AMMO(ITEM_NUT) = ITEM_NUT;
        } else {
            AMMO(ITEM_NUT)++;
            if (AMMO(ITEM_NUT) > CUR_CAPACITY(UPG_NUTS)) {
                AMMO(ITEM_NUT) = CUR_CAPACITY(UPG_NUTS);
            }
        }
    } else if ((item == ITEM_NUTS_5) || (item == ITEM_NUTS_10)) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            Inventory_ChangeUpgrade(UPG_NUTS, 1);
            AMMO(ITEM_NUT) += sAmmoRefillCounts[item - ITEM_NUTS_5];
            // "Deku Nuts %d(%d)=%d BS_count=%d"
            osSyncPrintf("デクの実 %d(%d)=%d  BS_count=%d\n", item, ITEM_NUTS_5, item - ITEM_NUTS_5,
                         sAmmoRefillCounts[item - ITEM_NUTS_5]);
        } else {
            AMMO(ITEM_NUT) += sAmmoRefillCounts[item - ITEM_NUTS_5];
            if (AMMO(ITEM_NUT) > CUR_CAPACITY(UPG_NUTS)) {
                AMMO(ITEM_NUT) = CUR_CAPACITY(UPG_NUTS);
            }
        }
        // [SOH] This results in the same behavior as the original code, but also allows us to get an accurate
        // ReceivedItemEntry hook
        INV_CONTENT(ITEM_NUT) = ITEM_NUT;
        return Return_Item(item, MOD_NONE, returnItem);
    } else if (item == ITEM_BOMB) {
        // "Bomb  Bomb  Bomb  Bomb Bomb   Bomb Bomb"
        osSyncPrintf(" 爆弾  爆弾  爆弾  爆弾 爆弾   爆弾 爆弾 \n");
        if ((AMMO(ITEM_BOMB) += 1) > CUR_CAPACITY(UPG_BOMB_BAG)) {
            AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_BOMBS_5) && (item <= ITEM_BOMBS_30)) {
        if ((AMMO(ITEM_BOMB) += sAmmoRefillCounts[item - ITEM_BOMBS_5]) > CUR_CAPACITY(UPG_BOMB_BAG)) {
            AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOMBCHU) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
            AMMO(ITEM_BOMBCHU) = 10;
        } else {
            AMMO(ITEM_BOMBCHU) += 10;
            
                if (AMMO(ITEM_BOMBCHU) > 50) {
                    AMMO(ITEM_BOMBCHU) = 50;
                }
            
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item == ITEM_BOMBCHUS_5) || (item == ITEM_BOMBCHUS_20)) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(ITEM_BOMBCHU) = ITEM_BOMBCHU;
            AMMO(ITEM_BOMBCHU) += sAmmoRefillCounts[item - ITEM_BOMBCHUS_5 + 8];
        } else {
            AMMO(ITEM_BOMBCHU) += sAmmoRefillCounts[item - ITEM_BOMBCHUS_5 + 8];
            
                if (AMMO(ITEM_BOMBCHU) > 50) {
                    AMMO(ITEM_BOMBCHU) = 50;
                }
            
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_ARROWS_SMALL) && (item <= ITEM_ARROWS_LARGE)) {
        AMMO(ITEM_BOW) += sAmmoRefillCounts[item - ITEM_ARROWS_SMALL + 4];

        if ((AMMO(ITEM_BOW) >= CUR_CAPACITY(UPG_QUIVER)) || (AMMO(ITEM_BOW) < 0)) {
            AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
        }

        osSyncPrintf("%d本  Item_MaxGet=%d\n", AMMO(ITEM_BOW), CUR_CAPACITY(UPG_QUIVER));

        return Return_Item(item, MOD_NONE, ITEM_BOW);
    } else if (item == ITEM_SLINGSHOT) {
        Inventory_ChangeUpgrade(UPG_BULLET_BAG, 1);
        INV_CONTENT(ITEM_SLINGSHOT) = ITEM_SLINGSHOT;
        AMMO(ITEM_SLINGSHOT) = 30;
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_SEEDS) {
        AMMO(ITEM_SLINGSHOT) += 5;

        if (AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG)) {
            AMMO(ITEM_SLINGSHOT) = CUR_CAPACITY(UPG_BULLET_BAG);
        }

        if (!Flags_GetItemGetInf(ITEMGETINF_13)) {
            Flags_SetItemGetInf(ITEMGETINF_13);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, ITEM_SEEDS);
    } else if (item == ITEM_SEEDS_30) {
        AMMO(ITEM_SLINGSHOT) += 30;

        if (AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG)) {
            AMMO(ITEM_SLINGSHOT) = CUR_CAPACITY(UPG_BULLET_BAG);
        }

        if (!Flags_GetItemGetInf(ITEMGETINF_13)) {
            Flags_SetItemGetInf(ITEMGETINF_13);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, ITEM_SEEDS);
    } else if (item == ITEM_OCARINA_FAIRY) {
        INV_CONTENT(ITEM_OCARINA_FAIRY) = ITEM_OCARINA_FAIRY;
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_OCARINA_TIME) {
        INV_CONTENT(ITEM_OCARINA_TIME) = ITEM_OCARINA_TIME;
        // always update "equips" as this is what is currently on the c-buttons
        for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if (gSaveContext.equips.buttonItems[i] == ITEM_OCARINA_FAIRY) {
                gSaveContext.equips.buttonItems[i] = ITEM_OCARINA_TIME;
                Interface_LoadItemIcon1(play, i);
            }
        }

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BEAN) {
        if (gSaveContext.inventory.items[slot] == ITEM_NONE) {
            INV_CONTENT(item) = item;
            AMMO(ITEM_BEAN) = 1;
            BEANS_BOUGHT = 1;
        } else {
            AMMO(ITEM_BEAN)++;
            BEANS_BOUGHT++;
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item == ITEM_HEART_PIECE_2) || (item == ITEM_HEART_PIECE)) {
        gSaveContext.inventory.questItems += 1 << (QUEST_HEART_PIECE + 4);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_HEART_CONTAINER) {
        
            gSaveContext.healthCapacity += FULL_HEART_HEALTH;
            gSaveContext.health += FULL_HEART_HEALTH;
        
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_HEART) {
        osSyncPrintf("回復ハート回復ハート回復ハート\n"); // "Recovery Heart"
        if (play != NULL) {
            Health_ChangeBy(play, FULL_HEART_HEALTH);
        }
        return Return_Item(item, MOD_NONE, item);
    } else if (item == ITEM_MAGIC_SMALL) {
        if (gSaveContext.magicState != MAGIC_STATE_ADD) {
            if (play != NULL) {
                Magic_Fill(play);
            }
        }

        if (play != NULL) {
            Magic_RequestChange(play, 12, MAGIC_ADD);
        }

        if (!Flags_GetInfTable(INFTABLE_198)) {
            Flags_SetInfTable(INFTABLE_198);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, item);
    } else if (item == ITEM_MAGIC_LARGE) {
        if (gSaveContext.magicState != MAGIC_STATE_ADD) {
            if (play != NULL) {
                Magic_Fill(play);
            }
        }
        if (play != NULL) {
            Magic_RequestChange(play, 24, MAGIC_ADD);
        }

        if (!Flags_GetInfTable(INFTABLE_198)) {
            Flags_SetInfTable(INFTABLE_198);
            return Return_Item(item, MOD_NONE, ITEM_NONE);
        }

        return Return_Item(item, MOD_NONE, item);
    } else if ((item >= ITEM_RUPEE_GREEN) && (item <= ITEM_INVALID_8)) {
        Rupees_ChangeBy(sAmmoRefillCounts[item - ITEM_RUPEE_GREEN + 10]);
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if (item == ITEM_BOTTLE) {
        temp = SLOT(item);

        for (i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[temp + i] == ITEM_NONE) {
                gSaveContext.inventory.items[temp + i] = item;
                return Return_Item(item, MOD_NONE, ITEM_NONE);
            }
        }
    } else if (((item >= ITEM_POTION_RED) && (item <= ITEM_POE)) || (item == ITEM_MILK)) {
        temp = SLOT(item);

        if ((item != ITEM_MILK_BOTTLE) && (item != ITEM_LETTER_RUTO)) {
            if (item == ITEM_MILK) {
                item = ITEM_MILK_BOTTLE;
                temp = SLOT(item);
            }

            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_BOTTLE) {
                    // "Item_Pt(1)=%d Item_Pt(2)=%d Item_Pt(3)=%d   Empty Bottle=%d   Content=%d"
                    osSyncPrintf("Item_Pt(1)=%d Item_Pt(2)=%d Item_Pt(3)=%d   空瓶=%d   中味=%d\n",
                                 gSaveContext.equips.cButtonSlots[0], gSaveContext.equips.cButtonSlots[1],
                                 gSaveContext.equips.cButtonSlots[2], temp + i, item);

                    for (int buttonIndex = 1; buttonIndex < ARRAY_COUNT(gSaveContext.equips.buttonItems);
                         buttonIndex++) {
                        if ((temp + i) == gSaveContext.equips.cButtonSlots[buttonIndex - 1]) {
                            gSaveContext.equips.buttonItems[buttonIndex] = item;
                            if (play != NULL) {
                                Interface_LoadItemIcon2(play, buttonIndex);
                            }
                            gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(buttonIndex)] = BTN_ENABLED;
                            break;
                        }
                    }

                    gSaveContext.inventory.items[temp + i] = item;
                    break;
                }
            }
        } else {
            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_NONE) {
                    gSaveContext.inventory.items[temp + i] = item;
                    break;
                }
            }
        }
        return Return_Item(item, MOD_NONE, ITEM_NONE);
    } else if ((item >= ITEM_WEIRD_EGG) && (item <= ITEM_CLAIM_CHECK)) {
        if ((item == ITEM_SAW)) {
            Flags_SetItemGetInf(ITEMGETINF_OBTAINED_NUT_UPGRADE_FROM_STAGE);
        }

        temp = INV_CONTENT(item);
        INV_CONTENT(item) = item;

        if (temp != ITEM_NONE) {
            for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                if (temp == gSaveContext.equips.buttonItems[i]) {
                    if (item != ITEM_SOLD_OUT) {
                        gSaveContext.equips.buttonItems[i] = item;
                        if (play != NULL) {
                            Interface_LoadItemIcon1(play, i);
                        }
                    } else {
                        gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                    }
                    return Return_Item(item, MOD_NONE, ITEM_NONE);
                }
            }
        }

        return Return_Item(item, MOD_NONE, ITEM_NONE);
    }
    returnItem = gSaveContext.inventory.items[slot];
    osSyncPrintf("Item_Register(%d)=%d  %d\n", slot, item, returnItem);
    INV_CONTENT(item) = item;
    return Return_Item(item, MOD_NONE, returnItem);
}

u8 Item_CheckObtainability(u8 item) {
    s16 i;
    s16 slot = SLOT(item);
    s32 temp;

    if (item >= ITEM_STICKS_5) {
        slot = SLOT(sExtraItemBases[item - ITEM_STICKS_5]);
    }

    osSyncPrintf(VT_FGCOL(GREEN));
    osSyncPrintf("item_get_non_setting=%d  pt=%d  z=%x\n", item, slot, gSaveContext.inventory.items[slot]);
    osSyncPrintf(VT_RST);

    if ((item >= ITEM_SONG_MINUET) && (item <= ITEM_SONG_STORMS)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_MEDALLION_FOREST) && (item <= ITEM_MEDALLION_LIGHT)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_KOKIRI_EMERALD) && (item <= ITEM_SKULL_TOKEN)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_BGS)) {
        if (item == ITEM_SWORD_BGS) {
            return ITEM_NONE;
        } else if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, item - ITEM_SWORD_KOKIRI + EQUIP_INV_SWORD_KOKIRI)) {
            return item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item >= ITEM_SHIELD_DEKU) && (item <= ITEM_SHIELD_MIRROR)) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, item - ITEM_SHIELD_DEKU + EQUIP_INV_SHIELD_DEKU)) {
            return item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item >= ITEM_TUNIC_KOKIRI) && (item <= ITEM_TUNIC_ZORA)) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_TUNIC, item - ITEM_TUNIC_KOKIRI + EQUIP_INV_TUNIC_KOKIRI)) {
            return item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item >= ITEM_BOOTS_KOKIRI) && (item <= ITEM_BOOTS_HOVER)) {
        if (CHECK_OWNED_EQUIP(EQUIP_TYPE_BOOTS, item - ITEM_BOOTS_KOKIRI + EQUIP_INV_BOOTS_KOKIRI)) {
            return item;
        } else {
            return ITEM_NONE;
        }
    } else if ((item == ITEM_KEY_BOSS) || (item == ITEM_COMPASS) || (item == ITEM_DUNGEON_MAP)) {
        return ITEM_NONE;
    } else if (item == ITEM_KEY_SMALL) {
        return ITEM_NONE;
    } else if ((item >= ITEM_SLINGSHOT) && (item <= ITEM_BOMBCHU)) {
        return ITEM_NONE;
    } else if ((item == ITEM_BOMBCHUS_5) || (item == ITEM_BOMBCHUS_20)) {
        return ITEM_NONE;
    } else if ((item == ITEM_QUIVER_30) || (item == ITEM_BOW)) {
        if (CUR_UPG_VALUE(UPG_QUIVER) == 0) {
            return ITEM_NONE;
        } else {
            return 0;
        }
    } else if ((item == ITEM_QUIVER_40) || (item == ITEM_QUIVER_50)) {
        return ITEM_NONE;
    } else if ((item == ITEM_BULLET_BAG_40) || (item == ITEM_BULLET_BAG_50)) {
        return ITEM_NONE;
    } else if ((item == ITEM_BOMB_BAG_20) || (item == ITEM_BOMB)) {
        if (CUR_UPG_VALUE(UPG_BOMB_BAG) == 0) {
            return ITEM_NONE;
        } else {
            return 0;
        }
    } else if ((item >= ITEM_STICK_UPGRADE_20) && (item <= ITEM_NUT_UPGRADE_40)) {
        return ITEM_NONE;
    } else if ((item >= ITEM_BOMB_BAG_30) && (item <= ITEM_WALLET_GIANT)) {
        return ITEM_NONE;
    } else if (item == ITEM_LONGSHOT) {
        return ITEM_NONE;
    } else if ((item == ITEM_SEEDS) || (item == ITEM_SEEDS_30)) {
        if (!Flags_GetItemGetInf(ITEMGETINF_13)) {
            return ITEM_NONE;
        } else {
            return ITEM_SEEDS;
        }
    } else if (item == ITEM_BEAN) {
        return ITEM_NONE;
    } else if ((item == ITEM_HEART_PIECE_2) || (item == ITEM_HEART_PIECE)) {
        return ITEM_NONE;
    } else if (item == ITEM_HEART_CONTAINER) {
        return ITEM_NONE;
    } else if (item == ITEM_HEART) {
        return ITEM_HEART;
    } else if ((item == ITEM_MAGIC_SMALL) || (item == ITEM_MAGIC_LARGE)) {
        // "Magic Pot Get_Inf_Table( 25, 0x0100)=%d"
        osSyncPrintf("魔法の壷 Get_Inf_Table( 25, 0x0100)=%d\n", Flags_GetInfTable(INFTABLE_198));
        if (!Flags_GetInfTable(INFTABLE_198)) {
            return ITEM_NONE;
        } else {
            return item;
        }
    } else if ((item >= ITEM_RUPEE_GREEN) && (item <= ITEM_INVALID_8)) {
        return ITEM_NONE;
    } else if (item == ITEM_BOTTLE) {
        return ITEM_NONE;
    } else if (((item >= ITEM_POTION_RED) && (item <= ITEM_POE)) || (item == ITEM_MILK)) {
        temp = SLOT(item);

        if ((item != ITEM_MILK_BOTTLE) && (item != ITEM_LETTER_RUTO)) {
            if (item == ITEM_MILK) {
                item = ITEM_MILK_BOTTLE;
                temp = SLOT(item);
            }

            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_BOTTLE) {
                    return ITEM_NONE;
                }
            }
        } else {
            for (i = 0; i < 4; i++) {
                if (gSaveContext.inventory.items[temp + i] == ITEM_NONE) {
                    return ITEM_NONE;
                }
            }
        }
    } else if ((item >= ITEM_WEIRD_EGG) && (item <= ITEM_CLAIM_CHECK)) {
        return ITEM_NONE;
    }

    return gSaveContext.inventory.items[slot];
}

void Inventory_DeleteItem(u16 item, u16 invSlot) {
    s16 i;

    if (item == ITEM_BEAN) {
        BEANS_BOUGHT = 0;
    }

    gSaveContext.inventory.items[invSlot] = ITEM_NONE;

    osSyncPrintf("\nItem_Register(%d)\n", invSlot, gSaveContext.inventory.items[invSlot]);

    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        if (gSaveContext.equips.buttonItems[i] == item) {
            gSaveContext.equips.buttonItems[i] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[i - 1] = SLOT_NONE;
        }
    }
}

s32 Inventory_ReplaceItem(PlayState* play, u16 oldItem, u16 newItem) {
    s16 i;

    for (i = 0; i < ARRAY_COUNT(gSaveContext.inventory.items); i++) {
        if (gSaveContext.inventory.items[i] == oldItem) {
            gSaveContext.inventory.items[i] = newItem;
            osSyncPrintf("アイテム消去(%d)\n", i); // "Item Purge (%d)"
            for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                if (gSaveContext.equips.buttonItems[i] == oldItem) {
                    gSaveContext.equips.buttonItems[i] = newItem;
                    Interface_LoadItemIcon1(play, i);
                    break;
                }
            }
            return 1;
        }
    }

    return 0;
}

s32 Inventory_HasEmptyBottle(void) {
    u8* items = gSaveContext.inventory.items;

    if (items[SLOT_BOTTLE_1] == ITEM_BOTTLE) {
        return 1;
    } else if (items[SLOT_BOTTLE_2] == ITEM_BOTTLE) {
        return 1;
    } else if (items[SLOT_BOTTLE_3] == ITEM_BOTTLE) {
        return 1;
    } else if (items[SLOT_BOTTLE_4] == ITEM_BOTTLE) {
        return 1;
    } else {
        return 0;
    }
}

bool Inventory_HasEmptyBottleSlot(void) {
    u8* items = gSaveContext.inventory.items;

    return (items[SLOT_BOTTLE_1] == ITEM_NONE || items[SLOT_BOTTLE_2] == ITEM_NONE ||
            items[SLOT_BOTTLE_3] == ITEM_NONE || items[SLOT_BOTTLE_4] == ITEM_NONE);
}

s32 Inventory_HasSpecificBottle(u8 bottleItem) {
    u8* items = gSaveContext.inventory.items;

    if (items[SLOT_BOTTLE_1] == bottleItem) {
        return 1;
    } else if (items[SLOT_BOTTLE_2] == bottleItem) {
        return 1;
    } else if (items[SLOT_BOTTLE_3] == bottleItem) {
        return 1;
    } else if (items[SLOT_BOTTLE_4] == bottleItem) {
        return 1;
    } else {
        return 0;
    }
}

void Inventory_UpdateBottleItem(PlayState* play, u8 item, u8 button) {
    osSyncPrintf("item_no=%x,  c_no=%x,  Pt=%x  Item_Register=%x\n", item, button,
                 gSaveContext.equips.cButtonSlots[button - 1],
                 gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[button - 1]]);

    // Special case to only empty half of a Lon Lon Milk Bottle
    if ((gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[button - 1]] == ITEM_MILK_BOTTLE) &&
        (item == ITEM_BOTTLE)) {
        item = ITEM_MILK_HALF;
    }

    
        gSaveContext.inventory.items[gSaveContext.equips.cButtonSlots[button - 1]] = item;
    

    gSaveContext.equips.buttonItems[button] = item;

    Interface_LoadItemIcon1(play, button);

    play->pauseCtx.cursorItem[PAUSE_ITEM] = item;
    gSaveContext.buttonStatus[BUTTON_STATUS_INDEX(button)] = BTN_ENABLED;
}

s32 Inventory_ConsumeFairy(PlayState* play) {
    s32 bottleSlot = SLOT(ITEM_FAIRY);
    s16 i;
    s16 j;

    for (i = 0; i < 4; i++) {
        if (gSaveContext.inventory.items[bottleSlot + i] == ITEM_FAIRY) {
            for (j = 1; j < ARRAY_COUNT(gSaveContext.equips.buttonItems); j++) {
                if (gSaveContext.equips.buttonItems[j] == ITEM_FAIRY) {
                    gSaveContext.equips.buttonItems[j] = ITEM_BOTTLE;
                    Interface_LoadItemIcon1(play, j);
                    i = 0;
                    bottleSlot = gSaveContext.equips.cButtonSlots[j - 1];
                    break;
                }
            }
            osSyncPrintf("妖精使用＝%d\n", bottleSlot); // "Fairy Usage＝%d"
            gSaveContext.inventory.items[bottleSlot + i] = ITEM_BOTTLE;
            return 1;
        }
    }

    return 0;
}

bool Inventory_HatchPocketCucco(PlayState* play) {
    return Inventory_ReplaceItem(play, ITEM_POCKET_EGG, ITEM_POCKET_CUCCO);
}

void func_80086D5C(s32* buf, u16 size) {
    for (u16 i = 0; i < size; i++) {
        buf[i] = 0;
    }
}

void Interface_LoadActionLabel(InterfaceContext* interfaceCtx, u16 action, s16 loadOffset) {
    static void* sDoActionTextures[] = { gAttackDoActionENGTex, gCheckDoActionENGTex };
    if (action >= DO_ACTION_MAX) {
        action = DO_ACTION_NONE;
    }

    char* doAction = actionsTbl[action];

    static char newName[4][512];
    if (gSaveContext.language != LANGUAGE_ENG) {
        size_t length = strlen(doAction);
        strcpy(newName[loadOffset], doAction);
        if (gSaveContext.language == LANGUAGE_FRA) {
            newName[loadOffset][length - 6] = 'F';
            newName[loadOffset][length - 5] = 'R';
            newName[loadOffset][length - 4] = 'A';
        } else if (gSaveContext.language == LANGUAGE_GER) {
            newName[loadOffset][length - 6] = 'G';
            newName[loadOffset][length - 5] = 'E';
            newName[loadOffset][length - 4] = 'R';
        } else if (gSaveContext.language == LANGUAGE_JPN) {
            newName[loadOffset][length - 6] = 'J';
            newName[loadOffset][length - 5] = 'P';
            newName[loadOffset][length - 4] = 'N';
        }
        doAction = newName[loadOffset];
    }

    char* segment = interfaceCtx->doActionSegment[loadOffset];
    interfaceCtx->doActionSegment[loadOffset] = action != DO_ACTION_NONE ? doAction : gEmptyTexture;
    gSegments[7] = interfaceCtx->doActionSegment[loadOffset];
}

void Interface_SetDoAction(PlayState* play, u16 action) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    PauseContext* pauseCtx = &play->pauseCtx;

    if (interfaceCtx->unk_1F0 != action) {
        interfaceCtx->unk_1F0 = action;
        interfaceCtx->unk_1EC = 1;
        interfaceCtx->unk_1F4 = 0.0f;
        Interface_LoadActionLabel(interfaceCtx, action, 1);
        if (pauseCtx->state != 0) {
            interfaceCtx->unk_1EC = 3;
        }
    }
}

void Interface_LoadActionLabelB(PlayState* play, u16 action) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    char* doAction = actionsTbl[action];
    static char newName[512];

    if (gSaveContext.language != LANGUAGE_ENG) {
        size_t length = strlen(doAction);
        strcpy(newName, doAction);
        if (gSaveContext.language == LANGUAGE_FRA) {
            newName[length - 6] = 'F';
            newName[length - 5] = 'R';
            newName[length - 4] = 'A';
        } else if (gSaveContext.language == LANGUAGE_GER) {
            newName[length - 6] = 'G';
            newName[length - 5] = 'E';
            newName[length - 4] = 'R';
        } else if (gSaveContext.language == LANGUAGE_JPN) {
            newName[length - 6] = 'J';
            newName[length - 5] = 'P';
            newName[length - 4] = 'N';
        }
        doAction = newName;
    }

    interfaceCtx->unk_1FC = action;

    char* segment = interfaceCtx->doActionSegment[1];
    interfaceCtx->doActionSegment[1] = action != DO_ACTION_NONE ? doAction : gEmptyTexture;
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);

    interfaceCtx->unk_1FA = 1;
}

s32 Health_ChangeBy(PlayState* play, s16 healthChange) {
    u16 heartCount;
    u16 healthLevel;

    // "＊＊＊＊＊ Fluctuation=%d (now=%d, max=%d) ＊＊＊"
    osSyncPrintf("＊＊＊＊＊  増減=%d (now=%d, max=%d)  ＊＊＊", healthChange, gSaveContext.health,
                 gSaveContext.healthCapacity);

    // clang-format off
    if (healthChange > 0) { Audio_PlaySoundGeneral(NA_SE_SY_HP_RECOVER, &gSfxDefaultPos, 4,
                                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    } else if ((gSaveContext.isDoubleDefenseAcquired != 0) && (healthChange < 0)) {
        healthChange >>= 1;
        osSyncPrintf("ハート減少半分！！＝%d\n", healthChange); // "Heart decrease halved!!＝%d"
    }
    // clang-format on

    gSaveContext.health += healthChange;

    if (gSaveContext.health > gSaveContext.healthCapacity) {
        gSaveContext.health = gSaveContext.healthCapacity;
    }

    heartCount = gSaveContext.health % FULL_HEART_HEALTH;

    healthLevel = heartCount;
    if (heartCount != 0) {
        if (heartCount > 10) {
            healthLevel = 3;
        } else if (heartCount > 5) {
            healthLevel = 2;
        } else {
            healthLevel = 1;
        }
    }

    // "Life=%d ＊＊＊  %d ＊＊＊＊＊＊"
    osSyncPrintf("  ライフ=%d  ＊＊＊  %d  ＊＊＊＊＊＊\n", gSaveContext.health, healthLevel);


    if (gSaveContext.health <= 0) {
        gSaveContext.health = 0;
        return 0;
    } else {
        return 1;
    }
}

void Rupees_ChangeBy(s64 rupeeChange) {
    if (rupeeChange > 0) {
        if (gSaveContext.rupees > RUPEE_BALANCE_MAX - rupeeChange) {
            gSaveContext.rupees = RUPEE_BALANCE_MAX;
        } else {
            gSaveContext.rupees += rupeeChange;
        }
    } else {
        if (rupeeChange < -gSaveContext.rupees) {
            gSaveContext.rupees = 0;
        } else {
            gSaveContext.rupees += rupeeChange;
        }
    }

    // Currency changes are committed atomically so large multiplayer trades
    // never spend minutes draining through the original one-rupee counter.
    gSaveContext.rupeeAccumulator = 0;
}

void Inventory_ChangeAmmo(s16 item, s16 ammoChange) {
    // "Item = (%d)    Amount = (%d + %d)"
    osSyncPrintf("アイテム = (%d)    数 = (%d + %d)  ", item, AMMO(item), ammoChange);

    if (item == ITEM_STICK) {
        AMMO(ITEM_STICK) += ammoChange;

        if (AMMO(ITEM_STICK) >= CUR_CAPACITY(UPG_STICKS)) {
            AMMO(ITEM_STICK) = CUR_CAPACITY(UPG_STICKS);
        } else if (AMMO(ITEM_STICK) < 0) {
            AMMO(ITEM_STICK) = 0;
        }
    } else if (item == ITEM_NUT) {
        AMMO(ITEM_NUT) += ammoChange;

        if (AMMO(ITEM_NUT) >= CUR_CAPACITY(UPG_NUTS)) {
            AMMO(ITEM_NUT) = CUR_CAPACITY(UPG_NUTS);
        } else if (AMMO(ITEM_NUT) < 0) {
            AMMO(ITEM_NUT) = 0;
        }
    } else if (item == ITEM_BOMBCHU) {
        AMMO(ITEM_BOMBCHU) += ammoChange;

        
            if (AMMO(ITEM_BOMBCHU) > 50) {
                AMMO(ITEM_BOMBCHU) = 50;
            }
        
    } else if (item == ITEM_BOW) {
        AMMO(ITEM_BOW) += ammoChange;

        if (AMMO(ITEM_BOW) >= CUR_CAPACITY(UPG_QUIVER)) {
            AMMO(ITEM_BOW) = CUR_CAPACITY(UPG_QUIVER);
        } else if (AMMO(ITEM_BOW) < 0) {
            AMMO(ITEM_BOW) = 0;
        }
    } else if ((item == ITEM_SLINGSHOT) || (item == ITEM_SEEDS)) {
        AMMO(ITEM_SLINGSHOT) += ammoChange;

        if (AMMO(ITEM_SLINGSHOT) >= CUR_CAPACITY(UPG_BULLET_BAG)) {
            AMMO(ITEM_SLINGSHOT) = CUR_CAPACITY(UPG_BULLET_BAG);
        } else if (AMMO(ITEM_SLINGSHOT) < 0) {
            AMMO(ITEM_SLINGSHOT) = 0;
        }
    } else if (item == ITEM_BOMB) {
        AMMO(ITEM_BOMB) += ammoChange;

        if (AMMO(ITEM_BOMB) >= CUR_CAPACITY(UPG_BOMB_BAG)) {
            AMMO(ITEM_BOMB) = CUR_CAPACITY(UPG_BOMB_BAG);
        } else if (AMMO(ITEM_BOMB) < 0) {
            AMMO(ITEM_BOMB) = 0;
        }
    } else if (item == ITEM_BEAN) {
        AMMO(ITEM_BEAN) += ammoChange;
    }

    osSyncPrintf("合計 = (%d)\n", AMMO(item)); // "Total = (%d)"

}

void Magic_Fill(PlayState* play) {
    if (gSaveContext.isMagicAcquired) {
        gSaveContext.prevMagicState = gSaveContext.magicState;
        gSaveContext.magicFillTarget = (gSaveContext.isDoubleMagicAcquired + 1) * MAGIC_NORMAL_METER;
        gSaveContext.magicState = MAGIC_STATE_FILL;
    }
}

void Magic_Reset(PlayState* play) {
    if ((gSaveContext.magicState != MAGIC_STATE_STEP_CAPACITY) && (gSaveContext.magicState != MAGIC_STATE_FILL)) {
        if (gSaveContext.magicState == MAGIC_STATE_ADD) {
            gSaveContext.prevMagicState = gSaveContext.magicState;
        }
        gSaveContext.magicState = MAGIC_STATE_RESET;
    }
}

s32 Magic_RequestChange(PlayState* play, s16 amount, s16 type) {
    if (!gSaveContext.isMagicAcquired) {
        return false;
    }

    if ((type != 5) && (gSaveContext.magic - amount) < 0) {
        if (gSaveContext.magicCapacity != 0) {
            Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        return false;
    }

    switch (type) {
        case MAGIC_CONSUME_NOW:
        case MAGIC_CONSUME_NOW_ALT:
            if ((gSaveContext.magicState == MAGIC_STATE_IDLE) ||
                (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                gSaveContext.magicTarget = gSaveContext.magic - amount;
                gSaveContext.magicState = MAGIC_STATE_CONSUME_SETUP;
                return 1;
            } else {
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                return false;
            }
        case MAGIC_CONSUME_WAIT_NO_PREVIEW:
            if ((gSaveContext.magicState == MAGIC_STATE_IDLE) ||
                (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                gSaveContext.magicTarget = gSaveContext.magic - amount;
                gSaveContext.magicState = MAGIC_STATE_METER_FLASH_3;
                return true;
            } else {
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                return false;
            }
        case MAGIC_CONSUME_LENS:
            if (gSaveContext.magicState == MAGIC_STATE_IDLE) {
                if (gSaveContext.magic != 0) {
                    play->interfaceCtx.unk_230 = 80;
                    gSaveContext.magicState = MAGIC_STATE_CONSUME_LENS;
                    return true;
                } else {
                    return false;
                }
            } else {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    return true;
                } else {
                    return false;
                }
            }
        case MAGIC_CONSUME_WAIT_PREVIEW:
            if ((gSaveContext.magicState == MAGIC_STATE_IDLE) ||
                (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) {
                if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
                    play->actorCtx.lensActive = false;
                }
                gSaveContext.magicTarget = gSaveContext.magic - amount;
                gSaveContext.magicState = MAGIC_STATE_METER_FLASH_2;
                return true;
            } else {
                Audio_PlaySoundGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                return false;
            }
        case MAGIC_ADD:
            if (gSaveContext.magicCapacity >= gSaveContext.magic) {
                gSaveContext.magicTarget = gSaveContext.magic + amount;

                if (gSaveContext.magicTarget >= gSaveContext.magicCapacity) {
                    gSaveContext.magicTarget = gSaveContext.magicCapacity;
                }

                gSaveContext.magicState = MAGIC_STATE_ADD;
                return true;
            }
            break;
    }

    return 0;
}

void Interface_UpdateMagicBar(PlayState* play) {
    static s16 sMagicBorderColors[][3] = {
        { 255, 255, 255 },
        { 150, 150, 150 },
        { 255, 255, 150 },
        { 255, 255, 50 },
    };
    Color_RGB8 MagicBorder_0 = { 255, 255, 255 };
    Color_RGB8 MagicBorder_1 = { 150, 150, 150 };
    Color_RGB8 MagicBorder_2 = { 255, 255, 150 };
    Color_RGB8 MagicBorder_3 = { 255, 255, 50 };

    

    static s16 sMagicBorderIndexes[] = { 0, 1, 1, 0 };
    static s16 sMagicBorderRatio = 2;
    static s16 sMagicBorderStep = 1;
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 borderChangeR;
    s16 borderChangeG;
    s16 borderChangeB;
    s16 temp;

    switch (gSaveContext.magicState) {
        case MAGIC_STATE_STEP_CAPACITY:
            temp = gSaveContext.magicLevel * MAGIC_NORMAL_METER;
            if (gSaveContext.magicCapacity != temp) {
                if (gSaveContext.magicCapacity < temp) {
                    gSaveContext.magicCapacity += 8;
                    if (gSaveContext.magicCapacity > temp) {
                        gSaveContext.magicCapacity = temp;
                    }
                } else {
                    gSaveContext.magicCapacity -= 8;
                    if (gSaveContext.magicCapacity <= temp) {
                        gSaveContext.magicCapacity = temp;
                    }
                }
            } else {
                gSaveContext.magicState = MAGIC_STATE_FILL;
            }
            break;

        case MAGIC_STATE_FILL:
            gSaveContext.magic += 4;

            if (gSaveContext.gameMode == GAMEMODE_NORMAL && gSaveContext.sceneSetupIndex < 4) {
                Audio_PlaySoundGeneral(NA_SE_SY_GAUGE_UP - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }

            // "Storage  MAGIC_NOW=%d (%d)"
            osSyncPrintf("蓄電  MAGIC_NOW=%d (%d)\n", gSaveContext.magic, gSaveContext.magicFillTarget);
            if (gSaveContext.magic >= gSaveContext.magicFillTarget) {
                gSaveContext.magic = gSaveContext.magicFillTarget;
                gSaveContext.magicState = gSaveContext.prevMagicState;
                gSaveContext.prevMagicState = 0;
            }
            break;

        case MAGIC_STATE_CONSUME_SETUP:
            sMagicBorderRatio = 2;
            gSaveContext.magicState = MAGIC_STATE_CONSUME;
            break;

        case MAGIC_STATE_CONSUME:
            {
                gSaveContext.magic -= 2;
                if (gSaveContext.magic <= 0) {
                    gSaveContext.magic = 0;
                    gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
                    
                        sMagicBorder = sMagicBorder_ori;
                    
                } else if (gSaveContext.magic == gSaveContext.magicTarget) {
                    gSaveContext.magicState = MAGIC_STATE_METER_FLASH_1;
                    
                        sMagicBorder = sMagicBorder_ori;
                    
                }
            }
        case MAGIC_STATE_METER_FLASH_1:
        case MAGIC_STATE_METER_FLASH_2:
        case MAGIC_STATE_METER_FLASH_3:
            temp = sMagicBorderIndexes[sMagicBorderStep];
            borderChangeR = ABS(sMagicBorder.r - sMagicBorderColors[temp][0]) / sMagicBorderRatio;
            borderChangeG = ABS(sMagicBorder.g - sMagicBorderColors[temp][1]) / sMagicBorderRatio;
            borderChangeB = ABS(sMagicBorder.b - sMagicBorderColors[temp][2]) / sMagicBorderRatio;

            if (sMagicBorder.r >= sMagicBorderColors[temp][0]) {
                sMagicBorder.r -= borderChangeR;
            } else {
                sMagicBorder.r += borderChangeR;
            }

            if (sMagicBorder.g >= sMagicBorderColors[temp][1]) {
                sMagicBorder.g -= borderChangeG;
            } else {
                sMagicBorder.g += borderChangeG;
            }

            if (sMagicBorder.b >= sMagicBorderColors[temp][2]) {
                sMagicBorder.b -= borderChangeB;
            } else {
                sMagicBorder.b += borderChangeB;
            }

            sMagicBorderRatio--;
            if (sMagicBorderRatio == 0) {
                sMagicBorder.r = sMagicBorderColors[temp][0];
                sMagicBorder.g = sMagicBorderColors[temp][1];
                sMagicBorder.b = sMagicBorderColors[temp][2];
                sMagicBorderRatio = YREG(40 + sMagicBorderStep);
                sMagicBorderStep++;
                if (sMagicBorderStep >= 4) {
                    sMagicBorderStep = 0;
                }
            }
            break;

        case MAGIC_STATE_RESET:
            
                sMagicBorder = sMagicBorder_ori;
            
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            break;

        case MAGIC_STATE_CONSUME_LENS:
            if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (msgCtx->msgMode == MSGMODE_NONE) &&
                (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
                (play->transitionMode == TRANS_MODE_OFF) && !Play_InCsMode(play)) {
                bool hasLens = false;
                for (int buttonIndex = 1; buttonIndex < 4; buttonIndex++) {
                    if (gSaveContext.equips.buttonItems[buttonIndex] == ITEM_LENS) {
                        hasLens = true;
                        break;
                    }
                }
                if ((gSaveContext.magic == 0) ||
                    ((Player_GetEnvironmentalHazard(play) >= 2) && (Player_GetEnvironmentalHazard(play) < 5)) ||
                    !hasLens || !play->actorCtx.lensActive) {
                    play->actorCtx.lensActive = false;
                    Audio_PlaySoundGeneral(NA_SE_SY_GLASSMODE_OFF, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                    gSaveContext.magicState = MAGIC_STATE_IDLE;
                    
                        sMagicBorder = sMagicBorder_ori;
                    
                    break;
                }

                interfaceCtx->unk_230--;
                if (interfaceCtx->unk_230 == 0) {
                    gSaveContext.magic--;
                    interfaceCtx->unk_230 = 80;
                }
            }

            temp = sMagicBorderIndexes[sMagicBorderStep];
            borderChangeR = ABS(sMagicBorder.r - sMagicBorderColors[temp][0]) / sMagicBorderRatio;
            borderChangeG = ABS(sMagicBorder.g - sMagicBorderColors[temp][1]) / sMagicBorderRatio;
            borderChangeB = ABS(sMagicBorder.b - sMagicBorderColors[temp][2]) / sMagicBorderRatio;

            if (sMagicBorder.r >= sMagicBorderColors[temp][0]) {
                sMagicBorder.r -= borderChangeR;
            } else {
                sMagicBorder.r += borderChangeR;
            }

            if (sMagicBorder.g >= sMagicBorderColors[temp][1]) {
                sMagicBorder.g -= borderChangeG;
            } else {
                sMagicBorder.g += borderChangeG;
            }

            if (sMagicBorder.b >= sMagicBorderColors[temp][2]) {
                sMagicBorder.b -= borderChangeB;
            } else {
                sMagicBorder.b += borderChangeB;
            }

            sMagicBorderRatio--;
            if (sMagicBorderRatio == 0) {
                sMagicBorder.r = sMagicBorderColors[temp][0];
                sMagicBorder.g = sMagicBorderColors[temp][1];
                sMagicBorder.b = sMagicBorderColors[temp][2];
                sMagicBorderRatio = YREG(40 + sMagicBorderStep);
                sMagicBorderStep++;
                if (sMagicBorderStep >= 4) {
                    sMagicBorderStep = 0;
                }
            }
            break;

        case MAGIC_STATE_ADD:
            gSaveContext.magic += 4;
            Audio_PlaySoundGeneral(NA_SE_SY_GAUGE_UP - SFX_FLAG, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            if (gSaveContext.magic >= gSaveContext.magicTarget) {
                gSaveContext.magic = gSaveContext.magicTarget;
                gSaveContext.magicState = gSaveContext.prevMagicState;
                gSaveContext.prevMagicState = 0;
            }
            break;

        default:
            gSaveContext.magicState = MAGIC_STATE_IDLE;
            
                sMagicBorder = sMagicBorder_ori;
            
            break;
    }
}

void Interface_DrawLineupTick(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_39Overlay(play->state.gfxCtx);

    gDPSetEnvColor(OVERLAY_DISP++, 255, 255, 255, 255);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);

    s16 width = 32;
    s16 height = 32;
    s16 x = -8 + (SCREEN_WIDTH / 2);
    s16 y = -6;

    OVERLAY_DISP =
        Gfx_TextureIA8(OVERLAY_DISP, gEmptyCDownArrowTex, width, height, x, y, width, height, 2 << 10, 2 << 10);

    gDPPipeSync(OVERLAY_DISP++);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Interface_DrawMagicBar(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 magicDrop = R_MAGIC_BAR_LARGE_Y - R_MAGIC_BAR_SMALL_Y + 2;
    s16 magicBarY;
    Color_RGB8 magicbar_yellow = { 250, 250, 0 }; // Magic bar being used
    Color_RGB8 magicbar_green = { R_MAGIC_FILL_COLOR(0), R_MAGIC_FILL_COLOR(1),
                                  R_MAGIC_FILL_COLOR(2) }; // Magic bar fill
    Color_RGB8 magicbar_blue = { 0, 0, 200 };              // Infinite magic bar

    
    
    OPEN_DISPS(play->state.gfxCtx);

    if (gSaveContext.magicLevel != 0) {
        s16 X_Margins;
        s16 Y_Margins;
        
            X_Margins = 0;
            Y_Margins = 0;
        
        const s16 magicBarY_original_l = R_MAGIC_BAR_LARGE_Y + Y_Margins;
        const s16 magicBarY_original_s = R_MAGIC_BAR_SMALL_Y + Y_Margins;
        const s16 PosX_Start_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X + X_Margins);
        const s16 PosX_MidEnd_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X + X_Margins + 8);
        const s16 rMagicBarX_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_BAR_X + X_Margins);
        const s16 rMagicFillX_original = OTRGetRectDimensionFromLeftEdge(R_MAGIC_FILL_X + X_Margins);
        s16 PosX_Start;
        s16 magicBarY;
        s16 rMagicBarX;
        s16 PosX_MidEnd;
        s16 rMagicFillX;
        s32 lineLength = (10);
        
            if ((gSaveContext.healthCapacity - 1) / FULL_HEART_HEALTH >= lineLength && lineLength != 0) {
                magicBarY =
                    magicBarY_original_l +
                    magicDrop * (lineLength == 0 ? 0 : ((gSaveContext.healthCapacity - 1) / (0x10 * lineLength) - 1));
            } else {
                magicBarY = magicBarY_original_s;
            }
            PosX_Start = PosX_Start_original;
            rMagicBarX = rMagicBarX_original;
            PosX_MidEnd = PosX_MidEnd_original;
            rMagicFillX = rMagicFillX_original;
        

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, sMagicBorder.r, sMagicBorder.g, sMagicBorder.b, interfaceCtx->magicAlpha);
        gDPSetEnvColor(OVERLAY_DISP++, 100, 50, 50, 255);

        OVERLAY_DISP =
            Gfx_TextureIA8(OVERLAY_DISP, gMagicMeterEndTex, 8, 16, PosX_Start, magicBarY, 8, 16, 1 << 10, 1 << 10);

        OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gMagicMeterMidTex, 24, 16, PosX_MidEnd, magicBarY,
                                      gSaveContext.magicCapacity, 16, 1 << 10, 1 << 10);

        gDPLoadTextureBlock(OVERLAY_DISP++, gMagicMeterEndTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 16, 0,
                            G_TX_MIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 3, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

        gSPWideTextureRectangle(OVERLAY_DISP++, ((rMagicBarX + gSaveContext.magicCapacity) + 8) << 2, magicBarY << 2,
                                ((rMagicBarX + gSaveContext.magicCapacity) + 16) << 2, (magicBarY + 16) << 2,
                                G_TX_RENDERTILE, 256, 0, 1 << 10, 1 << 10);

        gDPPipeSync(OVERLAY_DISP++);
        gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE, PRIMITIVE,
                          ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE);
        gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

        if (gSaveContext.magicState == MAGIC_STATE_METER_FLASH_2) {
            // Yellow part of the bar indicating the amount of magic to be subtracted
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_yellow.r, magicbar_yellow.g, magicbar_yellow.b,
                            interfaceCtx->magicAlpha);

            gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                                 G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                 G_TX_NOLOD, G_TX_NOLOD);

            gSPWideTextureRectangle(OVERLAY_DISP++, rMagicFillX << 2, (magicBarY + 3) << 2,
                                    (rMagicFillX + gSaveContext.magic) << 2, (magicBarY + 10) << 2, G_TX_RENDERTILE, 0,
                                    0, 1 << 10, 1 << 10);

            // Fill the rest of the bar with the normal magic color
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_green.r, magicbar_green.g, magicbar_green.b,
                            interfaceCtx->magicAlpha);

            gSPWideTextureRectangle(OVERLAY_DISP++, rMagicFillX << 2, (magicBarY + 3) << 2,
                                    (rMagicFillX + gSaveContext.magicTarget) << 2, (magicBarY + 10) << 2,
                                    G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        } else {
            // Fill the whole bar with the normal magic color
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicbar_green.r, magicbar_green.g, magicbar_green.b,
                            interfaceCtx->magicAlpha);

            gDPLoadMultiBlock_4b(OVERLAY_DISP++, gMagicMeterFillTex, 0, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                                 G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                 G_TX_NOLOD, G_TX_NOLOD);

            gSPWideTextureRectangle(OVERLAY_DISP++, rMagicFillX << 2, (magicBarY + 3) << 2,
                                    (rMagicFillX + gSaveContext.magic) << 2, (magicBarY + 10) << 2, G_TX_RENDERTILE, 0,
                                    0, 1 << 10, 1 << 10);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Interface_SetSubTimer(s16 seconds) {
    gSaveContext.timerX[TIMER_ID_SUB] = 140;
    gSaveContext.timerY[TIMER_ID_SUB] = 80;
    sEnvHazardActive = false;
    gSaveContext.subTimerSeconds = seconds;

    if (seconds) {
        gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_INIT;
    } else {
        gSaveContext.subTimerState = SUBTIMER_STATE_UP_INIT;
    }
}

void Interface_SetSubTimerToFinalSecond(PlayState* play) {
    if (gSaveContext.subTimerState != SUBTIMER_STATE_OFF) {
        if (gSaveContext.eventInf[1] & 1) {
            gSaveContext.subTimerSeconds = 239;
        } else {
            gSaveContext.subTimerSeconds = 1;
        }
    }
}

void Interface_SetTimer(s16 seconds) {
    gSaveContext.timerX[TIMER_ID_MAIN] = 140;
    gSaveContext.timerY[TIMER_ID_MAIN] = 80;
    sEnvHazardActive = false;
    gSaveContext.timerSeconds = seconds;

    if (seconds) {
        gSaveContext.timerState = TIMER_STATE_DOWN_INIT;
    } else {
        gSaveContext.timerState = TIMER_STATE_UP_INIT;
    }
}

void Interface_DrawActionLabel(GraphicsContext* gfxCtx, void* texture) {
    OPEN_DISPS(gfxCtx);

    gDPLoadTextureBlock_4b(OVERLAY_DISP++, texture, G_IM_FMT_IA, DO_ACTION_TEX_WIDTH(), DO_ACTION_TEX_HEIGHT(), 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);

    gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);

    CLOSE_DISPS(gfxCtx);
}

void Interface_DrawItemButtons(PlayState* play) {
    static s16 startButtonLeftPos[] = { 132, 130, 130, 132 };
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    PauseContext* pauseCtx = &play->pauseCtx;
    s16 temp; // Used as both an alpha value and a button index
    s16 dxdy;
    s16 width;
    s16 height;

    Color_RGB8 bButtonColor = { 0, 150, 0 };
    

    Color_RGB8 cButtonsColor = { 255, 160, 0 };
    
    Color_RGB8 cUpButtonColor = cButtonsColor;
    
    Color_RGB8 cDownButtonColor = cButtonsColor;
    
    Color_RGB8 cLeftButtonColor = cButtonsColor;
    
    Color_RGB8 cRightButtonColor = cButtonsColor;
    

    Color_RGB8 startButtonColor = { 200, 0, 0 };
    

    // B Button
    s16 X_Margins_BtnB;
    s16 Y_Margins_BtnB;
    s16 BBtn_Size = 32;
    int BBtnScaled = BBtn_Size * 0.95f;
    
    int BBtn_factor = (1 << 10) * BBtn_Size / BBtnScaled;
    
        X_Margins_BtnB = 0;
        Y_Margins_BtnB = 0;
    
    s16 PosX_BtnB_ori = OTRGetRectDimensionFromRightEdge(R_ITEM_BTN_X(0) + X_Margins_BtnB);
    s16 PosY_BtnB_ori = R_ITEM_BTN_Y(0) + Y_Margins_BtnB;
    s16 PosX_BtnB;
    s16 PosY_BtnB;
    
        PosY_BtnB = PosY_BtnB_ori;
        PosX_BtnB = PosX_BtnB_ori;
    
    // Start Button
    s16 X_Margins_StartBtn;
    s16 Y_Margins_StartBtn;
    
        X_Margins_StartBtn = 0;
        Y_Margins_StartBtn = 0;
    
    s16 StartBtn_Icon_H = 32;
    s16 StartBtn_Icon_W = 32;
    float Start_BTN_Scale = 0.75f;
    
    int StartBTN_H_Scaled = StartBtn_Icon_H * Start_BTN_Scale;
    int StartBTN_W_Scaled = StartBtn_Icon_W * Start_BTN_Scale;
    int StartBTN_W_factor = (1 << 10) * StartBtn_Icon_W / StartBTN_W_Scaled;
    int StartBTN_H_factor = (1 << 10) * StartBtn_Icon_H / StartBTN_H_Scaled;
    const s16 PosX_StartBtn_ori =
        OTRGetRectDimensionFromRightEdge(startButtonLeftPos[gSaveContext.language] + X_Margins_StartBtn);
    const s16 PosY_StartBtn_ori = 16 + Y_Margins_StartBtn;
    s16 StartBTN_Label_W = DO_ACTION_TEX_WIDTH();
    s16 StartBTN_Label_H = DO_ACTION_TEX_HEIGHT();
    s16 PosX_StartBtn;
    s16 PosY_StartBtn;
    
        StartBTN_H_Scaled = StartBtn_Icon_H * 0.75f;
        StartBTN_W_Scaled = StartBtn_Icon_W * 0.75f;
        StartBTN_W_factor = (1 << 10) * StartBtn_Icon_W / StartBTN_W_Scaled;
        StartBTN_H_factor = (1 << 10) * StartBtn_Icon_H / StartBTN_H_Scaled;
        PosY_StartBtn = PosY_StartBtn_ori;
        PosX_StartBtn = PosX_StartBtn_ori;
    
    // C Buttons position
    s16 X_Margins_CL;
    s16 X_Margins_CR;
    s16 X_Margins_CU;
    s16 X_Margins_CD;
    s16 Y_Margins_CL;
    s16 Y_Margins_CR;
    s16 Y_Margins_CU;
    s16 Y_Margins_CD;
    
        X_Margins_CL = 0;
        Y_Margins_CL = 0;
    
    
        X_Margins_CR = 0;
        Y_Margins_CR = 0;
    
    
        X_Margins_CU = 0;
        Y_Margins_CU = 0;
    
    
        X_Margins_CD = 0;
        Y_Margins_CD = 0;
    
    const s16 C_Left_BTN_Pos_ori[] = { C_LEFT_BUTTON_X + X_Margins_CL, C_LEFT_BUTTON_Y + Y_Margins_CL }; //(X,Y)
    const s16 C_Right_BTN_Pos_ori[] = { C_RIGHT_BUTTON_X + X_Margins_CR, C_RIGHT_BUTTON_Y + Y_Margins_CR };
    const s16 C_Up_BTN_Pos_ori[] = { C_UP_BUTTON_X + X_Margins_CU, C_UP_BUTTON_Y + Y_Margins_CU };
    const s16 C_Down_BTN_Pos_ori[] = { C_DOWN_BUTTON_X + X_Margins_CD, C_DOWN_BUTTON_Y + Y_Margins_CD };
    s16 C_Left_BTN_Pos[2]; //(X,Y)
    s16 C_Right_BTN_Pos[2];
    s16 C_Up_BTN_Pos[2];
    s16 C_Down_BTN_Pos[2];
    // C button Left
    s16 C_Left_BTN_Size = 32;
    float CLeftScale = (0.87f);
    int CLeftScaled = C_Left_BTN_Size * 0.87f;
    
    int CLeft_factor = (1 << 10) * C_Left_BTN_Size / CLeftScaled;
    
        C_Left_BTN_Pos[1] = C_Left_BTN_Pos_ori[1];
        C_Left_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Left_BTN_Pos_ori[0]);
    
    // C button Right
    s16 C_Right_BTN_Size = 32;
    float CRightScale = (0.87f);
    int CRightScaled = C_Right_BTN_Size * 0.87f;
    
    int CRight_factor = (1 << 10) * C_Right_BTN_Size / CRightScaled;
    
        C_Right_BTN_Pos[1] = C_Right_BTN_Pos_ori[1];
        C_Right_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Right_BTN_Pos_ori[0]);
    
    // C Button Up
    s16 C_Up_BTN_Size = 32;
    int CUpScaled = C_Up_BTN_Size * 0.5f;
    float CUpScale = (0.5f);
    
    int CUp_factor = (1 << 10) * C_Up_BTN_Size / CUpScaled;
    
        C_Up_BTN_Pos[1] = C_Up_BTN_Pos_ori[1];
        C_Up_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Up_BTN_Pos_ori[0]);
    
    // C Button down
    s16 C_Down_BTN_Size = 32;
    float CDownScale = (0.87f);
    
        CDownScale = 0.87f;
    
    int CDownScaled = C_Down_BTN_Size * CDownScale;
    int CDown_factor = (1 << 10) * C_Down_BTN_Size / CDownScaled;
    int PositionAdjustment = CDownScaled / 2;
    
        C_Down_BTN_Pos[1] = C_Down_BTN_Pos_ori[1];
        C_Down_BTN_Pos[0] = OTRGetRectDimensionFromRightEdge(C_Down_BTN_Pos_ori[0]);
    

    OPEN_DISPS(play->state.gfxCtx);

    // B Button Color & Texture
    // Also loads the Item Button Texture reused by other buttons afterwards
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, bButtonColor.r, bButtonColor.g, bButtonColor.b, interfaceCtx->bAlpha);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

    OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gButtonBackgroundTex, BBtn_Size, BBtn_Size, PosX_BtnB, PosY_BtnB,
                                  BBtnScaled, BBtnScaled, BBtn_factor, BBtn_factor);

    // C-Left Button Color & Texture
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cLeftButtonColor.r, cLeftButtonColor.g, cLeftButtonColor.b,
                    interfaceCtx->cLeftAlpha);
    gSPWideTextureRectangle(OVERLAY_DISP++, C_Left_BTN_Pos[0] << 2, C_Left_BTN_Pos[1] << 2,
                            (C_Left_BTN_Pos[0] + R_ITEM_BTN_WIDTH(1)) << 2,
                            (C_Left_BTN_Pos[1] + R_ITEM_BTN_WIDTH(1)) << 2, G_TX_RENDERTILE, 0, 0,
                            R_ITEM_BTN_DD(1) << 1, R_ITEM_BTN_DD(1) << 1);

    // C-Down Button Color & Texture
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cDownButtonColor.r, cDownButtonColor.g, cDownButtonColor.b,
                    interfaceCtx->cDownAlpha);
    gSPWideTextureRectangle(OVERLAY_DISP++, C_Down_BTN_Pos[0] << 2, C_Down_BTN_Pos[1] << 2,
                            (C_Down_BTN_Pos[0] + R_ITEM_BTN_WIDTH(2)) << 2,
                            (C_Down_BTN_Pos[1] + R_ITEM_BTN_WIDTH(2)) << 2, G_TX_RENDERTILE, 0, 0,
                            R_ITEM_BTN_DD(2) << 1, R_ITEM_BTN_DD(2) << 1);

    // C-Right Button Color & Texture
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cRightButtonColor.r, cRightButtonColor.g, cRightButtonColor.b,
                    interfaceCtx->cRightAlpha);
    gSPWideTextureRectangle(OVERLAY_DISP++, C_Right_BTN_Pos[0] << 2, C_Right_BTN_Pos[1] << 2,
                            (C_Right_BTN_Pos[0] + R_ITEM_BTN_WIDTH(3)) << 2,
                            (C_Right_BTN_Pos[1] + R_ITEM_BTN_WIDTH(3)) << 2, G_TX_RENDERTILE, 0, 0,
                            R_ITEM_BTN_DD(3) << 1, R_ITEM_BTN_DD(3) << 1);

    if ((pauseCtx->state < 8) || (pauseCtx->state >= 18)) {
        if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0)) {
            // Start Button Texture, Color & Label
            gDPPipeSync(OVERLAY_DISP++);

            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, startButtonColor.r, startButtonColor.g, startButtonColor.b,
                            interfaceCtx->startAlpha);
            gSPWideTextureRectangle(OVERLAY_DISP++, PosX_StartBtn << 2, PosY_StartBtn << 2,
                                    (PosX_StartBtn + StartBTN_W_Scaled) << 2, (PosY_StartBtn + StartBTN_H_Scaled) << 2,
                                    G_TX_RENDERTILE, 0, 0, StartBTN_W_factor, StartBTN_H_factor);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->startAlpha);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            Interface_LoadActionLabel(interfaceCtx, 3, 2);
            gDPLoadTextureBlock_4b(OVERLAY_DISP++, interfaceCtx->doActionSegment[2], G_IM_FMT_IA, DO_ACTION_TEX_WIDTH(),
                                   DO_ACTION_TEX_HEIGHT(), 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

            gDPPipeSync(OVERLAY_DISP++);
            gSPSetGeometryMode(OVERLAY_DISP++, G_CULL_BACK);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->startAlpha);
            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
            Matrix_Translate(PosX_StartBtn - 160 + ((Start_BTN_Scale + Start_BTN_Scale / 3) * 11.5f),
                             (PosY_StartBtn - 120 + ((Start_BTN_Scale + Start_BTN_Scale / 3) * 11.5f)) * -1, 1.0f,
                             MTXMODE_NEW);
            Matrix_Scale(Start_BTN_Scale + (Start_BTN_Scale / 3), Start_BTN_Scale + (Start_BTN_Scale / 3),
                         Start_BTN_Scale + (Start_BTN_Scale / 3), MTXMODE_APPLY);
            gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
            gSPVertex(OVERLAY_DISP++, &interfaceCtx->actionVtx[4], 4, 0);
            Interface_DrawActionLabel(play->state.gfxCtx, interfaceCtx->doActionSegment[2]);
            gDPPipeSync(OVERLAY_DISP++);
        }
    }

    gDPPipeSync(OVERLAY_DISP++);

    // Empty C Button Arrows
    for (temp = 1; temp < 4; temp++) {
        if (gSaveContext.equips.buttonItems[temp] > 0xF0) {
            s16 X_Margins_CL;
            s16 X_Margins_CR;
            s16 X_Margins_CD;
            s16 Y_Margins_CL;
            s16 Y_Margins_CR;
            s16 Y_Margins_CD;
            
                X_Margins_CL = 0;
                Y_Margins_CL = 0;
            
            
                X_Margins_CR = 0;
                Y_Margins_CR = 0;
            
            
                X_Margins_CD = 0;
                Y_Margins_CD = 0;
            
            const s16 ItemIconWidthFactor[3][2] = {
                { CLeftScaled, CLeft_factor },
                { CDownScaled, CDown_factor },
                { CRightScaled, CRight_factor },
            };
            const s16 ItemIconPos_ori[3][2] = {
                { R_ITEM_ICON_X(1) + X_Margins_CL, R_ITEM_ICON_Y(1) + Y_Margins_CL },
                { R_ITEM_ICON_X(2) + X_Margins_CD, R_ITEM_ICON_Y(2) + Y_Margins_CD },
                { R_ITEM_ICON_X(3) + X_Margins_CR, R_ITEM_ICON_Y(3) + Y_Margins_CR },
            };
            s16 ItemIconPos[3][2]; //(X,Y)
            // C button Left
            
                ItemIconPos[0][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[0][0]);
                ItemIconPos[0][1] = ItemIconPos_ori[0][1];
            
            // C Button down
            
                ItemIconPos[1][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[1][0]);
                ItemIconPos[1][1] = ItemIconPos_ori[1][1];
            
            // C button Right
            
                ItemIconPos[2][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[2][0]);
                ItemIconPos[2][1] = ItemIconPos_ori[2][1];
            

            if (temp == 1) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cLeftButtonColor.r, cLeftButtonColor.g, cLeftButtonColor.b,
                                interfaceCtx->cLeftAlpha);
            } else if (temp == 2) {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cDownButtonColor.r, cDownButtonColor.g, cDownButtonColor.b,
                                interfaceCtx->cDownAlpha);
            } else {
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, cRightButtonColor.r, cRightButtonColor.g, cRightButtonColor.b,
                                interfaceCtx->cRightAlpha);
            }

            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, ((u8*)gButtonBackgroundTex), 32, 32, ItemIconPos[temp - 1][0],
                                          ItemIconPos[temp - 1][1], ItemIconWidthFactor[temp - 1][0],
                                          ItemIconWidthFactor[temp - 1][0], ItemIconWidthFactor[temp - 1][1],
                                          ItemIconWidthFactor[temp - 1][1]);

            const char* cButtonIcons[] = { gButtonBackgroundTex, gEquippedItemOutlineTex, gEmptyCLeftArrowTex,
                                           gEmptyCDownArrowTex, gEmptyCRightArrowTex };

            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, cButtonIcons[(temp + 1)], 32, 32, ItemIconPos[temp - 1][0],
                                          ItemIconPos[temp - 1][1], ItemIconWidthFactor[temp - 1][0],
                                          ItemIconWidthFactor[temp - 1][0], ItemIconWidthFactor[temp - 1][1],
                                          ItemIconWidthFactor[temp - 1][1]);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

int16_t gItemIconWidth[] = { 30, 24, 24, 24 };
int16_t gItemIconDD[] = { 550, 680, 680, 680 };

void Interface_DrawItemIconTexture(PlayState* play, void* texture, s16 button) {
    OPEN_DISPS(play->state.gfxCtx);
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s16 X_Margins_CL;
    s16 X_Margins_CR;
    s16 X_Margins_CD;
    s16 Y_Margins_CL;
    s16 Y_Margins_CR;
    s16 Y_Margins_CD;
    s16 X_Margins_BtnB;
    s16 Y_Margins_BtnB;
    
        X_Margins_BtnB = 0;
        Y_Margins_BtnB = 0;
    
    
        X_Margins_CL = 0;
        Y_Margins_CL = 0;
    
    
        X_Margins_CR = 0;
        Y_Margins_CR = 0;
    
    
        X_Margins_CD = 0;
        Y_Margins_CD = 0;
    
    
    const s16 ItemIconPos_ori[4][2] = { { B_BUTTON_X + X_Margins_BtnB, B_BUTTON_Y + Y_Margins_BtnB },
                                        { C_LEFT_BUTTON_X + X_Margins_CL, C_LEFT_BUTTON_Y + Y_Margins_CL },
                                        { C_DOWN_BUTTON_X + X_Margins_CD, C_DOWN_BUTTON_Y + Y_Margins_CD },
                                        { C_RIGHT_BUTTON_X + X_Margins_CR, C_RIGHT_BUTTON_Y + Y_Margins_CR } };
    s16 ItemIconPos[4][2];
    // B Button
    
        ItemIconPos[0][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[0][0]);
        ItemIconPos[0][1] = ItemIconPos_ori[0][1];
    
    // C button Left
    
        ItemIconPos[1][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[1][0]);
        ItemIconPos[1][1] = ItemIconPos_ori[1][1];
    
    // C Button down
    
        ItemIconPos[2][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[2][0]);
        ItemIconPos[2][1] = ItemIconPos_ori[2][1];
    
    // C button Right
    
        ItemIconPos[3][0] = OTRGetRectDimensionFromRightEdge(ItemIconPos_ori[3][0]);
        ItemIconPos[3][1] = ItemIconPos_ori[3][1];
    

    gDPLoadTextureBlock(OVERLAY_DISP++, texture, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

    gSPWideTextureRectangle(OVERLAY_DISP++, ItemIconPos[button][0] << 2, ItemIconPos[button][1] << 2,
                            (ItemIconPos[button][0] + gItemIconWidth[button]) << 2,
                            (ItemIconPos[button][1] + gItemIconWidth[button]) << 2, G_TX_RENDERTILE, 0, 0,
                            gItemIconDD[button] << 1, gItemIconDD[button] << 1);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Interface_DrawActionButton(PlayState* play, f32 x, f32 y) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Translate(-137.0f + x, 97.0f - y, XREG(18) / 10.0f, MTXMODE_NEW);
    Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
    Matrix_RotateX(interfaceCtx->unk_1F4 / 10000.0f, MTXMODE_APPLY);

    gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPVertex(OVERLAY_DISP++, &interfaceCtx->actionVtx[0], 4, 0);

    gDPLoadTextureBlock(OVERLAY_DISP++, gButtonBackgroundTex, G_IM_FMT_IA, G_IM_SIZ_8b, 32, 32, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);

    gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Interface_InitVertices(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 i;

    interfaceCtx->actionVtx = Graph_Alloc(play->state.gfxCtx, 8 * sizeof(Vtx));

    // clang-format off
    interfaceCtx->actionVtx[0].v.ob[0] =
    interfaceCtx->actionVtx[2].v.ob[0] = -15;
    interfaceCtx->actionVtx[1].v.ob[0] =
    interfaceCtx->actionVtx[3].v.ob[0] = interfaceCtx->actionVtx[0].v.ob[0] + 29;

    interfaceCtx->actionVtx[0].v.ob[1] =
    interfaceCtx->actionVtx[1].v.ob[1] = 15;
    interfaceCtx->actionVtx[2].v.ob[1] =
    interfaceCtx->actionVtx[3].v.ob[1] = interfaceCtx->actionVtx[0].v.ob[1] - 29;

    interfaceCtx->actionVtx[4].v.ob[0] =
    interfaceCtx->actionVtx[6].v.ob[0] = -((XREG(21) + 1) / 2);
    interfaceCtx->actionVtx[5].v.ob[0] =
    interfaceCtx->actionVtx[7].v.ob[0] = interfaceCtx->actionVtx[4].v.ob[0] + (XREG(21) + 1);

    interfaceCtx->actionVtx[4].v.ob[1] =
    interfaceCtx->actionVtx[5].v.ob[1] = XREG(28) / 2;
    interfaceCtx->actionVtx[6].v.ob[1] =
    interfaceCtx->actionVtx[7].v.ob[1] = interfaceCtx->actionVtx[4].v.ob[1] - XREG(28);

    for (i = 0; i < 8; i += 4) {
        interfaceCtx->actionVtx[i].v.ob[2] = interfaceCtx->actionVtx[i+1].v.ob[2] =
        interfaceCtx->actionVtx[i+2].v.ob[2] = interfaceCtx->actionVtx[i+3].v.ob[2] = 0;

        interfaceCtx->actionVtx[i].v.flag = interfaceCtx->actionVtx[i+1].v.flag =
        interfaceCtx->actionVtx[i+2].v.flag = interfaceCtx->actionVtx[i+3].v.flag = 0;

        interfaceCtx->actionVtx[i].v.tc[0] = interfaceCtx->actionVtx[i].v.tc[1] =
        interfaceCtx->actionVtx[i+1].v.tc[1] = interfaceCtx->actionVtx[i+2].v.tc[0] = -16;
        interfaceCtx->actionVtx[i+1].v.tc[0] = interfaceCtx->actionVtx[i+2].v.tc[1] =
        interfaceCtx->actionVtx[i+3].v.tc[0] = interfaceCtx->actionVtx[i+3].v.tc[1] = 1024 - 16;

        interfaceCtx->actionVtx[i].v.cn[0] = interfaceCtx->actionVtx[i+1].v.cn[0] =
        interfaceCtx->actionVtx[i+2].v.cn[0] = interfaceCtx->actionVtx[i+3].v.cn[0] =
        interfaceCtx->actionVtx[i].v.cn[1] = interfaceCtx->actionVtx[i+1].v.cn[1] =
        interfaceCtx->actionVtx[i+2].v.cn[1] = interfaceCtx->actionVtx[i+3].v.cn[1] =
        interfaceCtx->actionVtx[i].v.cn[2] = interfaceCtx->actionVtx[i+1].v.cn[2] =
        interfaceCtx->actionVtx[i+2].v.cn[2] = interfaceCtx->actionVtx[i+3].v.cn[2] = 255;

        interfaceCtx->actionVtx[i].v.cn[3] = interfaceCtx->actionVtx[i+1].v.cn[3] =
        interfaceCtx->actionVtx[i+2].v.cn[3] = interfaceCtx->actionVtx[i+3].v.cn[3] = 255;
    }

    interfaceCtx->actionVtx[5].v.tc[0] = interfaceCtx->actionVtx[7].v.tc[0] = 1536;
    interfaceCtx->actionVtx[6].v.tc[1] = interfaceCtx->actionVtx[7].v.tc[1] = 512;

    interfaceCtx->beatingHeartVtx = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));

    interfaceCtx->beatingHeartVtx[0].v.ob[0] = interfaceCtx->beatingHeartVtx[2].v.ob[0] = -8;
    interfaceCtx->beatingHeartVtx[1].v.ob[0] = interfaceCtx->beatingHeartVtx[3].v.ob[0] = 8;
    interfaceCtx->beatingHeartVtx[0].v.ob[1] = interfaceCtx->beatingHeartVtx[1].v.ob[1] = 8;
    interfaceCtx->beatingHeartVtx[2].v.ob[1] = interfaceCtx->beatingHeartVtx[3].v.ob[1] = -8;

    interfaceCtx->beatingHeartVtx[0].v.ob[2] = interfaceCtx->beatingHeartVtx[1].v.ob[2] =
    interfaceCtx->beatingHeartVtx[2].v.ob[2] = interfaceCtx->beatingHeartVtx[3].v.ob[2] = 0;

    interfaceCtx->beatingHeartVtx[0].v.flag = interfaceCtx->beatingHeartVtx[1].v.flag =
    interfaceCtx->beatingHeartVtx[2].v.flag = interfaceCtx->beatingHeartVtx[3].v.flag = 0;

    interfaceCtx->beatingHeartVtx[0].v.tc[0] = interfaceCtx->beatingHeartVtx[0].v.tc[1] =
    interfaceCtx->beatingHeartVtx[1].v.tc[1] = interfaceCtx->beatingHeartVtx[2].v.tc[0] = 0;
    interfaceCtx->beatingHeartVtx[1].v.tc[0] = interfaceCtx->beatingHeartVtx[2].v.tc[1] =
    interfaceCtx->beatingHeartVtx[3].v.tc[0] = interfaceCtx->beatingHeartVtx[3].v.tc[1] = 512;

    interfaceCtx->beatingHeartVtx[0].v.cn[0] = interfaceCtx->beatingHeartVtx[1].v.cn[0] =
    interfaceCtx->beatingHeartVtx[2].v.cn[0] = interfaceCtx->beatingHeartVtx[3].v.cn[0] =
    interfaceCtx->beatingHeartVtx[0].v.cn[1] = interfaceCtx->beatingHeartVtx[1].v.cn[1] =
    interfaceCtx->beatingHeartVtx[2].v.cn[1] = interfaceCtx->beatingHeartVtx[3].v.cn[1] =
    interfaceCtx->beatingHeartVtx[0].v.cn[2] = interfaceCtx->beatingHeartVtx[1].v.cn[2] =
    interfaceCtx->beatingHeartVtx[2].v.cn[2] = interfaceCtx->beatingHeartVtx[3].v.cn[2] =
    interfaceCtx->beatingHeartVtx[0].v.cn[3] = interfaceCtx->beatingHeartVtx[1].v.cn[3] =
    interfaceCtx->beatingHeartVtx[2].v.cn[3] = interfaceCtx->beatingHeartVtx[3].v.cn[3] = 255;
    // clang-format on
}

/*
void func_8008A8B8(PlayState* play, s32 topY, s32 bottomY, s32 leftX, s32 rightX) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Vec3f eye;
    Vec3f lookAt;
    Vec3f up;

    eye.x = eye.y = eye.z = 0.0f;
    lookAt.x = lookAt.y = 0.0f;
    lookAt.z = -1.0f;
    up.x = up.z = 0.0f;
    up.y = 1.0f;

    func_800AA358(&interfaceCtx->view, &eye, &lookAt, &up);

    interfaceCtx->viewport.topY = topY;
    interfaceCtx->viewport.bottomY = bottomY;
    interfaceCtx->viewport.leftX = leftX;
    interfaceCtx->viewport.rightX = rightX;
    View_SetViewport(&interfaceCtx->view, &interfaceCtx->viewport);

    func_800AA460(&interfaceCtx->view, 60.0f, 10.0f, 60.0f);
    func_800AB560(&interfaceCtx->view);
}
*/

void func_8008A994(InterfaceContext* interfaceCtx) {
    SET_FULLSCREEN_VIEWPORT(&interfaceCtx->view);
    func_800AB2C4(&interfaceCtx->view);
}

const char* digitTextures[] = { gCounterDigit0Tex, gCounterDigit1Tex, gCounterDigit2Tex, gCounterDigit3Tex,
                                gCounterDigit4Tex, gCounterDigit5Tex, gCounterDigit6Tex, gCounterDigit7Tex,
                                gCounterDigit8Tex, gCounterDigit9Tex, gCounterColonTex,  gCounterDigit1Tex,
                                gCounterDigit2Tex, gCounterDigit3Tex, gCounterDigit4Tex, gCounterDigit5Tex,
                                gCounterDigit6Tex, gCounterDigit7Tex, gCounterDigit8Tex };

void Interface_Draw(PlayState* play) {
    static s16 magicArrowEffectsR[] = { 255, 100, 255 };
    static s16 magicArrowEffectsG[] = { 0, 100, 255 };
    static s16 magicArrowEffectsB[] = { 0, 255, 100 };
    static s16 timerDigitLeftPos[] = { 16, 25, 34, 42, 51 };
    static s16 digitWidth[] = { 9, 9, 8, 9, 9 };
    // unused, most likely colors
    static s16 D_80125B1C[][3] = {
        { 0, 150, 0 }, { 100, 255, 0 }, { 255, 255, 255 }, { 0, 0, 0 }, { 255, 255, 255 },
    };
    Color_RGB8 rColor;

    Color_RGB8 keyCountColor = { 200, 230, 255 };
    

    Color_RGB8 aButtonColor = { 90, 90, 255 };
    

    static s16 spoilingItemEntrances[] = { ENTR_LOST_WOODS_2, ENTR_ZORAS_DOMAIN_3, ENTR_ZORAS_DOMAIN_3 };
    static f32 D_80125B54[] = { -40.0f, -35.0f }; // unused
    static s16 D_80125B5C[] = { 91, 91 };         // unused
    static s16 sTimerNextSecondTimer;
    static s16 sTimerStateTimer;
    static s16 sSubTimerNextSecondTimer;
    static s16 sSubTimerStateTimer;
    static s16 timerDigits[5];
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    MessageContext* msgCtx = &play->msgCtx;
    Player* player = GET_PLAYER(play);
    s16 svar1;
    s16 svar2;
    s16 svar3;
    s16 svar4;
    s16 svar5;
    s16 timerId;
    // #region SOH [NTSC]
    s32 languageOffset = gSaveContext.language;

    if (languageOffset == LANGUAGE_JPN) {
        languageOffset = LANGUAGE_ENG;
    }
    // #endregion

    OPEN_DISPS(play->state.gfxCtx);

    gSPSegment(OVERLAY_DISP++, 0x02, interfaceCtx->parameterSegment);
    gSPSegment(OVERLAY_DISP++, 0x07, interfaceCtx->doActionSegment);
    gSPSegment(OVERLAY_DISP++, 0x08, interfaceCtx->iconItemSegment);
    gSPSegment(OVERLAY_DISP++, 0x0B, interfaceCtx->mapSegment);

    if (pauseCtx->debugState == 0) {
        Interface_InitVertices(play);
        func_8008A994(interfaceCtx);
        HealthMeter_Draw(play);

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        s16 PosX_RC;
        s16 PosY_RC;
            
                // Rupee Icon
                
                    rColor = (Color_RGB8){ 200, 220, 255 };
                

                // Rupee icon & counter
                s16 X_Margins_RC;
                s16 Y_Margins_RC;
                
                    X_Margins_RC = 0;
                    Y_Margins_RC = 0;
                
                s16 PosX_RC_ori = OTRGetRectDimensionFromLeftEdge(26 + X_Margins_RC);
                s16 PosY_RC_ori = 206 + Y_Margins_RC;
                
                    PosY_RC = PosY_RC_ori;
                    PosX_RC = PosX_RC_ori;
                
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, rColor.r, rColor.g, rColor.b, interfaceCtx->magicAlpha);
                OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gRupeeCounterIconTex, 16, 16, PosX_RC, PosY_RC, 16, 16,
                                              1 << 10, 1 << 10);
            

            
                switch (play->sceneNum) {
                    case SCENE_FOREST_TEMPLE:
                    case SCENE_FIRE_TEMPLE:
                    case SCENE_WATER_TEMPLE:
                    case SCENE_SPIRIT_TEMPLE:
                    case SCENE_SHADOW_TEMPLE:
                    case SCENE_BOTTOM_OF_THE_WELL:
                    case SCENE_ICE_CAVERN:
                    case SCENE_GANONS_TOWER:
                    case SCENE_GERUDO_TRAINING_GROUND:
                    case SCENE_THIEVES_HIDEOUT:
                    case SCENE_INSIDE_GANONS_CASTLE:
                    case SCENE_GANONS_TOWER_COLLAPSE_INTERIOR:
                    case SCENE_INSIDE_GANONS_CASTLE_COLLAPSE:
                    case SCENE_TREASURE_BOX_SHOP:
                        if (gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] >= 0) {
                            s16 X_Margins_SKC;
                            s16 Y_Margins_SKC;
                            
                                X_Margins_SKC = 0;
                                Y_Margins_SKC = 0;
                            
                            s16 PosX_SKC_ori = OTRGetRectDimensionFromLeftEdge(26 + X_Margins_SKC);
                            s16 PosY_SKC_ori = 190 + Y_Margins_SKC;
                            s16 PosX_SKC;
                            s16 PosY_SKC;
                            
                                PosY_SKC = PosY_SKC_ori;
                                PosX_SKC = PosX_SKC_ori;
                            
                            // Small Key Icon
                            gDPPipeSync(OVERLAY_DISP++);

                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, keyCountColor.r, keyCountColor.g, keyCountColor.b,
                                            interfaceCtx->magicAlpha);
                            gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 20,
                                           255); // We reset this here so it match user color :)
                            OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gSmallKeyCounterIconTex, 16, 16, PosX_SKC,
                                                          PosY_SKC, 16, 16, 1 << 10, 1 << 10);

                            // Small Key Counter
                            gDPPipeSync(OVERLAY_DISP++);
                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->magicAlpha);
                            gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0,
                                              PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);

                            interfaceCtx->counterDigits[2] = 0;
                            interfaceCtx->counterDigits[3] = gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex];

                            while (interfaceCtx->counterDigits[3] >= 10) {
                                interfaceCtx->counterDigits[2]++;
                                interfaceCtx->counterDigits[3] -= 10;
                            }

                            svar3 = 16;
                            if (interfaceCtx->counterDigits[2] != 0) {
                                OVERLAY_DISP = Gfx_TextureI8(
                                    OVERLAY_DISP, ((u8*)((u8*)digitTextures[interfaceCtx->counterDigits[2]])), 8, 16,
                                    PosX_SKC + 16, PosY_SKC, 8, 16, 1 << 10, 1 << 10);
                                svar3 = 24;
                            }

                            OVERLAY_DISP =
                                Gfx_TextureI8(OVERLAY_DISP, ((u8*)digitTextures[interfaceCtx->counterDigits[3]]), 8, 16,
                                              PosX_SKC + svar3, PosY_SKC, 8, 16, 1 << 10, 1 << 10);
                        }
                        break;
                    default:
                        break;
                }
            

            
                // Rupee Counter
                gDPPipeSync(OVERLAY_DISP++);

                if (gSaveContext.rupees == RUPEE_BALANCE_MAX) {
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 120, 255, 0, interfaceCtx->magicAlpha);
                } else if (gSaveContext.rupees != 0) {
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->magicAlpha);
                } else {
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 100, 100, 100, interfaceCtx->magicAlpha);
                }

                gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE,
                                  TEXEL0, 0, PRIMITIVE, 0);

                {
                    s64 silver = gSaveContext.rupees / GOLD_PER_SILVER;
                    s16 gold = (s16)(gSaveContext.rupees % GOLD_PER_SILVER);
                    s16 silverDigitCount = 1;
                    s16 drawX = PosX_RC + 16;
                    s64 divisor = 1;
                    s64 remainingSilver = silver;

                    while (divisor <= silver / 10) {
                        divisor *= 10;
                        silverDigitCount++;
                    }

                    for (svar1 = 0; svar1 < silverDigitCount; svar1++) {
                        s16 digit = (s16)(remainingSilver / divisor);
                        OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, (u8*)digitTextures[digit], 8, 16, drawX, PosY_RC,
                                                     8, 16, 1 << 10, 1 << 10);
                        remainingSilver %= divisor;
                        divisor /= 10;
                        drawX += 8;
                    }

                    drawX += 2;
                    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 200, 40, interfaceCtx->magicAlpha);
                    OVERLAY_DISP = Gfx_TextureIA8(OVERLAY_DISP, gRupeeCounterIconTex, 16, 16, drawX, PosY_RC, 16, 16,
                                                  1 << 10, 1 << 10);
                    drawX += 16;

                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->magicAlpha);
                    gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0,
                                      PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
                    OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, (u8*)digitTextures[gold / 10], 8, 16, drawX, PosY_RC,
                                                 8, 16, 1 << 10, 1 << 10);
                    drawX += 8;
                    OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, (u8*)digitTextures[gold % 10], 8, 16, drawX, PosY_RC,
                                                 8, 16, 1 << 10, 1 << 10);
                }
            

        Interface_DrawMagicBar(play);

        Minimap_Draw(play);

        if ((R_PAUSE_MENU_MODE != 2) && (R_PAUSE_MENU_MODE != 3)) {
            func_8002C124(&play->actorCtx.targetCtx, play); // Draw Z-Target
        }

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        Interface_DrawItemButtons(play);

        gDPPipeSync(OVERLAY_DISP++);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);

        if (!(interfaceCtx->unk_1FA)) {
            // B Button Icon & Ammo Count
            if (gSaveContext.equips.buttonItems[0] != ITEM_NONE) {
                Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[0]], 0);

                if ((player->stateFlags1 & PLAYER_STATE1_ON_HORSE) || (play->shootingGalleryStatus > 1) ||
                    ((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38))) {

                    gDPPipeSync(OVERLAY_DISP++);
                    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE,
                                      0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
                }
            }
        } else {
            // B Button Do Action Label
            s16 PosX_adjust;
            s16 PosY_adjust;
            if (gSaveContext.language == LANGUAGE_FRA) {
                PosX_adjust = -12;
                PosY_adjust = 5;
            } else if (gSaveContext.language == LANGUAGE_GER) {
                PosY_adjust = 6;
                PosX_adjust = -9;
            } else {
                PosY_adjust = 6;
                PosX_adjust = -10;
            }

            s16 BbtnPosX;
            s16 BbtnPosY;
            s16 X_Margins_BtnB_label;
            s16 Y_Margins_BtnB_label;
            
                X_Margins_BtnB_label = 0;
                Y_Margins_BtnB_label = 0;
            
            
                BbtnPosX = OTRGetRectDimensionFromRightEdge(R_B_LABEL_X(languageOffset) + X_Margins_BtnB_label);
                BbtnPosY = R_B_LABEL_Y(languageOffset) + Y_Margins_BtnB_label;
            
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);

            gDPLoadTextureBlock_4b(OVERLAY_DISP++, interfaceCtx->doActionSegment[1], G_IM_FMT_IA, DO_ACTION_TEX_WIDTH(),
                                   DO_ACTION_TEX_HEIGHT(), 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

            R_B_LABEL_DD = (1 << 10) / (WREG(37 + languageOffset) / 100.0f);
            gSPWideTextureRectangle(OVERLAY_DISP++, BbtnPosX << 2, BbtnPosY << 2,
                                    (BbtnPosX + DO_ACTION_TEX_WIDTH()) << 2, (BbtnPosY + DO_ACTION_TEX_HEIGHT()) << 2,
                                    G_TX_RENDERTILE, 0, 0, R_B_LABEL_DD, R_B_LABEL_DD);
        }

        gDPPipeSync(OVERLAY_DISP++);

        // C-Left Button Icon & Ammo Count
        if (gSaveContext.equips.buttonItems[1] < 0xF0) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->cLeftAlpha);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[1]], 1);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        }

        gDPPipeSync(OVERLAY_DISP++);

        // C-Down Button Icon & Ammo Count
        if (gSaveContext.equips.buttonItems[2] < 0xF0) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->cDownAlpha);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[2]], 2);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        }

        gDPPipeSync(OVERLAY_DISP++);

        // C-Right Button Icon & Ammo Count
        if (gSaveContext.equips.buttonItems[3] < 0xF0) {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->cRightAlpha);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            Interface_DrawItemIconTexture(play, gItemIcons[gSaveContext.equips.buttonItems[3]], 3);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                              PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        }

        // A Button
        Gfx_SetupDL_42Overlay(play->state.gfxCtx);
        s16 X_Margins_BtnA;
        s16 Y_Margins_BtnA;
        
            X_Margins_BtnA = 0;
            Y_Margins_BtnA = 0;
        
        s16 PosX_BtnA_ori = OTRGetDimensionFromRightEdge(R_A_BTN_X + X_Margins_BtnA);
        s16 PosY_BtnA_ori = R_A_BTN_Y + Y_Margins_BtnA;
        const f32 rAIconX_ori = OTRGetDimensionFromRightEdge(R_A_ICON_X + X_Margins_BtnA);
        const f32 rAIconY_ori = 98.0f - (R_A_ICON_Y + Y_Margins_BtnA);
        s16 PosX_BtnA;
        s16 PosY_BtnA;
        s16 rAIconX;
        s16 rAIconY;
        
            PosY_BtnA = PosY_BtnA_ori;
            PosX_BtnA = PosX_BtnA_ori;
            rAIconY = rAIconY_ori;
            rAIconX = rAIconX_ori;
        
        gSPClearGeometryMode(OVERLAY_DISP++, G_CULL_BOTH);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, aButtonColor.r, aButtonColor.g, aButtonColor.b, interfaceCtx->aAlpha);
        Interface_DrawActionButton(play, PosX_BtnA, PosY_BtnA);
        gDPPipeSync(OVERLAY_DISP++);
        gSPSetGeometryMode(OVERLAY_DISP++, G_CULL_BACK);
        gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0,
                          PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->aAlpha);
        gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
        Matrix_Translate(-138.0f + rAIconX, rAIconY, WREG(46 + languageOffset) / 10.0f, MTXMODE_NEW);
        Matrix_Scale(1.0f, 1.0f, 1.0f, MTXMODE_APPLY);
        Matrix_RotateX(interfaceCtx->unk_1F4 / 10000.0f, MTXMODE_APPLY);
        gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPVertex(OVERLAY_DISP++, &interfaceCtx->actionVtx[4], 4, 0);

        if ((interfaceCtx->unk_1EC < 2) || (interfaceCtx->unk_1EC == 3)) {
            Interface_DrawActionLabel(play->state.gfxCtx, interfaceCtx->doActionSegment[0]);
        } else {
            Interface_DrawActionLabel(play->state.gfxCtx, interfaceCtx->doActionSegment[1]);
        }

        gDPPipeSync(OVERLAY_DISP++);

        func_8008A994(interfaceCtx);
        svar3 = 16;

        if ((pauseCtx->state == 6) && (pauseCtx->unk_1E4 == 3)) {
            // Inventory Equip Effects
            gSPSegment(OVERLAY_DISP++, 0x08, pauseCtx->iconItemSegment);
            Gfx_SetupDL_42Overlay(play->state.gfxCtx);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            gSPMatrix(OVERLAY_DISP++, &gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD);

            pauseCtx->cursorVtx[svar3].v.ob[0] = pauseCtx->cursorVtx[18].v.ob[0] = svar2 = pauseCtx->equipAnimX / 10;
            pauseCtx->cursorVtx[17].v.ob[0] = pauseCtx->cursorVtx[19].v.ob[0] = svar2 =
                pauseCtx->cursorVtx[svar3].v.ob[0] + WREG(90) / 10;
            pauseCtx->cursorVtx[svar3].v.ob[1] = pauseCtx->cursorVtx[17].v.ob[1] = svar2 = pauseCtx->equipAnimY / 10;
            pauseCtx->cursorVtx[18].v.ob[1] = pauseCtx->cursorVtx[19].v.ob[1] = svar2 =
                pauseCtx->cursorVtx[svar3].v.ob[1] - WREG(90) / 10;

            if (pauseCtx->equipTargetItem < 0xBF) {
                // Normal Equip (icon goes from the inventory slot to the C button when equipping it)
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, pauseCtx->equipAnimAlpha);
                gSPVertex(OVERLAY_DISP++, &pauseCtx->cursorVtx[16], 4, 0);

                gDPLoadTextureBlock(OVERLAY_DISP++, gItemIcons[pauseCtx->equipTargetItem], G_IM_FMT_RGBA, G_IM_SIZ_32b,
                                    32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                                    G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            } else {
                // Magic Arrow Equip Effect
                svar1 = pauseCtx->equipTargetItem - 0xBF;
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, magicArrowEffectsR[svar1], magicArrowEffectsG[svar1],
                                magicArrowEffectsB[svar1], pauseCtx->equipAnimAlpha);

                if ((pauseCtx->equipAnimAlpha > 0) && (pauseCtx->equipAnimAlpha < 255)) {
                    svar1 = (pauseCtx->equipAnimAlpha / 8) / 2;
                    pauseCtx->cursorVtx[16].v.ob[0] = pauseCtx->cursorVtx[18].v.ob[0] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[0] - svar1;
                    pauseCtx->cursorVtx[17].v.ob[0] = pauseCtx->cursorVtx[19].v.ob[0] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[0] + svar1 * 2 + 32;
                    pauseCtx->cursorVtx[16].v.ob[1] = pauseCtx->cursorVtx[17].v.ob[1] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[1] + svar1;
                    pauseCtx->cursorVtx[18].v.ob[1] = pauseCtx->cursorVtx[19].v.ob[1] = svar2 =
                        pauseCtx->cursorVtx[16].v.ob[1] - svar1 * 2 - 32;
                }

                gSPVertex(OVERLAY_DISP++, &pauseCtx->cursorVtx[16], 4, 0);
                gDPLoadTextureBlock(OVERLAY_DISP++, gMagicArrowEquipEffectTex, G_IM_FMT_IA, G_IM_SIZ_8b, 32, 32, 0,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
            }

            gSP1Quadrangle(OVERLAY_DISP++, 0, 2, 3, 1, 0);
        }

        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0)) {
            if (gSaveContext.minigameState != 1) {
                // Carrots rendering if the action corresponds to riding a horse
                if (interfaceCtx->unk_1EE == 8) {
                    // Load Carrot Icon
                    gDPLoadTextureBlock(OVERLAY_DISP++, gCarrotIconTex, G_IM_FMT_RGBA, G_IM_SIZ_32b, 16, 16, 0,
                                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                        G_TX_NOLOD, G_TX_NOLOD);

                    // Draw 6 carrots
                    s16 CarrotsPosX = ZREG(14);
                    s16 CarrotsPosY = ZREG(15);
                    s16 CarrotsMargins_X = 0;
                    
                    for (svar1 = 1, svar5 = CarrotsPosX; svar1 < 7; svar1++, svar5 += 16) {
                        // Carrot Color (based on availability)
                        if ((interfaceCtx->numHorseBoosts == 0) || (interfaceCtx->numHorseBoosts < svar1)) {
                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 150, 255, interfaceCtx->aAlpha);
                        } else {
                            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->aAlpha);
                        }

                        gSPWideTextureRectangle(OVERLAY_DISP++, svar5 << 2, CarrotsPosY << 2, (svar5 + 16) << 2,
                                                (CarrotsPosY + 16) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
                    }
                }
            } else {
                // Score for the Horseback Archery
                s32 X_Margins_Archery;
                
                    X_Margins_Archery = 0;
                
                s16 ArcheryPos_Y = ZREG(15);
                s16 ArcheryPos_X = OTRGetRectDimensionFromRightEdge(WREG(32) + X_Margins_Archery);

                

                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, interfaceCtx->bAlpha);

                // Target Icon
                gDPLoadTextureBlock(OVERLAY_DISP++, gArcheryScoreIconTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 24, 16, 0,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);

                gSPWideTextureRectangle(OVERLAY_DISP++, (ArcheryPos_X + 28) << 2, ArcheryPos_Y << 2,
                                        (ArcheryPos_X + 52) << 2, (ArcheryPos_Y + 16) << 2, G_TX_RENDERTILE, 0, 0,
                                        1 << 10, 1 << 10);

                // Score Counter
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE,
                                  TEXEL0, 0, PRIMITIVE, 0);

                ArcheryPos_X = ArcheryPos_X + 6 * 9;

                for (svar1 = svar2 = 0; svar1 < 4; svar1++) {
                    if (sHBAScoreDigits[svar1] != 0 || (svar2 != 0) || (svar1 >= 3)) {
                        OVERLAY_DISP =
                            Gfx_TextureI8(OVERLAY_DISP, digitTextures[sHBAScoreDigits[svar1]], 8, 16, ArcheryPos_X,
                                          (ArcheryPos_Y - 2), digitWidth[0], VREG(42), VREG(43) << 1, VREG(43) << 1);
                        ArcheryPos_X += 9;
                        svar2++;
                    }
                }

                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
            }
        }

        if ((gSaveContext.subTimerState == SUBTIMER_STATE_RESPAWN) &&
            (Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT)) {
            // Trade quest timer reached 0
            sSubTimerStateTimer = 40;
            gSaveContext.cutsceneIndex = 0;
            play->transitionTrigger = TRANS_TRIGGER_START;
            play->transitionType = TRANS_TYPE_FADE_WHITE;
            gSaveContext.subTimerState = SUBTIMER_STATE_OFF;

            if ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI) &&
                (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER) &&
                (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_BGS) &&
                (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KNIFE)) {
                if (gSaveContext.buttonStatus[0] != BTN_ENABLED) {
                    gSaveContext.equips.buttonItems[0] = gSaveContext.buttonStatus[0];
                } else {
                    gSaveContext.equips.buttonItems[0] = ITEM_NONE;
                }
            }

            // Revert any spoiling trade quest items
            
                for (svar1 = 0; svar1 < ARRAY_COUNT(gSpoilingItems); svar1++) {
                    if (INV_CONTENT(ITEM_TRADE_ADULT) == gSpoilingItems[svar1]) {
                        gSaveContext.eventInf[0] &= 0x7F80;
                        osSyncPrintf("EVENT_INF=%x\n", gSaveContext.eventInf[0]);
                        play->nextEntranceIndex = spoilingItemEntrances[svar1];
                        INV_CONTENT(gSpoilingItemReverts[svar1]) = gSpoilingItemReverts[svar1];

                        for (svar2 = 1; svar2 < ARRAY_COUNT(gSaveContext.equips.buttonItems); svar2++) {
                            if (gSaveContext.equips.buttonItems[svar2] == gSpoilingItems[svar1]) {
                                gSaveContext.equips.buttonItems[svar2] = gSpoilingItemReverts[svar1];
                                Interface_LoadItemIcon1(play, svar2);
                            }
                        }
                    }
                }
            
        }

        if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) &&
            (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (msgCtx->msgMode == MSGMODE_NONE) &&
            !(player->stateFlags2 & PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR) &&
            (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF) &&
            !Play_InCsMode(play) && (gSaveContext.minigameState != 1) && (play->shootingGalleryStatus <= 1) &&
            !((play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY) && Flags_GetSwitch(play, 0x38))) {
            timerId = TIMER_ID_MAIN;
            switch (gSaveContext.timerState) {
                case TIMER_STATE_ENV_HAZARD_INIT:
                    sTimerStateTimer = 20;
                    sTimerNextSecondTimer = 20;
                    gSaveContext.timerSeconds = gSaveContext.health >> 1;
                    gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_PREVIEW;
                    break;
                case TIMER_STATE_ENV_HAZARD_PREVIEW:
                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_MOVE;
                    }
                    break;
                case TIMER_STATE_DOWN_INIT:
                case TIMER_STATE_UP_INIT:
                    sTimerStateTimer = 20;
                    sTimerNextSecondTimer = 20;
                    if (gSaveContext.timerState == TIMER_STATE_DOWN_INIT) {
                        gSaveContext.timerState = TIMER_STATE_DOWN_PREVIEW;
                    } else {
                        gSaveContext.timerState = TIMER_STATE_UP_PREVIEW;
                    }
                    break;
                case TIMER_STATE_DOWN_PREVIEW:
                case TIMER_STATE_UP_PREVIEW:
                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        if (gSaveContext.timerState == TIMER_STATE_DOWN_PREVIEW) {
                            gSaveContext.timerState = TIMER_STATE_DOWN_MOVE;
                        } else {
                            gSaveContext.timerState = TIMER_STATE_UP_MOVE;
                        }
                    }
                    break;
                case TIMER_STATE_ENV_HAZARD_MOVE:
                case TIMER_STATE_DOWN_MOVE:
                    svar1 = (gSaveContext.timerX[TIMER_ID_MAIN] - 26) / sTimerStateTimer;
                    gSaveContext.timerX[TIMER_ID_MAIN] -= svar1;

                    if (gSaveContext.healthCapacity > 0xA0) {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 54) / sTimerStateTimer;
                    } else {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 46) / sTimerStateTimer;
                    }
                    gSaveContext.timerY[TIMER_ID_MAIN] -= svar1;

                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        gSaveContext.timerX[TIMER_ID_MAIN] = 26;

                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }

                        if (gSaveContext.timerState == TIMER_STATE_ENV_HAZARD_MOVE) {
                            gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_TICK;
                        } else {
                            gSaveContext.timerState = TIMER_STATE_DOWN_TICK;
                        }
                    }
                case TIMER_STATE_ENV_HAZARD_TICK:
                case TIMER_STATE_DOWN_TICK:
                    if ((gSaveContext.timerState == TIMER_STATE_ENV_HAZARD_TICK) ||
                        (gSaveContext.timerState == TIMER_STATE_DOWN_TICK)) {
                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }
                    }

                    if ((gSaveContext.timerState >= TIMER_STATE_ENV_HAZARD_MOVE) && (msgCtx->msgLength == 0)) {
                        sTimerNextSecondTimer--;
                        if (sTimerNextSecondTimer == 0) {
                            if (gSaveContext.timerSeconds != 0) {
                                gSaveContext.timerSeconds--;
                            }

                            sTimerNextSecondTimer = 20;

                            if (gSaveContext.timerSeconds == 0) {
                                gSaveContext.timerState = TIMER_STATE_STOP;
                                if (sEnvHazardActive) {
                                    gSaveContext.health = 0;
                                    play->damagePlayer(play, -(gSaveContext.health + 2));
                                }
                                sEnvHazardActive = false;
                            } else if (gSaveContext.timerSeconds > 60) {
                                if (timerDigits[4] == 1) {
                                    Audio_PlaySoundGeneral(NA_SE_SY_MESSAGE_WOMAN, &gSfxDefaultPos, 4,
                                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                           &gSfxDefaultReverb);
                                }
                            } else if (gSaveContext.timerSeconds >= 11) {
                                if (timerDigits[4] & 1) {
                                    Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                           &gSfxDefaultReverb);
                                }
                            } else {
                                Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_E, &gSfxDefaultPos, 4,
                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                       &gSfxDefaultReverb);
                            }
                        }
                    }
                    break;
                case TIMER_STATE_UP_MOVE:
                    svar1 = (gSaveContext.timerX[TIMER_ID_MAIN] - 26) / sTimerStateTimer;
                    gSaveContext.timerX[TIMER_ID_MAIN] -= svar1;

                    if (gSaveContext.healthCapacity > 0xA0) {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 54) / sTimerStateTimer;
                    } else {
                        svar1 = (gSaveContext.timerY[TIMER_ID_MAIN] - 46) / sTimerStateTimer;
                    }
                    gSaveContext.timerY[TIMER_ID_MAIN] -= svar1;

                    sTimerStateTimer--;
                    if (sTimerStateTimer == 0) {
                        sTimerStateTimer = 20;
                        gSaveContext.timerX[TIMER_ID_MAIN] = 26;
                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }

                        gSaveContext.timerState = TIMER_STATE_UP_TICK;
                    }
                case TIMER_STATE_UP_TICK:
                    if (gSaveContext.timerState == TIMER_STATE_UP_TICK) {
                        if (gSaveContext.healthCapacity > 0xA0) {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 54;
                        } else {
                            gSaveContext.timerY[TIMER_ID_MAIN] = 46;
                        }
                    }

                    if (gSaveContext.timerState >= TIMER_STATE_ENV_HAZARD_MOVE) {
                        sTimerNextSecondTimer--;
                        if (sTimerNextSecondTimer == 0) {
                            gSaveContext.timerSeconds++;
                            sTimerNextSecondTimer = 20;

                            if (gSaveContext.timerSeconds == 3599) {
                                sTimerStateTimer = 40;
                                gSaveContext.timerState = TIMER_STATE_UP_FREEZE;
                            } else {
                                Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                       &gSfxDefaultReverb);
                            }
                        }
                    }
                    break;
                case TIMER_STATE_STOP:
                    if (gSaveContext.subTimerState != SUBTIMER_STATE_OFF) {
                        sSubTimerStateTimer = 20;
                        sSubTimerNextSecondTimer = 20;
                        gSaveContext.timerX[TIMER_ID_SUB] = 140;
                        gSaveContext.timerY[TIMER_ID_SUB] = 80;

                        if (gSaveContext.subTimerState <= SUBTIMER_STATE_STOP) {
                            gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_PREVIEW;
                        } else {
                            gSaveContext.subTimerState = SUBTIMER_STATE_UP_PREVIEW;
                        }

                        gSaveContext.timerState = TIMER_STATE_OFF;
                    } else {
                        gSaveContext.timerState = TIMER_STATE_OFF;
                    }
                case TIMER_STATE_UP_FREEZE:
                    break;
                default:
                    timerId = TIMER_ID_SUB;
                    switch (gSaveContext.subTimerState) {
                        case SUBTIMER_STATE_DOWN_INIT:
                        case SUBTIMER_STATE_UP_INIT:
                            sSubTimerStateTimer = 20;
                            sSubTimerNextSecondTimer = 20;
                            gSaveContext.timerX[TIMER_ID_SUB] = 140;
                            gSaveContext.timerY[TIMER_ID_SUB] = 80;
                            if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_INIT) {
                                gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_PREVIEW;
                            } else {
                                gSaveContext.subTimerState = SUBTIMER_STATE_UP_PREVIEW;
                            }
                            break;
                        case SUBTIMER_STATE_DOWN_PREVIEW:
                        case SUBTIMER_STATE_UP_PREVIEW:
                            sSubTimerStateTimer--;
                            if (sSubTimerStateTimer == 0) {
                                sSubTimerStateTimer = 20;
                                if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_PREVIEW) {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_MOVE;
                                } else {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_UP_MOVE;
                                }
                            }
                            break;
                        case SUBTIMER_STATE_DOWN_MOVE:
                        case SUBTIMER_STATE_UP_MOVE:
                            osSyncPrintf("event_xp[1]=%d,  event_yp[1]=%d  TOTAL_EVENT_TM=%d\n",
                                         svar5 = gSaveContext.timerX[TIMER_ID_SUB],
                                         svar2 = gSaveContext.timerY[TIMER_ID_SUB], gSaveContext.subTimerSeconds);
                            svar1 = (gSaveContext.timerX[TIMER_ID_SUB] - 26) / sSubTimerStateTimer;
                            gSaveContext.timerX[TIMER_ID_SUB] -= svar1;
                            if (gSaveContext.healthCapacity > 0xA0) {
                                svar1 = (gSaveContext.timerY[TIMER_ID_SUB] - 54) / sSubTimerStateTimer;
                            } else {
                                svar1 = (gSaveContext.timerY[TIMER_ID_SUB] - 46) / sSubTimerStateTimer;
                            }
                            gSaveContext.timerY[TIMER_ID_SUB] -= svar1;

                            sSubTimerStateTimer--;
                            if (sSubTimerStateTimer == 0) {
                                sSubTimerStateTimer = 20;
                                gSaveContext.timerX[TIMER_ID_SUB] = 26;

                                if (gSaveContext.healthCapacity > 0xA0) {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 54;
                                } else {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 46;
                                }

                                if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_MOVE) {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_DOWN_TICK;
                                } else {
                                    gSaveContext.subTimerState = SUBTIMER_STATE_UP_TICK;
                                }
                            }
                        case SUBTIMER_STATE_DOWN_TICK:
                        case SUBTIMER_STATE_UP_TICK:
                            if ((gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_TICK) ||
                                (gSaveContext.subTimerState == SUBTIMER_STATE_UP_TICK)) {
                                if (gSaveContext.healthCapacity > 0xA0) {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 54;
                                } else {
                                    gSaveContext.timerY[TIMER_ID_SUB] = 46;
                                }
                            }

                            if (gSaveContext.subTimerState >= SUBTIMER_STATE_DOWN_MOVE) {
                                sSubTimerNextSecondTimer--;
                                if (sSubTimerNextSecondTimer == 0) {
                                    sSubTimerNextSecondTimer = 20;
                                    if (gSaveContext.subTimerState == SUBTIMER_STATE_DOWN_TICK) {
                                        gSaveContext.subTimerSeconds--;
                                        osSyncPrintf("TOTAL_EVENT_TM=%d\n", gSaveContext.subTimerSeconds);

                                        if (gSaveContext.subTimerSeconds <= 0) {
                                            if (!Flags_GetSwitch(play, 0x37) ||
                                                ((play->sceneNum != SCENE_GANON_BOSS) &&
                                                 (play->sceneNum != SCENE_GANONS_TOWER_COLLAPSE_EXTERIOR) &&
                                                 (play->sceneNum != SCENE_GANONS_TOWER_COLLAPSE_INTERIOR) &&
                                                 (play->sceneNum != SCENE_INSIDE_GANONS_CASTLE_COLLAPSE))) {
                                                sSubTimerStateTimer = 40;
                                                gSaveContext.subTimerState = SUBTIMER_STATE_RESPAWN;
                                                gSaveContext.cutsceneIndex = 0;
                                                Message_StartTextbox(play, 0x71B0, NULL);
                                                Player_SetCsActionWithHaltedActors(play, NULL, 8);
                                            } else {
                                                sSubTimerStateTimer = 40;
                                                gSaveContext.subTimerState = SUBTIMER_STATE_STOP;
                                            }
                                        } else if (gSaveContext.subTimerSeconds > 60) {
                                            if (timerDigits[4] == 1) {
                                                Audio_PlaySoundGeneral(NA_SE_SY_MESSAGE_WOMAN, &gSfxDefaultPos, 4,
                                                                       &gSfxDefaultFreqAndVolScale,
                                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                                            }
                                        } else if (gSaveContext.subTimerSeconds > 10) {
                                            if ((timerDigits[4] & 1)) {
                                                Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                                       &gSfxDefaultFreqAndVolScale,
                                                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                                            }
                                        } else {
                                            Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_E, &gSfxDefaultPos, 4,
                                                                   &gSfxDefaultFreqAndVolScale,
                                                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
                                        }
                                    } else {
                                        gSaveContext.subTimerSeconds++;
                                        if (gSaveContext.eventInf[1] & 1) {
                                            if (gSaveContext.subTimerSeconds == 240) {
                                                Message_StartTextbox(play, 0x6083, NULL);
                                                gSaveContext.eventInf[1] &= ~1;
                                                gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
                                            }
                                        }
                                    }

                                    if ((gSaveContext.subTimerSeconds % 60) == 0) {
                                        Audio_PlaySoundGeneral(NA_SE_SY_WARNING_COUNT_N, &gSfxDefaultPos, 4,
                                                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                                               &gSfxDefaultReverb);
                                    }
                                }
                            }
                            break;
                        case 6:
                            sSubTimerStateTimer--;
                            if (sSubTimerStateTimer == 0) {
                                gSaveContext.subTimerState = SUBTIMER_STATE_OFF;
                            }
                            break;
                    }
                    break;
            }

            if (((gSaveContext.timerState != TIMER_STATE_OFF) && (gSaveContext.timerState != TIMER_STATE_STOP)) ||
                (gSaveContext.subTimerState != SUBTIMER_STATE_OFF)) {
                timerDigits[0] = timerDigits[1] = svar2 = timerDigits[3] = 0;
                timerDigits[2] = 10; // digit 10 is used as ':' (colon)

                if (gSaveContext.timerState != TIMER_STATE_OFF) {
                    timerDigits[4] = gSaveContext.timerSeconds;
                } else {
                    timerDigits[4] = gSaveContext.subTimerSeconds;
                }

                while (timerDigits[4] >= 60) {
                    timerDigits[1]++;
                    if (timerDigits[1] >= 10) {
                        timerDigits[0]++;
                        timerDigits[1] -= 10;
                    }
                    timerDigits[4] -= 60;
                }

                while (timerDigits[4] >= 10) {
                    timerDigits[3]++;
                    timerDigits[4] -= 10;
                }

                // Clock Icon
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
                gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 0);
                s32 X_Margins_Timer;
                
                    X_Margins_Timer = 0;
                
                svar5 = OTRGetRectDimensionFromLeftEdge(gSaveContext.timerX[timerId] + X_Margins_Timer);
                svar2 = gSaveContext.timerY[timerId];
                

                OVERLAY_DISP =
                    Gfx_TextureIA8(OVERLAY_DISP, gClockIconTex, 16, 16, svar5, svar2 + 2, 16, 16, 1 << 10, 1 << 10);

                // Timer Counter
                gDPPipeSync(OVERLAY_DISP++);
                gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE,
                                  TEXEL0, 0, PRIMITIVE, 0);

                if (gSaveContext.timerState != TIMER_STATE_OFF) {
                    if ((gSaveContext.timerSeconds < 10) && (gSaveContext.timerState <= TIMER_STATE_STOP)) {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 50, 0, 255);
                    } else {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, 255);
                    }
                } else {
                    if ((gSaveContext.subTimerSeconds < 10) && (gSaveContext.subTimerState <= SUBTIMER_STATE_RESPAWN)) {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 50, 0, 255);
                    } else {
                        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 0, 255);
                    }
                }

                for (svar1 = 0; svar1 < 5; svar1++) {
                    // clang-format off
                    //svar5 = svar5 + 8;
                    //svar5 = OTRGetRectDimensionFromLeftEdge(gSaveContext.timerX[timerId]);
                    OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, digitTextures[timerDigits[svar1]], 8, 16,
                                      svar5 + timerDigitLeftPos[svar1],
                                      svar2, digitWidth[svar1], VREG(42), VREG(43) << 1,
                                      VREG(43) << 1);
                    // clang-format on
                }
            }
        }
    }

    if (pauseCtx->debugState == 3) {
        FlagSet_Update(play);
    }

    if (interfaceCtx->unk_244 != 0) {
        gDPPipeSync(OVERLAY_DISP++);
        gSPDisplayList(OVERLAY_DISP++, sSetupDL_80125A60);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, interfaceCtx->unk_244);
        gDPFillRectangle(OVERLAY_DISP++, 0, 0, gScreenWidth - 1, gScreenHeight - 1);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}


void Interface_Update(PlayState* play) {
    static u8 D_80125B60 = 0;
    static s16 sPrevTimeIncrement = 0;
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    s16 alpha;
    s16 alpha1;
    u16 action;
    Input* debugInput = &play->state.input[2];

    Top_HUD_Margin = (0);
    Left_HUD_Margin = (0);
    Right_HUD_Margin = (0);
    Bottom_HUD_Margin = (0);


    bool isPal = ResourceMgr_GetGameRegion(0) == GAME_REGION_PAL;

    if (CHECK_BTN_ALL(debugInput->press.button, BTN_DLEFT)) {
        gSaveContext.language = LANGUAGE_ENG;
        CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_ENG);
        osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
    } else if (CHECK_BTN_ALL(debugInput->press.button, BTN_DUP) && sGerMessageEntryTablePtr != NULL) {
        gSaveContext.language = LANGUAGE_GER;
        CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_GER);
        osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
    } else if (CHECK_BTN_ALL(debugInput->press.button, BTN_DRIGHT) && sFraMessageEntryTablePtr != NULL) {
        gSaveContext.language = LANGUAGE_FRA;
        CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_FRA);
        osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
    } else if (CHECK_BTN_ALL(debugInput->press.button, BTN_DDOWN) && sJpnMessageEntryTablePtr != NULL) {
        // Add this in to have an equivalent ntsc language debugging feature
        gSaveContext.language = LANGUAGE_JPN;
        CVarSetInteger(CVAR_SETTING("Languages"), LANGUAGE_JPN);
        osSyncPrintf("J_N=%x J_N=%x\n", gSaveContext.language, &gSaveContext.language);
    }

    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0)) {
        if ((gSaveContext.minigameState == 1) || (gSaveContext.sceneSetupIndex < 4) ||
            ((play->sceneNum == SCENE_LON_LON_RANCH) && (gSaveContext.sceneSetupIndex == 4))) {
            if ((msgCtx->msgMode == MSGMODE_NONE) ||
                ((msgCtx->msgMode != MSGMODE_NONE) && (play->sceneNum == SCENE_BOMBCHU_BOWLING_ALLEY))) {
                if (play->gameOverCtx.state == GAMEOVER_INACTIVE) {
                    func_80083108(play);
                }
            }
        }
    }

    switch (gSaveContext.unk_13E8) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            alpha = 255 - (gSaveContext.unk_13EC << 5);
            if (alpha < 0) {
                alpha = 0;
            }

            func_80082850(play, alpha);
            gSaveContext.unk_13EC++;

            if (alpha == 0) {
                gSaveContext.unk_13E8 = 0;
            }
            break;
        case 50:
            alpha = 255 - (gSaveContext.unk_13EC << 5);
            if (alpha < 0) {
                alpha = 0;
            }

            alpha1 = 0xFF - alpha;
            if (alpha1 >= 0xFF) {
                alpha1 = 0xFF;
            }

            osSyncPrintf("case 50 : alpha=%d  alpha1=%d\n", alpha, alpha1);
            func_80082644(play, alpha1);

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha1;
            }

            if (interfaceCtx->magicAlpha != 255) {
                interfaceCtx->magicAlpha = alpha1;
            }

            switch (play->sceneNum) {
                case SCENE_HYRULE_FIELD:
                case SCENE_KAKARIKO_VILLAGE:
                case SCENE_GRAVEYARD:
                case SCENE_ZORAS_RIVER:
                case SCENE_KOKIRI_FOREST:
                case SCENE_SACRED_FOREST_MEADOW:
                case SCENE_LAKE_HYLIA:
                case SCENE_ZORAS_DOMAIN:
                case SCENE_ZORAS_FOUNTAIN:
                case SCENE_GERUDO_VALLEY:
                case SCENE_LOST_WOODS:
                case SCENE_DESERT_COLOSSUS:
                case SCENE_GERUDOS_FORTRESS:
                case SCENE_HAUNTED_WASTELAND:
                case SCENE_HYRULE_CASTLE:
                case SCENE_DEATH_MOUNTAIN_TRAIL:
                case SCENE_DEATH_MOUNTAIN_CRATER:
                case SCENE_GORON_CITY:
                case SCENE_LON_LON_RANCH:
                case SCENE_OUTSIDE_GANONS_CASTLE:
                    if (interfaceCtx->minimapAlpha < 170) {
                        interfaceCtx->minimapAlpha = alpha1;
                    } else {
                        interfaceCtx->minimapAlpha = 170;
                    }
                    break;
                default:
                    if (interfaceCtx->minimapAlpha != 255) {
                        interfaceCtx->minimapAlpha = alpha1;
                    }
                    break;
            }

            gSaveContext.unk_13EC++;
            if (alpha1 == 0xFF) {
                gSaveContext.unk_13E8 = 0;
            }

            break;
        case 52:
            gSaveContext.unk_13E8 = 1;
            func_80082850(play, 0);
            gSaveContext.unk_13E8 = 0;
        default:
            break;
    }

    Map_Update(play);

    if (gSaveContext.healthAccumulator != 0) {
        gSaveContext.healthAccumulator -= 4;
        gSaveContext.health += 4;

        if ((gSaveContext.health & 0xF) < 4) {
            Audio_PlaySoundGeneral(NA_SE_SY_HP_RECOVER, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }

        osSyncPrintf("now_life=%d  max_life=%d\n", gSaveContext.health, gSaveContext.healthCapacity);

        if (gSaveContext.health >= gSaveContext.healthCapacity) {
            gSaveContext.health = gSaveContext.healthCapacity;
            osSyncPrintf("S_Private.now_life=%d  S_Private.max_life=%d\n", gSaveContext.health,
                         gSaveContext.healthCapacity);
            gSaveContext.healthAccumulator = 0;
        }
    }

    HealthMeter_HandleCriticalAlarm(play);
    sEnvHazard = Player_GetEnvironmentalHazard(play);

    if (sEnvHazard == PLAYER_ENV_HAZARD_HOTROOM) {
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) == EQUIP_VALUE_TUNIC_GORON) {
            sEnvHazard = PLAYER_ENV_HAZARD_NONE;
        }
    } else if ((Player_GetEnvironmentalHazard(play) >= 2) && (Player_GetEnvironmentalHazard(play) < 5)) {
        if (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC) == EQUIP_VALUE_TUNIC_ZORA) {
            sEnvHazard = PLAYER_ENV_HAZARD_NONE;
        }
    }

    HealthMeter_Update(play);

    if ((gSaveContext.timerState >= TIMER_STATE_ENV_HAZARD_MOVE) && (play->pauseCtx.state == 0) &&
        (play->pauseCtx.debugState == 0) && (msgCtx->msgMode == MSGMODE_NONE) &&
        !(player->stateFlags2 & PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF) &&
        !Play_InCsMode(play)) {}

    if (gSaveContext.ship.pendingSale != ITEM_NONE) {
        gSaveContext.ship.pendingSale = ITEM_NONE;
        gSaveContext.ship.pendingSaleMod = MOD_NONE;
    }

    switch (interfaceCtx->unk_1EC) {
        case 1:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 15700.0f) {
                interfaceCtx->unk_1F4 = -15700.0f;
                interfaceCtx->unk_1EC = 2;
            }
            break;
        case 2:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 0.0f) {
                interfaceCtx->unk_1F4 = 0.0f;
                interfaceCtx->unk_1EC = 0;
                interfaceCtx->unk_1EE = interfaceCtx->unk_1F0;
                action = interfaceCtx->unk_1EE;
                if ((action == DO_ACTION_MAX) || (action == DO_ACTION_MAX + 1)) {
                    action = DO_ACTION_NONE;
                }
                Interface_LoadActionLabel(interfaceCtx, action, 0);
            }
            break;
        case 3:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 15700.0f) {
                interfaceCtx->unk_1F4 = -15700.0f;
                interfaceCtx->unk_1EC = 2;
            }
            break;
        case 4:
            interfaceCtx->unk_1F4 += 31400.0f / WREG(5);
            if (interfaceCtx->unk_1F4 >= 0.0f) {
                interfaceCtx->unk_1F4 = 0.0f;
                interfaceCtx->unk_1EC = 0;
                interfaceCtx->unk_1EE = interfaceCtx->unk_1F0;
                action = interfaceCtx->unk_1EE;
                if ((action == DO_ACTION_MAX) || (action == DO_ACTION_MAX + 1)) {
                    action = DO_ACTION_NONE;
                }
                Interface_LoadActionLabel(interfaceCtx, action, 0);
            }
            break;
    }

    WREG(7) = interfaceCtx->unk_1F4;

    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (msgCtx->msgMode == MSGMODE_NONE) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->gameOverCtx.state == GAMEOVER_INACTIVE) &&
        (play->transitionMode == TRANS_MODE_OFF) && ((play->csCtx.state == CS_STATE_IDLE) || !Player_InCsMode(play))) {
        if ((gSaveContext.isMagicAcquired != 0) && (gSaveContext.magicLevel == 0)) {
            gSaveContext.magicLevel = gSaveContext.isDoubleMagicAcquired + 1;
            gSaveContext.magicState = MAGIC_STATE_STEP_CAPACITY;
            osSyncPrintf(VT_FGCOL(YELLOW));
            osSyncPrintf("魔法スター─────ト！！！！！！！！！\n"); // "Magic Start!!!!!!!!!"
            osSyncPrintf("MAGIC_MAX=%d\n", gSaveContext.magicLevel);
            osSyncPrintf("MAGIC_NOW=%d\n", gSaveContext.magic);
            osSyncPrintf("Z_MAGIC_NOW_NOW=%d\n", gSaveContext.magicFillTarget);
            osSyncPrintf("Z_MAGIC_NOW_MAX=%d\n", gSaveContext.magicCapacity);
            osSyncPrintf(VT_RST);
        }

        Interface_UpdateMagicBar(play);
    }

    if (gSaveContext.timerState == TIMER_STATE_OFF) {
        if (((sEnvHazard == PLAYER_ENV_HAZARD_HOTROOM) || (sEnvHazard == PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) ||
             (sEnvHazard == PLAYER_ENV_HAZARD_UNDERWATER_FREE)) &&
            ((gSaveContext.health >> 1) != 0)) {
            gSaveContext.timerState = TIMER_STATE_ENV_HAZARD_INIT;
            gSaveContext.timerX[TIMER_ID_MAIN] = 140;
            gSaveContext.timerY[TIMER_ID_MAIN] = 80;
            sEnvHazardActive = true;
        }
    } else {
        if (((sEnvHazard == PLAYER_ENV_HAZARD_NONE) || (sEnvHazard == PLAYER_ENV_HAZARD_SWIMMING)) &&
            (gSaveContext.timerState <= TIMER_STATE_ENV_HAZARD_TICK)) {
            gSaveContext.timerState = TIMER_STATE_OFF;
        }
    }

    if (gSaveContext.minigameState == 1) {
        gSaveContext.minigameScore += interfaceCtx->unk_23C;
        interfaceCtx->unk_23C = 0;

        if (sHBAScoreTier == 0) {
            if (gSaveContext.minigameScore >= 1000) {
                sHBAScoreTier++;
            }
        } else if (sHBAScoreTier == 1) {
            if (gSaveContext.minigameScore >= 1500) {
                sHBAScoreTier++;
            }
        }

        sHBAScoreDigits[0] = sHBAScoreDigits[1] = 0;
        sHBAScoreDigits[2] = 0;
        sHBAScoreDigits[3] = gSaveContext.minigameScore;

        while (sHBAScoreDigits[3] >= 1000) {
            sHBAScoreDigits[0]++;
            sHBAScoreDigits[3] -= 1000;
        }

        while (sHBAScoreDigits[3] >= 100) {
            sHBAScoreDigits[1]++;
            sHBAScoreDigits[3] -= 100;
        }

        while (sHBAScoreDigits[3] >= 10) {
            sHBAScoreDigits[2]++;
            sHBAScoreDigits[3] -= 10;
        }
    }

    if (gSaveContext.sunsSongState != SUNSSONG_INACTIVE) {
        // exit out of ocarina mode after suns song finishes playing
        if ((msgCtx->ocarinaAction != OCARINA_ACTION_CHECK_NOWARP_DONE) &&
            (gSaveContext.sunsSongState == SUNSSONG_START)) {
            play->msgCtx.ocarinaMode = OCARINA_MODE_04;
        }

        // handle suns song in areas where time moves
        if (play->envCtx.timeIncrement != 0) {
            if (gSaveContext.sunsSongState != SUNSSONG_SPEED_TIME) {
                D_80125B60 = 0;
                if ((gSaveContext.dayTime >= 0x4555) && (gSaveContext.dayTime <= 0xC001)) {
                    D_80125B60 = 1;
                }

                gSaveContext.sunsSongState = SUNSSONG_SPEED_TIME;
                sPrevTimeIncrement = gTimeIncrement;
                gTimeIncrement = 400;
            } else if (D_80125B60 == 0) {
                if ((gSaveContext.dayTime >= 0x4555) && (gSaveContext.dayTime <= 0xC001)) {
                    gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
                    gTimeIncrement = sPrevTimeIncrement;
                    play->msgCtx.ocarinaMode = OCARINA_MODE_04;
                }
            } else if (gSaveContext.dayTime > 0xC001) {
                gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
                gTimeIncrement = sPrevTimeIncrement;
                play->msgCtx.ocarinaMode = OCARINA_MODE_04;
            }
        } else if ((play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_1) &&
                   (interfaceCtx->restrictions.sunsSong != 3)) {
            if ((gSaveContext.dayTime >= 0x4555) && (gSaveContext.dayTime < 0xC001)) {
                gSaveContext.nextDayTime = 0;
                play->transitionType = TRANS_TYPE_FADE_BLACK_FAST;
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
                play->unk_11DE9 = 1;
            } else {
                gSaveContext.nextDayTime = 0x8001;
                play->transitionType = TRANS_TYPE_FADE_WHITE_FAST;
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
                play->unk_11DE9 = 1;
            }

            if (play->sceneNum == SCENE_HAUNTED_WASTELAND) {
                play->transitionType = TRANS_TYPE_SANDSTORM_PERSIST;
                gSaveContext.nextTransitionType = TRANS_TYPE_SANDSTORM_PERSIST;
            }

            gSaveContext.respawnFlag = -2;
            play->nextEntranceIndex = gSaveContext.entranceIndex;

            play->transitionTrigger = TRANS_TRIGGER_START;
            gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
            func_800F6964(30);
            gSaveContext.seqId = (u8)NA_BGM_DISABLED;
            gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
        } else {
            gSaveContext.sunsSongState = SUNSSONG_SPECIAL;
        }
    }
}
