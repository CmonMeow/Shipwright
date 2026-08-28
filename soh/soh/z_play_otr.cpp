#include <libultraship/log/PathEngineLog.hpp>
#include <libultraship/libultraship.h>
#include "soh/resource/type/Scene.h"
#include "global.h"
#include "vt.h"

extern "C" void Play_InitEnvironment(PlayState* play, int16_t skyboxId);
void OTRPlay_InitScene(PlayState* play, int32_t spawn);
int32_t OTRScene_ExecuteCommands(PlayState* play, SOH::Scene* scene);

// LUS::OTRResource* OTRPlay_LoadFile(PlayState* play, RomFile* file) {
Ship::IResource* OTRPlay_LoadFile(const char* fileName) {
    auto res = Ship::Context::GetInstance()->GetResourceManager()->LoadResource(fileName);
    return res.get();
}

extern "C" void OTRPlay_SpawnScene(PlayState* play, int32_t sceneId, int32_t spawn) {
    SceneTableEntry* scene = &gSceneTable[sceneId];

    scene->unk_13 = 0;
    play->loadedScene = scene;
    play->sceneNum = sceneId;
    play->sceneConfig = scene->config;

    // osSyncPrintf("\nSCENE SIZE %fK\n", (scene->sceneFile.vromEnd - scene->sceneFile.vromStart) / 1024.0f);

    const std::string scenePath = "scenes/shared/test01_scene/test01_scene";

    play->sceneSegment = OTRPlay_LoadFile(scenePath.c_str());

    // The reduced runtime has no fallback scene.
    if (play->sceneSegment == nullptr) {
        PathEngineLog("Unable to load required test01 scene: {}", scenePath);
        play->state.running = false;
        return;
    }

    scene->unk_13 = 0;

    // gSegments[2] = VIRTUAL_TO_PHYSICAL(play->sceneSegment);

    OTRPlay_InitScene(play, spawn);
    auto roomSize = func_80096FE8(play, &play->roomCtx);

    osSyncPrintf("ROOM SIZE=%fK\n", roomSize / 1024.0f);

    PathEngineLog("Scene Init - sceneNum: {0:#x}, entranceIndex: {1:#x}", play->sceneNum, gSaveContext.entranceIndex);
}

void OTRPlay_InitScene(PlayState* play, int32_t spawn) {
    play->curSpawn = spawn;
    play->linkActorEntry = nullptr;
    play->unk_11DFC = nullptr;
    play->setupEntranceList = nullptr;
    Object_InitBank(play, &play->objectCtx);
    LightContext_Init(play, &play->lightCtx);
    func_80096FD4(play, &play->roomCtx.curRoom);
    YREG(15) = 0;
    gSaveContext.worldMapArea = 0;
    OTRScene_ExecuteCommands(play, (SOH::Scene*)play->sceneSegment);

    Play_InitEnvironment(play, play->skyboxId);
}
