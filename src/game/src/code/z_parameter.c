#include "global.h"
#include "vt.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

// The button statuses include the A button when most things are only the equip item buttons
// So, when indexing into it with a item button index, we need to adjust
#define BUTTON_STATUS_INDEX(button) ((button) >= 4) ? ((button) + 1) : (button)

static int16_t sHBAScoreTier = 0;
static uint16_t sHBAScoreDigits[] = { 0, 0, 0, 0 };

static uint16_t sCUpInvisible = 0;
static uint16_t sCUpTimer = 0;

static int16_t sEnvHazard = PLAYER_ENV_HAZARD_NONE;
static int16_t sEnvHazardActive = false;

// original name: "alpha_change"
void Interface_ChangeAlpha(uint16_t alphaType) {
    if (alphaType != gSaveContext.unk_13EA) {
        osSyncPrintf("ＡＬＰＨＡーＴＹＰＥ＝%d  LAST_TIME_TYPE=%d\n", alphaType, gSaveContext.unk_13EE);
        gSaveContext.unk_13EA = gSaveContext.unk_13E8 = alphaType;
        gSaveContext.unk_13EC = 1;
    }
}

void func_80082644(PlayState* play, int16_t alpha) {
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

void func_8008277C(PlayState* play, int16_t maxAlpha, int16_t alpha) {
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

void func_80082850(PlayState* play, int16_t maxAlpha) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    int16_t alpha = 255 - maxAlpha;

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

            if ((interfaceCtx->counterAlpha != 0) && (interfaceCtx->counterAlpha > maxAlpha)) {
                interfaceCtx->counterAlpha = maxAlpha;
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

            if ((interfaceCtx->counterAlpha != 0) && (interfaceCtx->counterAlpha > maxAlpha)) {
                interfaceCtx->counterAlpha = maxAlpha;
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

            if ((interfaceCtx->counterAlpha != 0) && (interfaceCtx->counterAlpha > maxAlpha)) {
                interfaceCtx->counterAlpha = maxAlpha;
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

            if (interfaceCtx->counterAlpha != 255) {
                interfaceCtx->counterAlpha = alpha;
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

            if (interfaceCtx->counterAlpha != 255) {
                interfaceCtx->counterAlpha = alpha;
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

            if (interfaceCtx->counterAlpha != 255) {
                interfaceCtx->counterAlpha = alpha;
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

            if (interfaceCtx->counterAlpha != 255) {
                interfaceCtx->counterAlpha = alpha;
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

            if ((interfaceCtx->counterAlpha != 0) && (interfaceCtx->counterAlpha > maxAlpha)) {
                interfaceCtx->counterAlpha = maxAlpha;
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

            if ((interfaceCtx->counterAlpha != 0) && (interfaceCtx->counterAlpha > maxAlpha)) {
                interfaceCtx->counterAlpha = maxAlpha;
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





            if ((interfaceCtx->counterAlpha != 0) && (interfaceCtx->counterAlpha > maxAlpha)) {
                interfaceCtx->counterAlpha = maxAlpha;
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

            if (interfaceCtx->counterAlpha != 255) {
                interfaceCtx->counterAlpha = alpha;
            }

            break;
    }

    if ((play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_1) && (interfaceCtx->minimapAlpha >= 255)) {
        interfaceCtx->minimapAlpha = 255;
    }
}

// B no longer owns an item or HUD action in the reduced test01 runtime.
// Keep its legacy save-array slot inert while leaving the three saved C-button
// slots available for Master Sword, Biggoron Sword, and Bow.
void func_80083108(PlayState* play) {
    gSaveContext.equips.buttonItems[0] = ITEM_NONE;
    gSaveContext.buttonStatus[0] = BTN_DISABLED;
    gSaveContext.buttonStatus[1] = BTN_ENABLED;
    gSaveContext.buttonStatus[2] = BTN_ENABLED;
    gSaveContext.buttonStatus[3] = BTN_ENABLED;
    play->interfaceCtx.bAlpha = 0;
    gSaveContext.forceRisingButtonAlphas = 0;
}

void Inventory_SwapAgeEquipment(void) {
    // This port is adult-only; age swaps cannot replace the fixed C-button loadout.
    gSaveContext.equips.buttonItems[0] = ITEM_NONE;
    gSaveContext.adultEquips.buttonItems[0] = ITEM_NONE;
    gSaveContext.childEquips.buttonItems[0] = ITEM_NONE;
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
    gSaveContext.equips.buttonItems[0] = ITEM_NONE;
}

void Interface_LoadItemIcon1(PlayState* play, uint16_t button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    osCreateMesgQueue(&interfaceCtx->loadQueue, &interfaceCtx->loadMsg, OS_MESG_BLOCK);
    DmaMgr_SendRequest2(&interfaceCtx->dmaRequest_160, interfaceCtx->iconItemSegment + button * 0x1000,
                        (uintptr_t)_icon_item_staticSegmentRomStart +
                            (gSaveContext.equips.buttonItems[button] * 0x1000),
                        0x1000, 0, &interfaceCtx->loadQueue, OS_MESG_PTR(NULL), __FILE__, __LINE__);
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);
}

void Interface_LoadItemIcon2(PlayState* play, uint16_t button) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    osCreateMesgQueue(&interfaceCtx->loadQueue, &interfaceCtx->loadMsg, OS_MESG_BLOCK);
    DmaMgr_SendRequest2(&interfaceCtx->dmaRequest_180, interfaceCtx->iconItemSegment + button * 0x1000,
                        (uintptr_t)_icon_item_staticSegmentRomStart +
                            (gSaveContext.equips.buttonItems[button] * 0x1000),
                        0x1000, 0, &interfaceCtx->loadQueue, OS_MESG_PTR(NULL), __FILE__, __LINE__);
    osRecvMesg(&interfaceCtx->loadQueue, NULL, OS_MESG_BLOCK);
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
 * @return uint8_t
 */
uint8_t Item_Give(uint8_t item) {
    switch (item) {
        case ITEM_SWORD_MASTER:
            gSaveContext.inventory.equipment |=
                OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
            gSaveContext.equips.equipment &= (uint16_t)~(0xF << (EQUIP_TYPE_SWORD * 4));
            gSaveContext.equips.equipment |= EQUIP_VALUE_SWORD_MASTER << (EQUIP_TYPE_SWORD * 4);
            return ITEM_NONE;

        case ITEM_SWORD_BGS:
            gSaveContext.inventory.equipment |=
                OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
            gSaveContext.swordHealth = 8;
            return ITEM_NONE;

        case ITEM_BOW:
            return ITEM_NONE;

        case ITEM_SHIELD_MIRROR:
            gSaveContext.inventory.equipment |=
                OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR);
            return ITEM_NONE;

        default:
            return ITEM_NONE;
    }
}

uint8_t Item_CheckObtainability(uint8_t item) {
    switch (item) {
        case ITEM_SWORD_MASTER:
            return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER) ? item : ITEM_NONE;

        case ITEM_SWORD_BGS:
            return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON) ? item : ITEM_NONE;

        case ITEM_BOW:
            return item;

        case ITEM_SHIELD_MIRROR:
            return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR) ? item : ITEM_NONE;

        default:
            return ITEM_NONE;
    }
}
void func_80086D5C(int32_t* buf, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        buf[i] = 0;
    }
}

void Interface_SetDoAction(PlayState* play, uint16_t action) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if (interfaceCtx->unk_1F0 != action) {
        interfaceCtx->unk_1F0 = action;
        interfaceCtx->unk_1EC = 1;
        interfaceCtx->unk_1F4 = 0.0f;
    }
}

int32_t Health_ChangeBy(PlayState* play, int16_t healthChange) {
    uint16_t heartCount = { 0 };
    uint16_t healthLevel = { 0 };

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

void Rupees_ChangeBy(int64_t rupeeChange) {
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

void Interface_Update(PlayState* play) {
    static uint8_t D_80125B60 = 0;
    static int16_t sPrevTimeIncrement = 0;
    MessageContext* msgCtx = &play->msgCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    int16_t alpha = { 0 };
    Input* debugInput = &play->state.input[2];

    {
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

            int16_t alpha1 = 0xFF - alpha;
            if (alpha1 >= 0xFF) {
                alpha1 = 0xFF;
            }

            osSyncPrintf("case 50 : alpha=%d  alpha1=%d\n", alpha, alpha1);
            func_80082644(play, alpha1);

            if (interfaceCtx->healthAlpha != 255) {
                interfaceCtx->healthAlpha = alpha1;
            }

            if (interfaceCtx->counterAlpha != 255) {
                interfaceCtx->counterAlpha = alpha1;
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

    HealthMeter_Update(play);

    if ((gSaveContext.timerState >= TIMER_STATE_ENV_HAZARD_MOVE) && (msgCtx->msgMode == MSGMODE_NONE) &&
        (play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF) &&
        !Play_InCsMode(play)) {}

    if (gSaveContext.game.pendingSale != ITEM_NONE) {
        gSaveContext.game.pendingSale = ITEM_NONE;
        gSaveContext.game.pendingSaleMod = MOD_NONE;
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
            }
            break;
    }

    WREG(7) = interfaceCtx->unk_1F4;

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
        } else if (play->roomCtx.curRoom.behaviorType1 != ROOM_BEHAVIOR_TYPE1_1) {
            if ((gSaveContext.dayTime >= 0x4555) && (gSaveContext.dayTime < 0xC001)) {
                gSaveContext.nextDayTime = 0;
                play->transitionType = TRANS_TYPE_FADE_BLACK;
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
                play->unk_11DE9 = 1;
            } else {
                gSaveContext.nextDayTime = 0x8001;
                play->transitionType = TRANS_TYPE_FADE_BLACK;
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
                play->unk_11DE9 = 1;
            }

            if (play->sceneNum == SCENE_HAUNTED_WASTELAND) {
                play->transitionType = TRANS_TYPE_FADE_BLACK;
                gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
            }

            gSaveContext.respawnFlag = -2;
            play->nextEntranceIndex = gSaveContext.entranceIndex;

            play->transitionTrigger = TRANS_TRIGGER_START;
            gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
            func_800F6964(30);
            gSaveContext.seqId = (uint8_t)NA_BGM_DISABLED;
            gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
        } else {
            gSaveContext.sunsSongState = SUNSSONG_SPECIAL;
        }
    }
}
