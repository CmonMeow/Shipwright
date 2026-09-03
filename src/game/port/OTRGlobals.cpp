#include <runtime/log/Log.hpp>
#include "OTRGlobals.h"
#include "OTRAudio.h"
#include "cvar_prefixes.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstdint>

#include "ResourceManagerHelpers.h"
#include <fast/Fast3dWindow.h>
#include <engine/resource/File.h>
#include <engine/window/Window.h>
#include <port/GameVersions.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif
#include <engine/audio/AudioPlayer.h>
#include "Enhancements/debugger/colViewer.h"
#include "frame_interpolation.h"
#include "variables.h"
#include "z64.h"
#include "macros.h"
#include <engine/window/FileDropMgr.h>
#include <engine/input/Win32Input.h>
#include "util.h"

#include <fast/interpreter.h>

#ifdef __SWITCH__
#include <port/switch/SwitchImpl.h>
#elif defined(__WIIU__)
#include <port/wiiu/WiiUImpl.h>
#include <coreinit/debug.h> // OSFatal
#endif

#include <functions.h>
#include "ActorDB.h"
#include "ClientRuntime.h"
#include "Network/NetworkVersion.h"
#include "platform/client/PresentationFrameBudget.h"
#include <runtime/runtime.h>
#include <fast/resource/ResourceType.h>

// Resource Types/Factories
#include "port/resource/type/Array.h"
#include <engine/resource/type/Blob.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Matrix.h>
#include <fast/resource/type/Texture.h>
#include <fast/resource/type/Vertex.h>
#include "port/resource/type/SohResourceType.h"
#include "port/resource/type/Animation.h"
#include "port/resource/type/AudioSample.h"
#include "port/resource/type/AudioSequence.h"
#include "port/resource/type/AudioSoundFont.h"
#include "port/resource/type/CollisionHeader.h"
#include "port/resource/type/PlayerAnimation.h"
#include "port/resource/type/Scene.h"
#include "port/resource/type/Skeleton.h"
#include "port/resource/type/SkeletonLimb.h"
#include "port/resource/type/Text.h"
#include <engine/resource/factory/BlobFactory.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/MatrixFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include "port/resource/importer/ArrayFactory.h"
#include "port/resource/importer/AnimationFactory.h"
#include "port/resource/importer/AudioSampleFactory.h"
#include "port/resource/importer/AudioSequenceFactory.h"
#include "port/resource/importer/AudioSoundFontFactory.h"
#include "port/resource/importer/CollisionHeaderFactory.h"
#include "port/resource/importer/PlayerAnimationFactory.h"
#include "port/resource/importer/SceneFactory.h"
#include "port/resource/importer/SkeletonFactory.h"
#include "port/resource/importer/SkeletonLimbFactory.h"
#include "port/resource/importer/TextFactory.h"


bool SoH_HandleConfigDrop(char* filePath);

OTRGlobals* OTRGlobals::Instance;

extern "C" PlayState* gPlayState;

extern "C" void PadMgr_ThreadEntry(PadMgr* padMgr);

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} OTRVersion;

std::shared_ptr<Fast::Fast3dWindow> sohFast3dWindow;
std::string gameArchivePath = "";
static bool gameArchiveVersionMatch = false;

OTRGlobals::OTRGlobals() {
    const std::string windowTitle = "v" + std::to_string(APP_PROTOCOL_VERSION);
    context = Engine::Context::CreateUninitializedInstance(windowTitle, appShortName, "settings.json");

    gameArchivePath = Engine::Context::LocateFileAcrossAppDirs("oot.o2r", appShortName);

    context->InitConfiguration();
    context->InitConsoleVariables();

    context->InitResourceManager({ gameArchivePath }, {}, 3, true);
    context->InitConsole();

    sohFast3dWindow = std::make_shared<Fast::Fast3dWindow>();
    context->InitWindow(sohFast3dWindow);
}

void OTRGlobals::RunExtract() {
    if (std::filesystem::exists(gameArchivePath)) {
        return;
    }

    std::string title;
    std::string message;
    if (std::filesystem::exists(gameArchivePath)) {
        title = "Outdated oot.o2r";
        message = "The required combined oot.o2r archive is incompatible.";
    } else {
        title = "No Ocarina of Time archive";
        message = "The required combined oot.o2r archive is missing.";
    }

#ifdef _WIN32
    MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "%s: %s\n", title.c_str(), message.c_str());
#endif
    std::exit(EXIT_FAILURE);
}
void OTRGlobals::Initialize() {
    std::unordered_set<uint32_t> ValidHashes = {
        OOT_PAL_MQ,     OOT_NTSC_JP_MQ, OOT_NTSC_US_MQ, OOT_PAL_GC_MQ_DBG, OOT_NTSC_US_10,
        OOT_NTSC_US_11, OOT_NTSC_US_12, OOT_PAL_10,     OOT_PAL_11,        OOT_NTSC_JP_GC_CE,
        OOT_NTSC_JP_GC, OOT_NTSC_US_GC, OOT_PAL_GC,     OOT_PAL_GC_DBG1,   OOT_PAL_GC_DBG2,
    };

    context->InitConfiguration();
    context->InitConsoleVariables();

    context->InitGfxDebugger();
    context->InitFileDropMgr();

    // tell LUS to reserve 3 SoH specific threads (Game, Audio, Save)
    context->GetResourceManager()->SetAltAssetsEnabled(false);

    context->InitCrashHandler();

    context->GetWindow()->SetForceCursorVisibility(CVarGetInteger(CVAR_SETTING("CursorVisibility"), 0));

    context->InitAudio({ .SampleRate = 32000, .SampleLength = 1024, .DesiredBuffered = 1680 });

    auto loader = context->GetResourceManager()->GetResourceLoader();
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                    "Vertex", static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                    RESOURCE_FORMAT_BINARY, "DisplayList",
                                    static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                    "Matrix", static_cast<uint32_t>(Fast::ResourceType::Matrix), 0);
    loader->RegisterResourceFactory(std::make_shared<Engine::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                    "Blob", static_cast<uint32_t>(Engine::ResourceType::Blob), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryArrayV0>(), RESOURCE_FORMAT_BINARY,
                                    "Array", static_cast<uint32_t>(SOH::ResourceType::SOH_Array), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAnimationV0>(), RESOURCE_FORMAT_BINARY,
                                    "Animation", static_cast<uint32_t>(SOH::ResourceType::SOH_Animation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPlayerAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "PlayerAnimation",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_PlayerAnimation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySceneV0>(), RESOURCE_FORMAT_BINARY,
                                    "Room", static_cast<uint32_t>(SOH::ResourceType::SOH_Room),
                                    0); // Is room scene? maybe?
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCollisionHeaderV0>(),
                                    RESOURCE_FORMAT_BINARY, "CollisionHeader",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonV0>(), RESOURCE_FORMAT_BINARY,
                                    "Skeleton", static_cast<uint32_t>(SOH::ResourceType::SOH_Skeleton), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonLimbV0>(),
                                    RESOURCE_FORMAT_BINARY, "SkeletonLimb",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryTextV0>(), RESOURCE_FORMAT_BINARY,
                                    "Text", static_cast<uint32_t>(SOH::ResourceType::SOH_Text), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSampleV2>(), RESOURCE_FORMAT_BINARY,
                                    "AudioSample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSoundFontV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSoundFont",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSequenceV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSequence",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 2);
    auto versions = context->GetResourceManager()->GetArchiveManager()->GetGameVersions();

    for (uint32_t version : versions) {
        if (!ValidHashes.contains(version)) {
#if defined(__SWITCH__)
            WriteLog("Invalid OTR File!");
#elif defined(__WIIU__)
            Engine::WiiU::ThrowInvalidOTR();
#elif defined(_WIN32)
            MessageBoxA(nullptr, "Attempted to load an invalid OTR file. Try regenerating.", "Invalid OTR File",
                        MB_OK | MB_ICONERROR);
#else
            std::fprintf(stderr, "Invalid OTR File: Attempted to load an invalid OTR file. Try regenerating.\n");
            WriteLog("Invalid OTR File!");
#endif
            exit(1);
        }
    }
}

OTRGlobals::~OTRGlobals() {
}

uint32_t OTRGlobals::GetInterpolationFPS() {
    // Every interpolated frame is presented separately. With vertical sync
    // enabled, presenting fewer interpolation frames than the monitor refresh
    // rate shortens the interval between 20 Hz game updates. For example, a
    // saved 40 FPS target on a 60 Hz display presents two frames in 1/30 s and
    // makes gameplay run at 30 Hz. Use the real refresh rate whenever vsync
    // owns presentation timing so rendering frequency cannot change game time.
    if (CVarGetInteger(CVAR_SETTING("MatchRefreshRate"), 0) ||
        CVarGetInteger(CVAR_VSYNC_ENABLED, 1)) {
        return Engine::Context::GetInstance()->GetWindow()->GetCurrentRefreshRate();
    } else if (!Engine::Context::GetInstance()->GetWindow()->CanDisableVerticalSync()) {
        return Engine::Context::GetInstance()->GetWindow()->GetCurrentRefreshRate();
    }
    return CVarGetInteger(CVAR_SETTING("InterpolationFPS"), 20);
}

extern "C" void AudioMgr_CreateNextAudioBuffer(int16_t* samples, uint32_t num_samples);
extern "C" void AudioPlayer_Play(const uint8_t* buf, uint32_t len);
extern "C" int AudioPlayer_Buffered(void);
extern "C" int AudioPlayer_GetDesiredBuffered(void);
void OTRAudio_Thread() {
    while (audio.running) {
        {
            std::unique_lock<std::mutex> Lock(audio.mutex);
            while (!audio.processing && audio.running) {
                audio.cv_to_thread.wait(Lock);
            }

            if (!audio.running) {
                break;
            }
        }
        std::unique_lock<std::mutex> Lock(audio.mutex);
// AudioMgr_ThreadEntry(&gAudioMgr);
//  528 and 544 relate to 60 fps at 32 kHz 32000/60 = 533.333..
//  in an ideal world, one third of the calls should use num_samples=544 and two thirds num_samples=528
#define SAMPLES_HIGH 560
#define SAMPLES_LOW 528

#define AUDIO_FRAMES_PER_UPDATE (R_UPDATE_RATE > 0 ? R_UPDATE_RATE : 1)
#define NUM_AUDIO_CHANNELS 2

        int samples_left = AudioPlayer_Buffered();
        uint32_t num_audio_samples = samples_left < AudioPlayer_GetDesiredBuffered() ? SAMPLES_HIGH : SAMPLES_LOW;

        // 3 is the maximum authentic frame divisor.
        int16_t audio_buffer[SAMPLES_HIGH * NUM_AUDIO_CHANNELS * 3];
        for (int i = 0; i < AUDIO_FRAMES_PER_UPDATE; i++) {
            AudioMgr_CreateNextAudioBuffer(audio_buffer + i * (num_audio_samples * NUM_AUDIO_CHANNELS),
                                           num_audio_samples);
        }

        AudioPlayer_Play((uint8_t*)audio_buffer,
                         num_audio_samples * (sizeof(int16_t) * NUM_AUDIO_CHANNELS * AUDIO_FRAMES_PER_UPDATE));

        audio.processing = false;
        audio.cv_from_thread.notify_one();
    }
}

// C->C++ Bridge
extern "C" void OTRAudio_Init() {
    if (!audio.running) {
        audio.running = true;
        audio.thread = std::thread(OTRAudio_Thread);
    }
}

extern "C" char** sequenceMap;
extern "C" size_t sequenceMapSize;

extern "C" char** fontMap;
extern "C" size_t fontMapSize;

extern "C" void OTRAudio_Exit() {
    // Tell the audio thread to stop
    {
        std::unique_lock<std::mutex> Lock(audio.mutex);
        audio.running = false;
    }
    audio.cv_to_thread.notify_all();

    // Wait until the audio thread quit
    audio.thread.join();
}

extern "C" void Messagebox_ShowErrorBox(char* title, char* body) {
#ifdef _WIN32
    MessageBoxA(nullptr, body, title, MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "%s: %s\n", title, body);
#endif
}

extern "C" void InitOTR() {
    OTRGlobals::Instance = new OTRGlobals();
    OTRGlobals::Instance->RunExtract();

    OTRGlobals::Instance->Initialize();
    InitColViewer();
    ActorDB::Instance = new ActorDB();
    OTRAudio_Init();
    ClientRuntime_RegisterActors();
    ClientRuntime_Initialize();
    Engine::Context::GetInstance()->GetFileDropMgr()->RegisterDropHandler(SoH_HandleConfigDrop);

    const uint64_t seedTime = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    std::srand(static_cast<unsigned int>(seedTime ^ (seedTime >> 32)));
}

extern "C" void DeinitOTR() {
    ClientRuntime_Shutdown();
    OTRAudio_Exit();

    sohFast3dWindow = nullptr;

    OTRGlobals::Instance->context = nullptr;
}

#ifdef _WIN32
extern "C" uint64_t GetFrequency() {
    LARGE_INTEGER nFreq;

    QueryPerformanceFrequency(&nFreq);

    return nFreq.QuadPart;
}

extern "C" uint64_t GetPerfCounter() {
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);

    return ticks.QuadPart;
}
#else
extern "C" uint64_t GetFrequency() {
    return 1000; // sec -> ms
}

extern "C" uint64_t GetPerfCounter() {
    struct timespec monotime;
    clock_gettime(CLOCK_MONOTONIC, &monotime);

    uint64_t remainingMs = (monotime.tv_nsec / 1000000);

    // in milliseconds
    return monotime.tv_sec * 1000 + remainingMs;
}
#endif

extern "C" uint64_t GetUnixTimestamp() {
    auto time = std::chrono::system_clock::now();
    auto since_epoch = time.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
    return (uint64_t)millis.count();
}

extern "C" void Graph_StartFrame() {
    // Transport must continue through title/file-select screens, scene loads,
    // and gameplay pauses. PlayState-specific synchronization runs later.
    ClientRuntime_UpdateTransport();
#ifdef _WIN32
    if (Engine::GetWin32Input().ConsumePress(VK_F1)) {
        ToggleColViewer();
    }
#endif
}

void RunCommands(Gfx* Commands, const std::vector<std::unordered_map<Mtx*, MtxF>>& mtx_replacements,
                 int presentationFps, int simulationFps) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(OTRGlobals::Instance->context->GetWindow());

    if (wnd == nullptr) {
        return;
    }

    // Process window events for resize, mouse, keyboard events
    wnd->HandleEvents();

    auto intp = wnd->GetInterpreterWeak().lock().get();
    intp->mInterpolationIndex = 0;

    // Native gameplay advances at a fixed rate. Interpolated presents are only
    // visual samples between two gameplay states and must never postpone the
    // next simulation tick. If this machine cannot draw every requested sample
    // inside one simulation interval, discard intermediate samples and always
    // present the newest native state.
    using Clock = std::chrono::steady_clock;
    static Game::Client::PresentationFrameBudget frameBudget;
    frameBudget.BeginBatch(presentationFps, simulationFps);
    const auto batchStart = Clock::now();

    for (size_t index = 0; index < mtx_replacements.size(); ++index) {
        const bool newestNativeState = index + 1 == mtx_replacements.size();
        if (!newestNativeState) {
            const double elapsedSeconds =
                std::chrono::duration<double>(Clock::now() - batchStart).count();
            if (!frameBudget.CanPresentIntermediate(elapsedSeconds)) {
                continue;
            }
        }

        const auto presentStart = Clock::now();
        const auto& m = mtx_replacements[index];
        intp->mInterpolationIndex = static_cast<int>(index);
        wnd->DrawAndRunGraphicsCommands(Commands, m);
        const double presentSeconds =
            std::chrono::duration<double>(Clock::now() - presentStart).count();
        // React quickly when rendering becomes expensive, then recover
        // gradually so a transient fast frame cannot cause another stall.
        frameBudget.ObservePresent(presentSeconds);
    }
}

// C->C++ Bridge
extern "C" void Graph_ProcessGfxCommands(Gfx* commands) {
    {
        std::unique_lock<std::mutex> Lock(audio.mutex);
        audio.processing = true;
    }

    audio.cv_to_thread.notify_one();
    std::vector<std::unordered_map<Mtx*, MtxF>> mtx_replacements;
    int target_fps = OTRGlobals::Instance->GetInterpolationFPS();
    static int last_fps;
    static int last_update_rate;
    static int time;
    int fps = target_fps;
    int original_fps = 60 / R_UPDATE_RATE;
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow());

    if (target_fps == 20 || original_fps > target_fps) {
        fps = original_fps;
    }

    if (last_fps != fps || last_update_rate != R_UPDATE_RATE) {
        time = 0;
    }

    // time_base = fps * original_fps (one second)
    int next_original_frame = fps;

    while (time + original_fps <= next_original_frame) {
        time += original_fps;
        if (time != next_original_frame) {
            mtx_replacements.push_back(FrameInterpolation_Interpolate((float)time / next_original_frame));
        } else {
            mtx_replacements.emplace_back();
        }
    }

    time -= fps;

    if (wnd != nullptr) {
        wnd->SetTargetFps(fps);
    }

    // When the gfx debugger is active, only run with the final mtx
    if (GfxDebuggerIsDebugging()) {
        mtx_replacements.clear();
        mtx_replacements.emplace_back();
    }

    RunCommands(commands, mtx_replacements, fps, original_fps);

    last_fps = fps;
    last_update_rate = R_UPDATE_RATE;

    {
        std::unique_lock<std::mutex> Lock(audio.mutex);
        while (audio.processing) {
            audio.cv_from_thread.wait(Lock);
        }
    }

    // OTRTODO: FIGURE OUT END FRAME POINT
    /* if (OTRGlobals::Instance->context->lastScancode != -1)
         OTRGlobals::Instance->context->lastScancode = -1;*/
}

float divisor_num = 0.0f;

extern "C" void OTRGetPixelDepthPrepare(float x, float y) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow());
    if (wnd == nullptr) {
        return;
    }

    wnd->GetPixelDepthPrepare(x, y);
}

extern "C" uint16_t OTRGetPixelDepth(float x, float y) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow());
    if (wnd == nullptr) {
        return 0;
    }

    return wnd->GetPixelDepth(x, y);
}

extern "C" uint32_t OTRGetCurrentWidth() {
    return OTRGlobals::Instance->context->GetWindow()->GetWidth();
}

extern "C" uint32_t OTRGetCurrentHeight() {
    return OTRGlobals::Instance->context->GetWindow()->GetHeight();
}

extern "C" void OTRControllerCallback(uint8_t) {
}

extern "C" float OTRGetAspectRatio() {
    return Engine::Context::GetInstance()->GetWindow()->GetAspectRatio();
}

extern "C" float OTRGetDimensionFromLeftEdge(float v) {
    return (SCREEN_WIDTH / 2 - SCREEN_HEIGHT / 2 * OTRGetAspectRatio() + (v));
}

extern "C" float OTRGetDimensionFromRightEdge(float v) {
    return (SCREEN_WIDTH / 2 + SCREEN_HEIGHT / 2 * OTRGetAspectRatio() - (SCREEN_WIDTH - v));
}

// Gets the width of the current render target area
extern "C" uint32_t OTRGetGameRenderWidth() {
    auto fastWnd = dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow());
    auto intP = fastWnd->GetInterpreterWeak().lock();

    if (!intP) {
        assert(false && "Lost reference to Fast::Interpreter");
        return 320;
    }

    uint32_t height, width;
    intP->GetCurDimensions(&width, &height);

    return width;
}

// Gets the height of the current render target area
extern "C" uint32_t OTRGetGameRenderHeight() {
    auto fastWnd = dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow());
    auto intP = fastWnd->GetInterpreterWeak().lock();

    if (!intP) {
        assert(false && "Lost reference to Fast::Interpreter");
        return 240;
    }

    uint32_t height, width;
    intP->GetCurDimensions(&width, &height);

    return height;
}

float floorf(float x);
float ceilf(float x);  // This gets annoying

extern "C" int16_t OTRGetRectDimensionFromLeftEdge(float v) {
    return ((int)floorf(OTRGetDimensionFromLeftEdge(v)));
}

extern "C" int16_t OTRGetRectDimensionFromRightEdge(float v) {
    return ((int)ceilf(OTRGetDimensionFromRightEdge(v)));
}

extern "C" int AudioPlayer_Buffered(void) {
    return AudioPlayerBuffered();
}

extern "C" int AudioPlayer_GetDesiredBuffered(void) {
    return AudioPlayerGetDesiredBuffered();
}

extern "C" void AudioPlayer_Play(const uint8_t* buf, uint32_t len) {
    AudioPlayerPlayFrame(buf, len);
}

extern "C" int Controller_ShouldRumble(size_t) {
    return 0;
}

extern "C" void Gfx_RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement) {
    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow())
                        ->GetInterpreterWeak()
                        .lock()) {
        intP->RegisterBlendedTexture(name, mask, replacement);
    } else {
        assert(false && "Lost reference to Fast::Interpreter");
    }
}

extern "C" void Gfx_UnregisterBlendedTexture(const char* name) {
    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow())
                        ->GetInterpreterWeak()
                        .lock()) {
        intP->UnregisterBlendedTexture(name);
    } else {
        assert(false && "Lost reference to Fast::Interpreter");
    }
}

extern "C" void Gfx_TextureCacheDelete(const uint8_t* texAddr) {
    char* imgName = (char*)texAddr;

    if (texAddr == nullptr) {
        return;
    }

    if (ResourceMgr_OTRSigCheck(imgName)) {
        texAddr = (const uint8_t*)ResourceMgr_GetResourceDataByNameHandlingMQ(imgName);
    }

    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Engine::Context::GetInstance()->GetWindow())
                        ->GetInterpreterWeak()
                        .lock()) {
        intP->TextureCacheDelete(texAddr);
    } else {
        assert(false && "Lost reference to Fast::Interpreter");
    }
}

bool SoH_HandleConfigDrop(char* filePath) {
    if (SohUtils::IsStringEmpty(filePath)) {
        return false;
    }
    try {
        std::ifstream configStream(filePath);
        if (!configStream) {
            return false;
        }

        nlohmann::json configJson;
        configStream >> configJson;

        if (!configJson.contains("CVars")) {
            return false;
        }

        // Flatten everything under CVars into a single array
        auto cvars = configJson["CVars"].flatten();

        for (auto& [key, value] : cvars.items()) {
            // Replace slashes with dots in key, and remove leading dot
            std::string path = key;
            std::replace(path.begin(), path.end(), '/', '.');
            if (path[0] == '.') {
                path.erase(0, 1);
            }
            if (value.is_string()) {
                CVarSetString(path.c_str(), value.get<std::string>().c_str());
            } else if (value.is_number_integer()) {
                CVarSetInteger(path.c_str(), value.get<int>());
            } else if (value.is_number_float()) {
                CVarSetFloat(path.c_str(), value.get<float>());
            }
        }

        Engine::Context::GetInstance()->GetConsoleVariables()->Save();
        uint32_t finalHash = SohUtils::Hash(configJson.dump());
        WriteLog("Configuration loaded. Hash: {}", finalHash);
        return true;
    } catch (std::exception& e) {
        WriteLog("Failed to load config file: {}", e.what());
        return false;
    } catch (...) {
        WriteLog("Failed to load config file");
        return false;
    }
    return false;
}

extern "C" uint32_t Interpolation_GetFPS() {
    return OTRGlobals::Instance->GetInterpolationFPS();
}

// Number of interpolated frames
extern "C" uint32_t Interpolation_GetFrameCount() {
    const uint32_t framesPerSecond = Interpolation_GetFPS();
    const uint32_t simulationFramesPerSecond =
        R_UPDATE_RATE > 0 ? 60U / static_cast<uint32_t>(R_UPDATE_RATE) : 20U;
    return Game::Client::PresentationFrameBudget::FrameCount(
        framesPerSecond, simulationFramesPerSecond);
}
