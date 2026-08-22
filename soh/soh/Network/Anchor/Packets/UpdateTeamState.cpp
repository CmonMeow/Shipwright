#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/JsonConversions.hpp"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
#include "soh/OTRGlobals.h"
#include "soh/Notification/Notification.h"

extern "C" {
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
}

/**
 * UPDATE_TEAM_STATE
 *
 * Pushes the current save state to the server for other teammates to use.
 *
 * Fires when the server passes on a REQUEST_TEAM_STATE packet, or when this client saves the game
 *
 * When sending this packet we will assume that the team queue has been emptied for this client, so the queue
 * stored in the server will be cleared.
 *
 * When receiving this packet, if there is items in the team queue, we will play them back in order.
 */

void Anchor::SendPacket_UpdateTeamState() {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

    // Assume the team queue has been emptied, so clear it
    payload["queue"] = json::array();

    payload["state"] = gSaveContext;
    // manually update current scene flags
    payload["state"]["sceneFlags"][gPlayState->sceneNum * 4] = gPlayState->actorCtx.flags.chest;
    payload["state"]["sceneFlags"][gPlayState->sceneNum * 4 + 1] = gPlayState->actorCtx.flags.swch;
    payload["state"]["sceneFlags"][gPlayState->sceneNum * 4 + 2] = gPlayState->actorCtx.flags.clear;
    payload["state"]["sceneFlags"][gPlayState->sceneNum * 4 + 3] = gPlayState->actorCtx.flags.collect;

    SendJsonToRemote(payload);
}

void Anchor::SendPacket_ClearTeamState(std::string teamId) {
    json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = teamId;
    payload["queue"] = json::array();
    payload["state"] = json::object();
    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateTeamState(nlohmann::json payload) {
    if (!roomState.syncItemsAndFlags) {
        return;
    }

    isHandlingUpdateTeamState = true;
    // This can happen in between file select and the game starting, so we cant use this check, but we need to ensure we
    // be careful to wrap PlayState usage in this check
    // if (!IsSaveLoaded()) {
    //     return;
    // }

    if (payload.contains("state")) {
        SaveContext loadedData = payload["state"].get<SaveContext>();

        gSaveContext.healthCapacity = loadedData.healthCapacity;
        gSaveContext.magicLevel = loadedData.magicLevel;
        gSaveContext.magicCapacity = gSaveContext.magic = loadedData.magicCapacity;
        gSaveContext.isMagicAcquired = loadedData.isMagicAcquired;
        gSaveContext.isDoubleMagicAcquired = loadedData.isDoubleMagicAcquired;
        gSaveContext.isDoubleDefenseAcquired = loadedData.isDoubleDefenseAcquired;
        gSaveContext.bgsFlag = loadedData.bgsFlag;
        gSaveContext.swordHealth = loadedData.swordHealth;
        gSaveContext.ship.quest = loadedData.ship.quest;

        for (int i = 0; i < 124; i++) {
            if (i == SCENE_WATER_TEMPLE) {
                // Keep water temple water level flags
                u32 mask = (1 << 0x1C) | (1 << 0x1D) | (1 << 0x1E);
                loadedData.sceneFlags[i].swch =
                    (loadedData.sceneFlags[i].swch & ~mask) | (gSaveContext.sceneFlags[i].swch & mask);
            }

            if (i == SCENE_FOREST_TEMPLE) {
                // Keep forest temple elevator flag
                u32 mask = (1 << 0x1B);
                loadedData.sceneFlags[i].swch =
                    (loadedData.sceneFlags[i].swch & ~mask) | (gSaveContext.sceneFlags[i].swch & mask);
            }

            gSaveContext.sceneFlags[i] = loadedData.sceneFlags[i];
            if (IsSaveLoaded() && gPlayState->sceneNum == i) {
                gPlayState->actorCtx.flags.chest = loadedData.sceneFlags[i].chest;
                gPlayState->actorCtx.flags.swch = loadedData.sceneFlags[i].swch;
                gPlayState->actorCtx.flags.clear = loadedData.sceneFlags[i].clear;
                gPlayState->actorCtx.flags.collect = loadedData.sceneFlags[i].collect;
            }
        }

        for (int i = 0; i < 14; i++) {
            gSaveContext.eventChkInf[i] = loadedData.eventChkInf[i];
        }

        for (int i = 0; i < 4; i++) {
            gSaveContext.itemGetInf[i] = loadedData.itemGetInf[i];
        }

        // Skip last row of infTable, don't want to sync swordless flag
        for (int i = 0; i < 29; i++) {
            gSaveContext.infTable[i] = loadedData.infTable[i];
        }

        for (int i = 0; i < 6; i++) {
            gSaveContext.gsFlags[i] = loadedData.gsFlags[i];
        }

        gSaveContext.ship.stats.firstInput = loadedData.ship.stats.firstInput;
        gSaveContext.ship.stats.fileCreatedAt = loadedData.ship.stats.fileCreatedAt;

        // Restore master sword state
        // Disabling this for now, not really sure I understand why I did this in the past
        // u8 hasMasterSword = CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, 1);
        // if (hasMasterSword) {
        //     loadedData.inventory.equipment |= 0x2;
        // } else {
        //     loadedData.inventory.equipment &= ~0x2;
        // }

        // Restore bottle contents (unless it's ruto's letter)
        for (int i = 0; i < 4; i++) {
            if (gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] != ITEM_NONE &&
                gSaveContext.inventory.items[SLOT_BOTTLE_1 + i] != ITEM_LETTER_RUTO) {
                loadedData.inventory.items[SLOT_BOTTLE_1 + i] = gSaveContext.inventory.items[SLOT_BOTTLE_1 + i];
            }
        }

        // Restore ammo if it's non-zero, unless it's beans
        for (int i = 0; i < ARRAY_COUNT(gSaveContext.inventory.ammo); i++) {
            if (gSaveContext.inventory.ammo[i] != 0 && i != SLOT(ITEM_BEAN) && i != SLOT(ITEM_BEAN + 1)) {
                loadedData.inventory.ammo[i] = gSaveContext.inventory.ammo[i];
            }
        }

        gSaveContext.inventory = loadedData.inventory;

        Notification::Emit({
            .message = "Save updated from team",
        });
    }

    if (payload.contains("queue")) {
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        for (auto& item : payload["queue"]) {
            nlohmann::json itemPayload = nlohmann::json::parse(item.get<std::string>());
            incomingPacketQueue.push(itemPayload);
        }
    }
    isHandlingUpdateTeamState = false;
}
