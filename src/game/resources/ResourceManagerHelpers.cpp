#include <runtime/log/Log.hpp>
#include <engine/resource/ResourceManager.h>
#include "resources/ResourceManagerHelpers.h"
#include "platform/client/RetainedGameBridge.h"
#include "variables.h"
#include "z64.h"
#include "platform/SettingsKeys.h"
#include <resources/GameVersions.h>
#include "resources/type/GameResourceType.h"
#include "resources/type/Array.h"
#include "resources/type/Skeleton.h"
#include "resources/type/PlayerAnimation.h"
#include "resources/type/AudioSoundFont.h"
#include <rendering/interpreter.h>
#include <rendering/resource/ResourceType.h>
#include <rendering/resource/type/DisplayList.h>
#include <limits>

extern "C" PlayState* gPlayState;

static Engine::ResourceManager& Resources() {
    return *RetainedGame_GetResourceManager();
}

static void* LoadResourceData(const char* path) {
    return Resources().GetResourceRawPointer(path);
}

extern "C" uint32_t ResourceMgr_GetNumGameVersions() {
    const size_t versionCount = Resources().GetArchiveManager()->GetGameVersions().size();
    if (versionCount > std::numeric_limits<uint32_t>::max()) {
        Error("Game-version count exceeds the public 32-bit resource API");
        return std::numeric_limits<uint32_t>::max();
    }
    return static_cast<uint32_t>(versionCount);
}

extern "C" uint32_t ResourceMgr_GetGameVersion(int index) {
    return Resources().GetArchiveManager()->GetGameVersions()[index];
}

extern "C" uint32_t ResourceMgr_GetGamePlatform(int index) {
    uint32_t version = Resources().GetArchiveManager()->GetGameVersions()[index];

    switch (version) {
        case OOT_NTSC_US_10:
        case OOT_NTSC_US_11:
        case OOT_NTSC_US_12:
        case OOT_PAL_10:
        case OOT_PAL_11:
            return GAME_PLATFORM_N64;
        case OOT_NTSC_JP_GC:
        case OOT_NTSC_JP_GC_CE:
        case OOT_NTSC_US_GC:
        case OOT_PAL_GC:
        case OOT_NTSC_JP_MQ:
        case OOT_NTSC_US_MQ:
        case OOT_PAL_MQ:
        case OOT_PAL_GC_DBG1:
        case OOT_PAL_GC_DBG2:
        case OOT_PAL_GC_MQ_DBG:
            return GAME_PLATFORM_GC;
    }
}

extern "C" uint32_t ResourceMgr_GetGameRegion(int index) {
    uint32_t version = Resources().GetArchiveManager()->GetGameVersions()[index];

    switch (version) {
        case OOT_NTSC_US_10:
        case OOT_NTSC_US_11:
        case OOT_NTSC_US_12:
        case OOT_NTSC_JP_GC:
        case OOT_NTSC_JP_GC_CE:
        case OOT_NTSC_US_GC:
        case OOT_NTSC_JP_MQ:
        case OOT_NTSC_US_MQ:
            return GAME_REGION_NTSC;
        case OOT_PAL_10:
        case OOT_PAL_11:
        case OOT_PAL_GC:
        case OOT_PAL_MQ:
        case OOT_PAL_GC_DBG1:
        case OOT_PAL_GC_DBG2:
        case OOT_PAL_GC_MQ_DBG:
            return GAME_REGION_PAL;
    }
}

uint32_t IsSceneMasterQuest() {
    return false;
}

extern "C" uint32_t ResourceMgr_IsSceneMasterQuest(int16_t) {
    return IsSceneMasterQuest();
}

extern "C" uint32_t ResourceMgr_IsGameMasterQuest() {
    return gPlayState != NULL ? IsSceneMasterQuest() : 0;
}

extern "C" void ResourceMgr_DirtyDirectory(const char* resName) {
    Resources().DirtyResources(resName);
}

extern "C" void ResourceMgr_UnloadResource(const char* resName) {
    std::string path = resName;
    if (path.substr(0, 7) == "__OTR__") {
        path = path.substr(7);
    }
    auto res = Resources().UnloadResource(path);
}

extern "C" uint8_t ResourceMgr_FileExists(const char* filePath) {
    std::string path = filePath;
    if (path.substr(0, 7) == "__OTR__") {
        path = path.substr(7);
    }

    return Resources().GetArchiveManager()->HasFile(path);
}

std::shared_ptr<Engine::IResource> ResourceMgr_GetResourceByNameHandlingMQ(const char* path) {
    std::string Path = path;
    if (ResourceMgr_IsGameMasterQuest()) {
        size_t pos = 0;
        if ((pos = Path.find("/nonmq/", 0)) != std::string::npos) {
            Path.replace(pos, 7, "/mq/");
        }
    }
    return Resources().LoadResource(Path.c_str());
}

extern "C" char* ResourceMgr_GetResourceDataByNameHandlingMQ(const char* path) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(path);

    if (res == nullptr) {
        return nullptr;
    }

    return (char*)res->GetRawPointer();
}

extern "C" uint8_t ResourceMgr_TexIsRaw(const char* texPath) {
    auto res = std::static_pointer_cast<Engine::Rendering::Texture>(ResourceMgr_GetResourceByNameHandlingMQ(texPath));
    return res->Flags & TEX_FLAG_LOAD_AS_RAW;
}

extern "C" char* ResourceMgr_LoadTexOrDListByName(const char* filePath) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(filePath);

    if (res->GetInitData()->Type == static_cast<uint32_t>(Engine::Rendering::ResourceType::DisplayList)) {
        return (char*)&((std::static_pointer_cast<Engine::Rendering::DisplayList>(res))->Instructions[0]);
    }

    if (res->GetInitData()->Type == static_cast<uint32_t>(Game::Resources::ResourceType::Array)) {
        return (char*)(std::static_pointer_cast<Game::Resources::Array>(res))->Vertices.data();
    }

    return (char*)ResourceMgr_GetResourceDataByNameHandlingMQ(filePath);
}

extern "C" char* ResourceMgr_LoadIfDListByName(const char* filePath) {
    auto res = ResourceMgr_GetResourceByNameHandlingMQ(filePath);

    if (res->GetInitData()->Type == static_cast<uint32_t>(Engine::Rendering::ResourceType::DisplayList)) {
        return (char*)&((std::static_pointer_cast<Engine::Rendering::DisplayList>(res))->Instructions[0]);
    }

    return nullptr;
}

extern "C" char* ResourceMgr_LoadPlayerAnimByName(const char* animPath) {
    auto anim = std::static_pointer_cast<Game::Resources::PlayerAnimation>(ResourceMgr_GetResourceByNameHandlingMQ(animPath));

    return (char*)&anim->limbRotData[0];
}

extern "C" void ResourceMgr_PushCurrentDirectory(char* path) {
    Engine::Rendering::gfx_push_current_dir(path);
}

extern "C" Gfx* ResourceMgr_LoadGfxByName(const char* path) {
    auto res = std::static_pointer_cast<Engine::Rendering::DisplayList>(ResourceMgr_GetResourceByNameHandlingMQ(path));
    return (Gfx*)&res->Instructions[0];
}

extern "C" uint8_t ResourceMgr_FileIsCustomByName(const char* path) {
    auto res = std::static_pointer_cast<Engine::Rendering::DisplayList>(ResourceMgr_GetResourceByNameHandlingMQ(path));
    return res->GetInitData()->IsCustom;
}

typedef struct {
    int index;
    Gfx instruction;
    const void* instructionsPtr;
    size_t instructionCount;
    bool isCustom;
} GfxPatch;

std::unordered_map<std::string, std::unordered_map<std::string, GfxPatch>> originalGfx;

// Attention! This is primarily for cosmetics & bug fixes. For things like mods and model replacement you should be
// using archived resources instead. The index can be found using the commented-out section below.
extern "C" void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction) {
    auto res = std::static_pointer_cast<Engine::Rendering::DisplayList>(Resources().LoadResource(path));

    if (res == nullptr || static_cast<size_t>(index) >= res->Instructions.size()) {
        return;
    }

    // Leaving this here for people attempting to find the correct Dlist index to patch
    /*if (strcmp("__OTR__objects/object_gi_longsword/gGiBiggoronSwordDL", path) == 0) {
        for (int i = 0; i < res->instructions.size(); i++) {
            Gfx* gfx = (Gfx*)&res->instructions[i];
            // Log all commands
            // WriteLog("index:{} command:{}", i, gfx->words.w0 >> 24);
            // Log only SetPrimColors
            if (gfx->words.w0 >> 24 == 250) {
                WriteLog("index:{} r:{} g:{} b:{} a:{}", i, _SHIFTR(gfx->words.w1, 24, 8), _SHIFTR(gfx->words.w1, 16,
    8), _SHIFTR(gfx->words.w1, 8, 8), _SHIFTR(gfx->words.w1, 0, 8));
            }
        }
    }*/

    // Index refers to individual gfx words, which are half the size on 32-bit
    // if (sizeof(uintptr_t) < 8) {
    // index /= 2;
    // }

    // Do not patch custom assets as they most likely do not have the same instructions as authentic assets
    if (res->GetInitData()->IsCustom) {
        return;
    }

    Gfx* gfx = (Gfx*)&res->Instructions[index];

    if (!originalGfx.contains(path) || !originalGfx[path].contains(patchName)) {
        originalGfx[path][patchName] = { index, *gfx, res->Instructions.data(), res->Instructions.size(),
                                         res->GetInitData()->IsCustom };
    }

    *gfx = instruction;
}

extern "C" void ResourceMgr_ReplaceGfxPrimColorByName(const char* path, const char* patchName, uint8_t oldR,
                                                       uint8_t oldG, uint8_t oldB, uint8_t newR, uint8_t newG,
                                                       uint8_t newB) {
    auto res = std::static_pointer_cast<Engine::Rendering::DisplayList>(Resources().LoadResource(path));

    if (res == nullptr || res->GetInitData()->IsCustom) {
        return;
    }

    for (size_t i = 0; i < res->Instructions.size(); i++) {
        Gfx* gfx = reinterpret_cast<Gfx*>(&res->Instructions[i]);

        if ((gfx->words.w0 >> 24) == G_SETPRIMCOLOR && _SHIFTR(gfx->words.w1, 24, 8) == oldR &&
            _SHIFTR(gfx->words.w1, 16, 8) == oldG && _SHIFTR(gfx->words.w1, 8, 8) == oldB) {
            std::string indexedPatchName = std::string(patchName) + "_" + std::to_string(i);

            if (!originalGfx.contains(path) || !originalGfx[path].contains(indexedPatchName)) {
                originalGfx[path][indexedPatchName] = { static_cast<int>(i), *gfx, res->Instructions.data(),
                                                        res->Instructions.size(), false };
            }

            gfx->words.w1 = _SHIFTL(newR, 24, 8) | _SHIFTL(newG, 16, 8) | _SHIFTL(newB, 8, 8) |
                            _SHIFTR(gfx->words.w1, 0, 8);
        }
    }
}

extern "C" void ResourceMgr_PatchGfxCopyCommandByName(const char* path, const char* patchName, int destinationIndex,
                                                      int sourceIndex) {
    auto res = std::static_pointer_cast<Engine::Rendering::DisplayList>(Resources().LoadResource(path));

    if (res == nullptr || static_cast<size_t>(destinationIndex) >= res->Instructions.size() ||
        static_cast<size_t>(sourceIndex) >= res->Instructions.size()) {
        return;
    }

    // Do not patch custom assets as they most likely do not have the same instructions as authentic assets
    if (res->GetInitData()->IsCustom) {
        return;
    }

    Gfx* destinationGfx = (Gfx*)&res->Instructions[destinationIndex];
    Gfx sourceGfx = *(Gfx*)&res->Instructions[sourceIndex];

    if (!originalGfx.contains(path) || !originalGfx[path].contains(patchName)) {
        originalGfx[path][patchName] = { destinationIndex, *destinationGfx, res->Instructions.data(),
                                         res->Instructions.size(), res->GetInitData()->IsCustom };
    }

    *destinationGfx = sourceGfx;
}

extern "C" void ResourceMgr_PatchCustomGfxByName(const char* path, const char* patchName, int index, Gfx instruction) {
    auto res = std::static_pointer_cast<Engine::Rendering::DisplayList>(Resources().LoadResource(path));

    if (res == nullptr || static_cast<size_t>(index) >= res->Instructions.size()) {
        return;
    }

    Gfx* gfx = (Gfx*)&res->Instructions[index];

    if (!originalGfx.contains(path) || !originalGfx[path].contains(patchName)) {
        originalGfx[path][patchName] = { index, *gfx, res->Instructions.data(), res->Instructions.size(),
                                         res->GetInitData()->IsCustom };
    }

    *gfx = instruction;
}

extern "C" void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName) {
    if (originalGfx.contains(path) && originalGfx[path].contains(patchName)) {
        auto res = std::static_pointer_cast<Engine::Rendering::DisplayList>(Resources().LoadResource(path));

        // If the resource is unavailable, clean up the patch record and bail.
        if (res == nullptr) {
            ResourceMgr_UnloadResource(path);
            originalGfx[path].erase(patchName);
            return;
        }

        const GfxPatch& patch = originalGfx[path][patchName];
        // Skip and clean up if the backing resource changed since we recorded the patch (e.g. alt<->vanilla swap)
        // to avoid writing instructions from a different asset onto the current one.
        if (res->Instructions.data() != patch.instructionsPtr || res->Instructions.size() != patch.instructionCount ||
            res->GetInitData()->IsCustom != patch.isCustom) {
            ResourceMgr_UnloadResource(path);
            originalGfx[path].erase(patchName);
            return;
        }

        // Skip and clean up if the loaded resource is smaller than the recorded patch index.
        if (static_cast<size_t>(patch.index) >= res->Instructions.size()) {
            originalGfx[path].erase(patchName);
            return;
        }

        Gfx* gfx = (Gfx*)&res->Instructions[patch.index];
        *gfx = patch.instruction;

        originalGfx[path].erase(patchName);
    }
}

extern "C" char* ResourceMgr_LoadArrayByName(const char* path) {
    auto res = std::static_pointer_cast<Game::Resources::Array>(ResourceMgr_GetResourceByNameHandlingMQ(path));

    return (char*)res->Scalars.data();
}

// Return of LoadArrayByNameAsVec3s must be freed by the caller
extern "C" char* ResourceMgr_LoadArrayByNameAsVec3s(const char* path) {
    auto res = std::static_pointer_cast<Game::Resources::Array>(ResourceMgr_GetResourceByNameHandlingMQ(path));

    // if (res->CachedGameAsset != nullptr)
    //     return (char*)res->CachedGameAsset;
    // else
    // {
    Vec3s* data = (Vec3s*)malloc(sizeof(Vec3s) * res->Scalars.size());

    for (size_t i = 0; i < res->Scalars.size(); i += 3) {
        data[(i / 3)].x = res->Scalars[i + 0].signed16;
        data[(i / 3)].y = res->Scalars[i + 1].signed16;
        data[(i / 3)].z = res->Scalars[i + 2].signed16;
    }

    // res->CachedGameAsset = data;

    return (char*)data;
    // }
}

extern "C" CollisionHeader* ResourceMgr_LoadColByName(const char* path) {
    return (CollisionHeader*)LoadResourceData(path);
}

extern "C" Vtx* ResourceMgr_LoadVtxByName(char* path) {
    return (Vtx*)LoadResourceData(path);
}

extern "C" SequenceData ResourceMgr_LoadSeqByName(const char* path) {
    SequenceData* sequence = (SequenceData*)LoadResourceData(path);
    return *sequence;
}

extern "C" SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path) {
    SequenceData* sequence = (SequenceData*)LoadResourceData(path);
    return sequence;
}

extern "C" SoundFontSample* ResourceMgr_LoadAudioSample(const char* path) {
    return (SoundFontSample*)LoadResourceData(path);
}

extern "C" SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path) {
    return (SoundFont*)LoadResourceData(path);
}

extern "C" Drum* ResourceMgr_LoadAudioDrumSampleByName(const char* path, int32_t drumIndex) {
    if (path == nullptr || drumIndex < 0) {
        return nullptr;
    }

    auto resource = std::dynamic_pointer_cast<Game::Resources::AudioSoundFont>(Resources().LoadResource(path));
    return resource ? reinterpret_cast<Drum*>(resource->ResolveDrumSample((size_t)drumIndex)) : nullptr;
}

extern "C" Instrument* ResourceMgr_LoadAudioInstrumentSamplesByName(const char* path, int32_t instrumentIndex) {
    if (path == nullptr || instrumentIndex < 0) {
        return nullptr;
    }

    auto resource = std::dynamic_pointer_cast<Game::Resources::AudioSoundFont>(Resources().LoadResource(path));
    return resource ? reinterpret_cast<Instrument*>(resource->ResolveInstrumentSamples((size_t)instrumentIndex))
                    : nullptr;
}

extern "C" SoundFontSound* ResourceMgr_LoadAudioSoundEffectSampleByName(const char* path, int32_t soundEffectIndex) {
    if (path == nullptr || soundEffectIndex < 0) {
        return nullptr;
    }

    auto resource = std::dynamic_pointer_cast<Game::Resources::AudioSoundFont>(Resources().LoadResource(path));
    return resource ? reinterpret_cast<SoundFontSound*>(resource->ResolveSoundEffectSample((size_t)soundEffectIndex))
                    : nullptr;
}

extern "C" int ResourceMgr_HasResourceSignature(const char* data) {
    uintptr_t address = (uintptr_t)data;

    if ((address & 1) == 1)
        return 0;

    if (address != 0) {
        if (data[0] == '_' && data[1] == '_' && data[2] == 'O' && data[3] == 'T' && data[4] == 'R' &&
            data[5] == '_' && data[6] == '_') {
            return 1;
        }
    }

    return 0;
}

extern "C" AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path) {
    return (AnimationHeaderCommon*)LoadResourceData(path);
}

extern "C" SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path) {
    std::string pathStr = std::string(path);
    static const std::string kResourceSignature = "__OTR__";

    if (pathStr.starts_with(kResourceSignature)) {
        pathStr = pathStr.substr(kResourceSignature.length());
    }

    return (SkeletonHeader*)LoadResourceData(pathStr.c_str());
}

extern "C" int32_t* ResourceMgr_LoadCSByName(const char* path) {
    return (int32_t*)ResourceMgr_GetResourceDataByNameHandlingMQ(path);
}
