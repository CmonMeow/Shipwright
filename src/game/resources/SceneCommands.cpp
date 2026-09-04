#include <runtime/log/Log.hpp>
#include "resources/ResourceManagerHelpers.h"
#include "resources/SceneLoader.h"
#include "resources/type/Scene.h"
#include <engine/utils/StringHelper.h>
#include "global.h"
#include "vt.h"
#include "resources/type/CollisionHeader.h"
#include <rendering/resource/type/DisplayList.h>
#include <engine/resource/type/Blob.h>
#include <memory>
#include <cassert>
#include "resources/type/scenecommand/SetCameraSettings.h"
#include "resources/type/scenecommand/SetStartPositionList.h"
#include "resources/type/scenecommand/SetCollisionHeader.h"
#include "resources/type/scenecommand/SetRoomList.h"
#include "resources/type/scenecommand/SetEntranceList.h"
#include "resources/type/scenecommand/SetSpecialObjects.h"
#include "resources/type/scenecommand/SetRoomBehavior.h"
#include "resources/type/scenecommand/SetMesh.h"
#include "resources/type/scenecommand/SetSkyboxSettings.h"
#include "resources/type/scenecommand/SetSkyboxModifier.h"
#include "resources/type/scenecommand/SetTimeSettings.h"
#include "resources/type/scenecommand/SetSoundSettings.h"
#include "resources/type/scenecommand/SetEchoSettings.h"

extern "C" int32_t Object_Spawn(ObjectContext* objectCtx, int16_t objectId);

bool Scene_CommandSpawnList(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* startPositions = static_cast<Game::Resources::SetStartPositionList*>(cmd);
    ActorEntry* entries = static_cast<ActorEntry*>(startPositions->GetRawPointer());

    play->linkActorEntry = &entries[play->setupEntranceList[play->curSpawn].spawn];
    play->linkAgeOnLoad = LINK_AGE_ADULT;
    Object_Spawn(&play->objectCtx, OBJECT_LINK_BOY);

    return false;
}



bool Scene_CommandCollisionHeader(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* collision = static_cast<Game::Resources::SetCollisionHeader*>(cmd);
    BgCheck_Allocate(&play->colCtx, play, static_cast<CollisionHeader*>(collision->GetRawPointer()));

    return false;
}

bool Scene_CommandRoomList(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* roomList = static_cast<Game::Resources::SetRoomList*>(cmd);

    play->numRooms = roomList->numRooms;
    play->roomList = static_cast<RomFile*>(roomList->GetRawPointer());

    return false;
}

bool Scene_CommandEntranceList(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* entrances = static_cast<Game::Resources::SetEntranceList*>(cmd);
    play->setupEntranceList = static_cast<EntranceEntry*>(entrances->GetRawPointer());

    return false;
}

bool Scene_CommandSpecialFiles(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* specialObjects = static_cast<Game::Resources::SetSpecialObjects*>(cmd);

    if (specialObjects->specialObjects.globalObject != 0) {
        play->objectCtx.subKeepIndex = Object_Spawn(&play->objectCtx, specialObjects->specialObjects.globalObject);
    }

    return false;
}

bool Scene_CommandRoomBehavior(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* roomBehavior = static_cast<Game::Resources::SetRoomBehavior*>(cmd);

    play->roomCtx.curRoom.behaviorType1 = roomBehavior->roomBehavior.gameplayFlags;
    play->roomCtx.curRoom.behaviorType2 = roomBehavior->roomBehavior.gameplayFlags2 & 0xFF;
    play->roomCtx.curRoom.lensMode = (roomBehavior->roomBehavior.gameplayFlags2 >> 8) & 1;
    play->msgCtx.disableWarpSongs = (roomBehavior->roomBehavior.gameplayFlags2 >> 0xA) & 1;

    return false;
}

bool Scene_CommandMeshHeader(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* mesh = static_cast<Game::Resources::SetMesh*>(cmd);
    play->roomCtx.curRoom.meshHeader = static_cast<MeshHeader*>(mesh->GetRawPointer());

    return false;
}

bool Scene_CommandLightSettingsList(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    play->envCtx.lightSettingsList = (EnvLightSettings*)cmd->GetRawPointer();

    return false;
}

// Scene Command 0x11: Skybox Settings
bool Scene_CommandSkyboxSettings(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* skybox = static_cast<Game::Resources::SetSkyboxSettings*>(cmd);

    play->skyboxId = skybox->settings.skyboxId;
    play->envCtx.unk_17 = play->envCtx.unk_18 = skybox->settings.weather;
    play->envCtx.indoors = skybox->settings.indoors;

    return false;
}

bool Scene_CommandSkyboxDisables(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* skyboxModifier = static_cast<Game::Resources::SetSkyboxModifier*>(cmd);

    play->envCtx.sunMoonDisabled = skyboxModifier->modifier.sunMoonDisabled;
    play->envCtx.skyboxDisabled = skyboxModifier->modifier.skyboxDisabled;

    return false;
}

bool Scene_CommandTimeSettings(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* timeSettings = static_cast<Game::Resources::SetTimeSettings*>(cmd);

    if ((timeSettings->settings.hour != 0xFF) && (timeSettings->settings.minute != 0xFF)) {
        gSaveContext.skyboxTime = gSaveContext.dayTime =
            ((timeSettings->settings.hour + (timeSettings->settings.minute / 60.0f)) * 60.0f) /
            (static_cast<float>(24 * 60) / 0x10000);
    }

    if (timeSettings->settings.timeIncrement != 0xFF) {
        play->envCtx.timeIncrement = timeSettings->settings.timeIncrement;
    } else {
        play->envCtx.timeIncrement = 0;
    }

    if (gSaveContext.sunsSongState == SUNSSONG_INACTIVE) {
        gTimeIncrement = play->envCtx.timeIncrement;
    }

    play->envCtx.sunPos.x = -(Math_SinS(((void)0, gSaveContext.dayTime) - 0x8000) * 120.0f) * 25.0f;
    play->envCtx.sunPos.y = (Math_CosS(((void)0, gSaveContext.dayTime) - 0x8000) * 120.0f) * 25.0f;
    play->envCtx.sunPos.z = (Math_CosS(((void)0, gSaveContext.dayTime) - 0x8000) * 20.0f) * 25.0f;

    if (((play->envCtx.timeIncrement == 0) && (gSaveContext.cutsceneIndex < 0xFFF0)) ||
        (gSaveContext.entranceIndex == ENTR_LAKE_HYLIA_WARP_PAD)) {
        gSaveContext.skyboxTime = ((void)0, gSaveContext.dayTime);
        if ((gSaveContext.skyboxTime >= 0x2AAC) && (gSaveContext.skyboxTime < 0x4555)) {
            gSaveContext.skyboxTime = 0x3556;
        } else if ((gSaveContext.skyboxTime >= 0x4555) && (gSaveContext.skyboxTime < 0x5556)) {
            gSaveContext.skyboxTime = 0x5556;
        } else if ((gSaveContext.skyboxTime >= 0xAAAB) && (gSaveContext.skyboxTime < 0xB556)) {
            gSaveContext.skyboxTime = 0xB556;
        } else if ((gSaveContext.skyboxTime >= 0xC001) && (gSaveContext.skyboxTime < 0xCAAC)) {
            gSaveContext.skyboxTime = 0xCAAC;
        }
    }

    return false;
}




bool Scene_CommandSoundSettings(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* soundSettings = static_cast<Game::Resources::SetSoundSettings*>(cmd);

    play->sequenceCtx.seqId = soundSettings->settings.seqId;
    play->sequenceCtx.natureAmbienceId = soundSettings->settings.natureAmbienceId;

    if (gSaveContext.seqId == 0xFF) {
        Audio_QueueSeqCmd(soundSettings->settings.reverb | 0xF0000000);
    }

    return false;
}

bool Scene_CommandEchoSettings(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* echoSettings = static_cast<Game::Resources::SetEchoSettings*>(cmd);

    play->roomCtx.curRoom.echo = echoSettings->settings.echo;

    return false;
}



// Camera & World Map Area
bool Scene_CommandMiscSettings(PlayState* play, Game::Resources::ISceneCommand* cmd) {
    auto* cameraSettings = static_cast<Game::Resources::SetCameraSettings*>(cmd);

    YREG(15) = cameraSettings->settings.cameraMovement;
    gSaveContext.worldMapArea = cameraSettings->settings.worldMapArea;

    if ((play->sceneNum == SCENE_BAZAAR) || (play->sceneNum == SCENE_SHOOTING_GALLERY)) {
        if (LINK_AGE_IN_YEARS == YEARS_ADULT) {
            gSaveContext.worldMapArea = 1;
        }
    }

    if (((play->sceneNum >= SCENE_HYRULE_FIELD) && (play->sceneNum <= SCENE_OUTSIDE_GANONS_CASTLE)) ||
        ((play->sceneNum >= SCENE_MARKET_ENTRANCE_DAY) && (play->sceneNum <= SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS))) {
        if (gSaveContext.cutsceneIndex < 0xFFF0) {
            gSaveContext.worldMapAreaData |= gBitFlags[gSaveContext.worldMapArea];
            osSyncPrintf("０００  ａｒｅａ＿ａｒｒｉｖａｌ＝%x (%d)\n", gSaveContext.worldMapAreaData,
                         gSaveContext.worldMapArea);
        }
    }
    return false;
}

int32_t Game::Resources::ExecuteSceneCommands(PlayState* play, Scene* scene) {
    for (const auto& sceneCmd : scene->commands) {
        if (sceneCmd == nullptr) {
            continue;
        }

        switch (sceneCmd->cmdId) {
            case Game::Resources::SceneCommandID::SetStartPositionList:
                Scene_CommandSpawnList(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetCollisionHeader:
                Scene_CommandCollisionHeader(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetRoomList:
                Scene_CommandRoomList(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetEntranceList:
                Scene_CommandEntranceList(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetSpecialObjects:
                Scene_CommandSpecialFiles(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetRoomBehavior:
                Scene_CommandRoomBehavior(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetMesh:
                Scene_CommandMeshHeader(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetLightingSettings:
                Scene_CommandLightSettingsList(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetTimeSettings:
                Scene_CommandTimeSettings(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetSkyboxSettings:
                Scene_CommandSkyboxSettings(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetSkyboxModifier:
                Scene_CommandSkyboxDisables(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetSoundSettings:
                Scene_CommandSoundSettings(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetEchoSettings:
                Scene_CommandEchoSettings(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::SetCameraSettings:
                Scene_CommandMiscSettings(play, sceneCmd.get());
                break;
            case Game::Resources::SceneCommandID::EndMarker:
                return 0;
            default:
                WriteLog("Rejected unsupported test01 scene command {}", static_cast<uint32_t>(sceneCmd->cmdId));
                return -1;
        }
    }
    return 0;
}

extern "C" int32_t SceneLoader_FinalizeRoom(PlayState* play, RoomContext* roomCtx) {
    if (roomCtx->status == 1) {
        // if (!osRecvMesg(&roomCtx->loadQueue, NULL, OS_MESG_NOBLOCK)) {

            roomCtx->status = 0;
            roomCtx->curRoom.segment = roomCtx->unk_34;
            gSegments[3] = VIRTUAL_TO_PHYSICAL(roomCtx->unk_34);

            Game::Resources::ExecuteSceneCommands(play, static_cast<Game::Resources::Scene*>(roomCtx->roomToLoad));

            Player_ApplyMovementTuning(play);
            return 1;


        return 0;
    }

    return 1;
}

extern "C" int32_t SceneLoader_RequestRoom(PlayState* play, RoomContext* roomCtx, int32_t roomNum) {
    uint32_t size;

    if (roomCtx->status == 0) {
        roomCtx->prevRoom = roomCtx->curRoom;
        roomCtx->curRoom.num = roomNum;
        roomCtx->curRoom.segment = NULL;
        roomCtx->status = 1;

        assert(roomNum < play->numRooms);

        if (roomNum >= play->numRooms)
            return 0; // UH OH

        size = play->roomList[roomNum].vromEnd - play->roomList[roomNum].vromStart;
        roomCtx->unk_34 =
            (void*)ALIGN16((uintptr_t)roomCtx->bufPtrs[roomCtx->unk_30] - ((size + 8) * roomCtx->unk_30 + 7));

        osCreateMesgQueue(&roomCtx->loadQueue, &roomCtx->loadMsg, 1);
        // DmaMgr_SendRequest2(&roomCtx->dmaRequest, roomCtx->unk_34, play->roomList[roomNum].vromStart, size, 0,
        //&roomCtx->loadQueue, NULL, __FILE__, __LINE__);

        auto roomData = std::static_pointer_cast<Game::Resources::Scene>(
            ResourceMgr_GetResourceByNameHandlingMQ(play->roomList[roomNum].fileName));
        roomCtx->status = 1;
        roomCtx->roomToLoad = roomData.get();

        roomCtx->unk_30 ^= 1;

        WriteLog("Room Init - curRoom.num: {0:#x}", roomCtx->curRoom.num);

        return 1;
    }

    return 0;
}
