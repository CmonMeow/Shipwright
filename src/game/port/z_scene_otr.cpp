#include <runtime/log/Log.hpp>
#include "ResourceManagerHelpers.h"
#include <runtime/runtime.h>
#include "port/resource/type/Scene.h"
#include <engine/utils/StringHelper.h>
#include "global.h"
#include "vt.h"
#include "port/resource/type/CollisionHeader.h"
#include <fast/resource/type/DisplayList.h>
#include <engine/resource/type/Blob.h>
#include <memory>
#include <cassert>
#include "port/resource/type/scenecommand/SetCameraSettings.h"
#include "port/resource/type/scenecommand/SetStartPositionList.h"
#include "port/resource/type/scenecommand/SetCollisionHeader.h"
#include "port/resource/type/scenecommand/SetRoomList.h"
#include "port/resource/type/scenecommand/SetEntranceList.h"
#include "port/resource/type/scenecommand/SetSpecialObjects.h"
#include "port/resource/type/scenecommand/SetRoomBehavior.h"
#include "port/resource/type/scenecommand/SetMesh.h"
#include "port/resource/type/scenecommand/SetSkyboxSettings.h"
#include "port/resource/type/scenecommand/SetSkyboxModifier.h"
#include "port/resource/type/scenecommand/SetTimeSettings.h"
#include "port/resource/type/scenecommand/SetSoundSettings.h"
#include "port/resource/type/scenecommand/SetEchoSettings.h"

extern Engine::IResource* OTRPlay_LoadFile(const char* fileName);
extern "C" int32_t Object_Spawn(ObjectContext* objectCtx, int16_t objectId);
int32_t OTRScene_ExecuteCommands(PlayState* play, SOH::Scene* scene);

bool Scene_CommandSpawnList(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetStartPositionList* cmdStartPos = std::static_pointer_cast<SOH::SetStartPositionList>(cmd);
    SOH::SetStartPositionList* cmdStartPos = (SOH::SetStartPositionList*)cmd;
    ActorEntry* entries = (ActorEntry*)cmdStartPos->GetRawPointer();

    play->linkActorEntry = &entries[play->setupEntranceList[play->curSpawn].spawn];
    play->linkAgeOnLoad = LINK_AGE_ADULT;
    Object_Spawn(&play->objectCtx, OBJECT_LINK_BOY);

    return false;
}



bool Scene_CommandCollisionHeader(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetCollisionHeader* cmdCol = std::static_pointer_cast<SOH::SetCollisionHeader>(cmd);
    SOH::SetCollisionHeader* cmdCol = (SOH::SetCollisionHeader*)cmd;
    BgCheck_Allocate(&play->colCtx, play, (CollisionHeader*)cmdCol->GetRawPointer());

    return false;
}

bool Scene_CommandRoomList(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetRoomList* cmdRoomList = std::static_pointer_cast<SOH::SetRoomList>(cmd);
    SOH::SetRoomList* cmdRoomList = (SOH::SetRoomList*)cmd;

    play->numRooms = cmdRoomList->numRooms;
    play->roomList = (RomFile*)cmdRoomList->GetRawPointer();

    return false;
}

bool Scene_CommandEntranceList(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetEntranceList* otrEntrance = std::static_pointer_cast<SOH::SetEntranceList>(cmd);
    SOH::SetEntranceList* otrEntrance = (SOH::SetEntranceList*)cmd;
    play->setupEntranceList = (EntranceEntry*)otrEntrance->GetRawPointer();

    return false;
}

bool Scene_CommandSpecialFiles(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetSpecialObjects* specialCmd = std::static_pointer_cast<SOH::SetSpecialObjects>(cmd);
    SOH::SetSpecialObjects* specialCmd = (SOH::SetSpecialObjects*)cmd;

    if (specialCmd->specialObjects.globalObject != 0) {
        play->objectCtx.subKeepIndex = Object_Spawn(&play->objectCtx, specialCmd->specialObjects.globalObject);
    }

    return false;
}

bool Scene_CommandRoomBehavior(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetRoomBehavior* cmdRoom = std::static_pointer_cast<SOH::SetRoomBehavior>(cmd);
    SOH::SetRoomBehavior* cmdRoom = (SOH::SetRoomBehavior*)cmd;

    play->roomCtx.curRoom.behaviorType1 = cmdRoom->roomBehavior.gameplayFlags;
    play->roomCtx.curRoom.behaviorType2 = cmdRoom->roomBehavior.gameplayFlags2 & 0xFF;
    play->roomCtx.curRoom.lensMode = (cmdRoom->roomBehavior.gameplayFlags2 >> 8) & 1;
    play->msgCtx.disableWarpSongs = (cmdRoom->roomBehavior.gameplayFlags2 >> 0xA) & 1;

    return false;
}

bool Scene_CommandMeshHeader(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetMesh* otrMesh = static_pointer_cast<SOH::SetMesh>(cmd);
    SOH::SetMesh* otrMesh = (SOH::SetMesh*)cmd;
    play->roomCtx.curRoom.meshHeader = (MeshHeader*)otrMesh->GetRawPointer();

    return false;
}

bool Scene_CommandLightSettingsList(PlayState* play, SOH::ISceneCommand* cmd) {
    play->envCtx.lightSettingsList = (EnvLightSettings*)cmd->GetRawPointer();

    return false;
}

// Scene Command 0x11: Skybox Settings
bool Scene_CommandSkyboxSettings(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetSkyboxSettings* cmdSky = static_pointer_cast<SOH::SetSkyboxSettings>(cmd);
    SOH::SetSkyboxSettings* cmdSky = (SOH::SetSkyboxSettings*)cmd;

    play->skyboxId = cmdSky->settings.skyboxId;
    play->envCtx.unk_17 = play->envCtx.unk_18 = cmdSky->settings.weather;
    play->envCtx.indoors = cmdSky->settings.indoors;

    return false;
}

bool Scene_CommandSkyboxDisables(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetSkyboxModifier* cmdSky = static_pointer_cast<SOH::SetSkyboxModifier>(cmd);
    SOH::SetSkyboxModifier* cmdSky = (SOH::SetSkyboxModifier*)cmd;

    play->envCtx.sunMoonDisabled = cmdSky->modifier.sunMoonDisabled;
    play->envCtx.skyboxDisabled = cmdSky->modifier.skyboxDisabled;

    return false;
}

bool Scene_CommandTimeSettings(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetTimeSettings* cmdTime = static_pointer_cast<SOH::SetTimeSettings>(cmd);
    SOH::SetTimeSettings* cmdTime = (SOH::SetTimeSettings*)cmd;

    if ((cmdTime->settings.hour != 0xFF) && (cmdTime->settings.minute != 0xFF)) {
        gSaveContext.skyboxTime = gSaveContext.dayTime =
            ((cmdTime->settings.hour + (cmdTime->settings.minute / 60.0f)) * 60.0f) / ((float)(24 * 60) / 0x10000);
    }

    if (cmdTime->settings.timeIncrement != 0xFF) {
        play->envCtx.timeIncrement = cmdTime->settings.timeIncrement;
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




bool Scene_CommandSoundSettings(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetSoundSettings* cmdSnd = static_pointer_cast<SOH::SetSoundSettings>(cmd);
    SOH::SetSoundSettings* cmdSnd = (SOH::SetSoundSettings*)cmd;

    play->sequenceCtx.seqId = cmdSnd->settings.seqId;
    play->sequenceCtx.natureAmbienceId = cmdSnd->settings.natureAmbienceId;

    if (gSaveContext.seqId == 0xFF) {
        Audio_QueueSeqCmd(cmdSnd->settings.reverb | 0xF0000000);
    }

    return false;
}

bool Scene_CommandEchoSettings(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetEchoSettings* cmdEcho = static_pointer_cast<SOH::SetEchoSettings>(cmd);
    SOH::SetEchoSettings* cmdEcho = (SOH::SetEchoSettings*)cmd;

    play->roomCtx.curRoom.echo = cmdEcho->settings.echo;

    return false;
}



// Camera & World Map Area
bool Scene_CommandMiscSettings(PlayState* play, SOH::ISceneCommand* cmd) {
    // SOH::SetCameraSettings* cmdCam = std::static_pointer_cast<SOH::SetCameraSettings>(cmd);
    SOH::SetCameraSettings* cmdCam = (SOH::SetCameraSettings*)cmd;

    YREG(15) = cmdCam->settings.cameraMovement;
    gSaveContext.worldMapArea = cmdCam->settings.worldMapArea;

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

int32_t OTRScene_ExecuteCommands(PlayState* play, SOH::Scene* scene) {
    for (const auto& sceneCmd : scene->commands) {
        if (sceneCmd == nullptr) {
            continue;
        }

        switch (sceneCmd->cmdId) {
            case SOH::SceneCommandID::SetStartPositionList:
                Scene_CommandSpawnList(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetCollisionHeader:
                Scene_CommandCollisionHeader(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetRoomList:
                Scene_CommandRoomList(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetEntranceList:
                Scene_CommandEntranceList(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetSpecialObjects:
                Scene_CommandSpecialFiles(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetRoomBehavior:
                Scene_CommandRoomBehavior(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetMesh:
                Scene_CommandMeshHeader(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetLightingSettings:
                Scene_CommandLightSettingsList(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetTimeSettings:
                Scene_CommandTimeSettings(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetSkyboxSettings:
                Scene_CommandSkyboxSettings(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetSkyboxModifier:
                Scene_CommandSkyboxDisables(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetSoundSettings:
                Scene_CommandSoundSettings(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetEchoSettings:
                Scene_CommandEchoSettings(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::SetCameraSettings:
                Scene_CommandMiscSettings(play, sceneCmd.get());
                break;
            case SOH::SceneCommandID::EndMarker:
                return 0;
            default:
                WriteLog("Rejected unsupported test01 scene command {}", static_cast<uint32_t>(sceneCmd->cmdId));
                return -1;
        }
    }
    return 0;
}

extern "C" int32_t OTRfunc_800973FC(PlayState* play, RoomContext* roomCtx) {
    if (roomCtx->status == 1) {
        // if (!osRecvMesg(&roomCtx->loadQueue, NULL, OS_MESG_NOBLOCK)) {

            roomCtx->status = 0;
            roomCtx->curRoom.segment = roomCtx->unk_34;
            gSegments[3] = VIRTUAL_TO_PHYSICAL(roomCtx->unk_34);

            OTRScene_ExecuteCommands(play, (SOH::Scene*)roomCtx->roomToLoad);

            Player_SetBootData(play, GET_PLAYER(play));
            return 1;


        return 0;
    }

    return 1;
}

extern "C" int32_t OTRfunc_8009728C(PlayState* play, RoomContext* roomCtx, int32_t roomNum) {
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

        auto roomData = std::static_pointer_cast<SOH::Scene>(
            ResourceMgr_GetResourceByNameHandlingMQ(play->roomList[roomNum].fileName));
        roomCtx->status = 1;
        roomCtx->roomToLoad = roomData.get();

        roomCtx->unk_30 ^= 1;

        WriteLog("Room Init - curRoom.num: {0:#x}", roomCtx->curRoom.num);

        return 1;
    }

    return 0;
}
