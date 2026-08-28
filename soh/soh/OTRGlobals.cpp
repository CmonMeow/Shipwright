#include <libultraship/log/PathEngineLog.hpp>
#include "OTRGlobals.h"
#include "OTRAudio.h"
#include "cvar_prefixes.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include <chrono>

#include "ResourceManagerHelpers.h"
#include <fast/Fast3dWindow.h>
#include <ship/resource/File.h>
#include <ship/window/Window.h>
#include <soh/GameVersions.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif
#include <ship/audio/AudioPlayer.h>
#include "Enhancements/debugger/colViewer.h"
#include "frame_interpolation.h"
#include "variables.h"
#include "z64.h"
#include "macros.h"
#include <ship/window/FileDropMgr.h>
#include <ship/input/Win32Input.h>
#include <ship/utils/StringHelper.h>
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
#include "NetworkGameBridge.h"
#include <libultraship/libultraship.h>
#include <fast/resource/ResourceType.h>

// Resource Types/Factories
#include "soh/resource/type/Array.h"
#include <ship/resource/type/Blob.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Matrix.h>
#include <fast/resource/type/Texture.h>
#include <fast/resource/type/Vertex.h>
#include "soh/resource/type/SohResourceType.h"
#include "soh/resource/type/Animation.h"
#include "soh/resource/type/AudioSample.h"
#include "soh/resource/type/AudioSequence.h"
#include "soh/resource/type/AudioSoundFont.h"
#include "soh/resource/type/CollisionHeader.h"
#include "soh/resource/type/PlayerAnimation.h"
#include "soh/resource/type/Scene.h"
#include "soh/resource/type/Skeleton.h"
#include "soh/resource/type/SkeletonLimb.h"
#include "soh/resource/type/Text.h"
#include <ship/resource/factory/BlobFactory.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/MatrixFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include "soh/resource/importer/ArrayFactory.h"
#include "soh/resource/importer/AnimationFactory.h"
#include "soh/resource/importer/AudioSampleFactory.h"
#include "soh/resource/importer/AudioSequenceFactory.h"
#include "soh/resource/importer/AudioSoundFontFactory.h"
#include "soh/resource/importer/CollisionHeaderFactory.h"
#include "soh/resource/importer/PlayerAnimationFactory.h"
#include "soh/resource/importer/SceneFactory.h"
#include "soh/resource/importer/SkeletonFactory.h"
#include "soh/resource/importer/SkeletonLimbFactory.h"
#include "soh/resource/importer/TextFactory.h"


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
static OTRVersion DetectOTRVersion(std::string path, bool isMq);
static bool VerifyArchiveVersion(OTRVersion version);
std::string gameArchivePath = "";
static bool gameArchiveVersionMatch = false;

OTRGlobals::OTRGlobals() {
    context = Ship::Context::CreateUninitializedInstance("Ship of Harkinian", appShortName, "shipofharkinian.json");

    gameArchivePath = Ship::Context::LocateFileAcrossAppDirs("oot.o2r", appShortName);
    OTRVersion gameArchiveVersion = DetectOTRVersion("oot.o2r", false);
    gameArchiveVersionMatch = gameArchiveVersion.major == gBuildVersionMajor &&
                              gameArchiveVersion.minor == gBuildVersionMinor &&
                              gameArchiveVersion.patch == gBuildVersionPatch;

    context->InitConfiguration();
    context->InitConsoleVariables();

    context->InitResourceManager({ gameArchivePath }, {}, 3, true);
    context->InitConsole();

    sohFast3dWindow = std::make_shared<Fast::Fast3dWindow>();
    context->InitWindow(sohFast3dWindow);
}

void OTRGlobals::RunExtract(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (gameArchiveVersionMatch && std::filesystem::exists(gameArchivePath)) {
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

    PathEngineLog("Starting Ship of Harkinian version {} (Branch: {} | Commit: {})", (char*)gBuildVersion,
                (char*)gGitBranch, (char*)gGitCommitHash);

    auto loader = context->GetResourceManager()->GetResourceLoader();
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                    "Vertex", static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLVertexV0>(), RESOURCE_FORMAT_XML, "Vertex",
                                    static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                    RESOURCE_FORMAT_BINARY, "DisplayList",
                                    static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLDisplayListV0>(), RESOURCE_FORMAT_XML,
                                    "DisplayList", static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                    "Matrix", static_cast<uint32_t>(Fast::ResourceType::Matrix), 0);
    loader->RegisterResourceFactory(std::make_shared<Ship::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                    "Blob", static_cast<uint32_t>(Ship::ResourceType::Blob), 0);
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
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSceneV0>(), RESOURCE_FORMAT_XML, "Room",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_Room), 0); // Is room scene? maybe?
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCollisionHeaderV0>(),
                                    RESOURCE_FORMAT_BINARY, "CollisionHeader",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLCollisionHeaderV0>(), RESOURCE_FORMAT_XML,
                                    "CollisionHeader", static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader),
                                    0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonV0>(), RESOURCE_FORMAT_BINARY,
                                    "Skeleton", static_cast<uint32_t>(SOH::ResourceType::SOH_Skeleton), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSkeletonV0>(), RESOURCE_FORMAT_XML,
                                    "Skeleton", static_cast<uint32_t>(SOH::ResourceType::SOH_Skeleton), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonLimbV0>(),
                                    RESOURCE_FORMAT_BINARY, "SkeletonLimb",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSkeletonLimbV0>(), RESOURCE_FORMAT_XML,
                                    "SkeletonLimb", static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryTextV0>(), RESOURCE_FORMAT_BINARY,
                                    "Text", static_cast<uint32_t>(SOH::ResourceType::SOH_Text), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLTextV0>(), RESOURCE_FORMAT_XML, "Text",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_Text), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSampleV2>(), RESOURCE_FORMAT_BINARY,
                                    "AudioSample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 2);

    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSampleV0>(), RESOURCE_FORMAT_XML,
                                    "Sample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSoundFontV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSoundFont",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSoundFontV0>(), RESOURCE_FORMAT_XML,
                                    "SoundFont", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSequenceV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSequence",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSequenceV0>(), RESOURCE_FORMAT_XML,
                                    "Sequence", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 0);
    auto versions = context->GetResourceManager()->GetArchiveManager()->GetGameVersions();

    for (uint32_t version : versions) {
        if (!ValidHashes.contains(version)) {
#if defined(__SWITCH__)
            PathEngineLog("Invalid OTR File!");
#elif defined(__WIIU__)
            Ship::WiiU::ThrowInvalidOTR();
#elif defined(_WIN32)
            MessageBoxA(nullptr, "Attempted to load an invalid OTR file. Try regenerating.", "Invalid OTR File",
                        MB_OK | MB_ICONERROR);
#else
            std::fprintf(stderr, "Invalid OTR File: Attempted to load an invalid OTR file. Try regenerating.\n");
            PathEngineLog("Invalid OTR File!");
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
        return Ship::Context::GetInstance()->GetWindow()->GetCurrentRefreshRate();
    } else if (!Ship::Context::GetInstance()->GetWindow()->CanDisableVerticalSync()) {
        return Ship::Context::GetInstance()->GetWindow()->GetCurrentRefreshRate();
    }
    return CVarGetInteger(CVAR_SETTING("InterpolationFPS"), 20);
}

extern "C" void AudioMgr_CreateNextAudioBuffer(s16* samples, u32 num_samples);
extern "C" void AudioPlayer_Play(const uint8_t* buf, uint32_t len);
extern "C" int AudioPlayer_Buffered(void);
extern "C" int AudioPlayer_GetDesiredBuffered(void);
std::unordered_map<std::string, ExtensionEntry> ExtensionCache;

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
        u32 num_audio_samples = samples_left < AudioPlayer_GetDesiredBuffered() ? SAMPLES_HIGH : SAMPLES_LOW;

        // 3 is the maximum authentic frame divisor.
        s16 audio_buffer[SAMPLES_HIGH * NUM_AUDIO_CHANNELS * 3];
        for (int i = 0; i < AUDIO_FRAMES_PER_UPDATE; i++) {
            AudioMgr_CreateNextAudioBuffer(audio_buffer + i * (num_audio_samples * NUM_AUDIO_CHANNELS),
                                           num_audio_samples);
        }

        AudioPlayer_Play((u8*)audio_buffer,
                         num_audio_samples * (sizeof(int16_t) * NUM_AUDIO_CHANNELS * AUDIO_FRAMES_PER_UPDATE));

        audio.processing = false;
        audio.cv_from_thread.notify_one();
    }
}

// C->C++ Bridge
extern "C" void OTRAudio_Init() {
    // Precache all our samples, sequences, etc...
    ResourceMgr_LoadDirectory("audio");

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
#if 0
    for (size_t i = 0; i < sequenceMapSize; i++) {
        free(sequenceMap[i]);
    }
    free(sequenceMap);

    for (size_t i = 0; i < fontMapSize; i++) {
        free(fontMap[i]);
    }
    free(fontMap);
    free(gAudioContext.seqLoadStatus);
    free(gAudioContext.fontLoadStatus);
#endif
}

extern "C" void OTRExtScanner() {
    auto lst = *Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager()->ListFiles().get();

    for (auto& rPath : lst) {
        std::vector<std::string> raw = StringHelper::Split(rPath, ".");
        std::string ext = raw[raw.size() - 1];
        std::string nPath = rPath.substr(0, rPath.size() - (ext.size() + 1));
        replace(nPath.begin(), nPath.end(), '\\', '/');

        ExtensionCache[nPath] = { rPath, ext };
    }
}

// Read the port version from an OTR file
OTRVersion ReadPortVersionFromOTR(std::string otrPath) {
    OTRVersion version = {};

    // Use a temporary archive instance to load the otr and read the version file
    auto archive = std::make_shared<Ship::O2rArchive>(otrPath);
    if (archive->Open()) {
        auto t = archive->LoadFile("portVersion");
        if (t != nullptr && t->IsLoaded) {
            auto stream = std::make_shared<Ship::MemoryStream>(t->Buffer->data(), t->Buffer->size());
            auto reader = std::make_shared<Ship::BinaryReader>(stream);
            Ship::Endianness endianness = (Ship::Endianness)reader->ReadUByte();
            reader->SetEndianness(endianness);
            version.major = reader->ReadUInt16();
            version.minor = reader->ReadUInt16();
            version.patch = reader->ReadUInt16();
        }
    }

    return version;
}

// Checks the program version stored in the otr and compares the major value to soh
// For Windows/Mac/Linux if the version doesn't match, offer to
OTRVersion DetectOTRVersion(std::string fileName, bool isMQ) {
    bool isOtrOld = false;
    std::string otrPath = Ship::Context::LocateFileAcrossAppDirs(fileName, appShortName);

    // Doesn't exist so nothing to do here
    if (!std::filesystem::exists(otrPath)) {
        return { INT16_MAX, INT16_MAX, INT16_MAX };
    }

    return ReadPortVersionFromOTR(otrPath);
}

extern "C" void Messagebox_ShowErrorBox(char* title, char* body) {
#ifdef _WIN32
    MessageBoxA(nullptr, body, title, MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "%s: %s\n", title, body);
#endif
}

bool VerifyArchiveVersion(OTRVersion version) {
    return version.major != INT16_MAX && version.major != gBuildVersionMajor;
}

extern "C" void InitOTR(int argc, char* argv[]) {
    OTRGlobals::Instance = new OTRGlobals();
    OTRGlobals::Instance->RunExtract(argc, argv);

    OTRGlobals::Instance->Initialize();
    InitColViewer();
    ActorDB::Instance = new ActorDB();
    OTRAudio_Init();
    OTRExtScanner();
    ActorDB::AddBuiltInCustomActors();
    NetworkGame_Initialize(argc, argv);
    Ship::Context::GetInstance()->GetFileDropMgr()->RegisterDropHandler(SoH_HandleConfigDrop);

    srand(time(NULL));
}

extern "C" void DeinitOTR() {
    NetworkGame_Shutdown();
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
    NetworkGame_UpdateTransport();
#ifdef _WIN32
    if (Ship::GetWin32Input().ConsumePress(VK_F1)) {
        ToggleColViewer();
    }
#endif
}

void RunCommands(Gfx* Commands, const std::vector<std::unordered_map<Mtx*, MtxF>>& mtx_replacements) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(OTRGlobals::Instance->context->GetWindow());

    if (wnd == nullptr) {
        return;
    }

    // Process window events for resize, mouse, keyboard events
    wnd->HandleEvents();

    auto intp = wnd->GetInterpreterWeak().lock().get();
    intp->mInterpolationIndex = 0;

    for (const auto& m : mtx_replacements) {
        wnd->DrawAndRunGraphicsCommands(Commands, m);
        intp->mInterpolationIndex++;
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
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());

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

    RunCommands(commands, mtx_replacements);

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
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    if (wnd == nullptr) {
        return;
    }

    wnd->GetPixelDepthPrepare(x, y);
}

extern "C" uint16_t OTRGetPixelDepth(float x, float y) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    if (wnd == nullptr) {
        return 0;
    }

    return wnd->GetPixelDepth(x, y);
}

std::map<std::string, SoundFontSample*> cachedCustomSFs;

extern "C" SoundFontSample* ReadCustomSample(const char* path) {
    return nullptr;
    /*
    if (!ExtensionCache.contains(path))
        return nullptr;

    ExtensionEntry entry = ExtensionCache[path];

    auto sampleRaw = Ship::Context::GetInstance()->GetResourceManager()->LoadFile(entry.path);
    uint32_t* strem = (uint32_t*)sampleRaw->Buffer.get();
    uint8_t* strem2 = (uint8_t*)strem;

    SoundFontSample* sampleC = new SoundFontSample;

    if (entry.ext == "wav") {
        drwav_uint32 channels;
        drwav_uint32 sampleRate;
        drwav_uint64 totalPcm;
        drmp3_int16* pcmData =
            drwav_open_memory_and_read_pcm_frames_s16(strem2, sampleRaw->BufferSize, &channels, &sampleRate, &totalPcm,
    NULL); sampleC->size = totalPcm; sampleC->sampleAddr = (uint8_t*)pcmData; sampleC->codec = CODEC_S16;

        sampleC->loop = new AdpcmLoop;
        sampleC->loop->start = 0;
        sampleC->loop->end = sampleC->size - 1;
        sampleC->loop->count = 0;
        sampleC->sampleRateMagicValue = 'RIFF';
        sampleC->sampleRate = sampleRate;

        cachedCustomSFs[path] = sampleC;
        return sampleC;
    } else if (entry.ext == "mp3") {
        drmp3_config mp3Info;
        drmp3_uint64 totalPcm;
        drmp3_int16* pcmData =
            drmp3_open_memory_and_read_pcm_frames_s16(strem2, sampleRaw->BufferSize, &mp3Info, &totalPcm, NULL);

        sampleC->size = totalPcm * mp3Info.channels * sizeof(short);
        sampleC->sampleAddr = (uint8_t*)pcmData;
        sampleC->codec = CODEC_S16;

        sampleC->loop = new AdpcmLoop;
        sampleC->loop->start = 0;
        sampleC->loop->end = sampleC->size;
        sampleC->loop->count = 0;
        sampleC->sampleRateMagicValue = 'RIFF';
        sampleC->sampleRate = mp3Info.sampleRate;

        cachedCustomSFs[path] = sampleC;
        return sampleC;
    }

    return nullptr;
    */
}

extern "C" uint32_t OTRGetCurrentWidth() {
    return OTRGlobals::Instance->context->GetWindow()->GetWidth();
}

extern "C" uint32_t OTRGetCurrentHeight() {
    return OTRGlobals::Instance->context->GetWindow()->GetHeight();
}

extern "C" void OTRControllerCallback(uint8_t rumble) {
}

extern "C" float OTRGetAspectRatio() {
    return Ship::Context::GetInstance()->GetWindow()->GetAspectRatio();
}

extern "C" float OTRGetDimensionFromLeftEdge(float v) {
    return (SCREEN_WIDTH / 2 - SCREEN_HEIGHT / 2 * OTRGetAspectRatio() + (v));
}

extern "C" float OTRGetDimensionFromRightEdge(float v) {
    return (SCREEN_WIDTH / 2 + SCREEN_HEIGHT / 2 * OTRGetAspectRatio() - (SCREEN_WIDTH - v));
}

// Gets the width of the current render target area
extern "C" uint32_t OTRGetGameRenderWidth() {
    auto fastWnd = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
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
    auto fastWnd = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    auto intP = fastWnd->GetInterpreterWeak().lock();

    if (!intP) {
        assert(false && "Lost reference to Fast::Interpreter");
        return 240;
    }

    uint32_t height, width;
    intP->GetCurDimensions(&width, &height);

    return height;
}

f32 floorf(f32 x);
f32 ceilf(f32 x);  // This gets annoying

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

extern "C" int Controller_ShouldRumble(size_t slot) {
    return 0;
}

extern "C" void Gfx_RegisterBlendedTexture(const char* name, u8* mask, u8* replacement) {
    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow())
                        ->GetInterpreterWeak()
                        .lock()) {
        intP->RegisterBlendedTexture(name, mask, replacement);
    } else {
        assert(false && "Lost reference to Fast::Interpreter");
    }
}

extern "C" void Gfx_UnregisterBlendedTexture(const char* name) {
    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow())
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

    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow())
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

        Ship::Context::GetInstance()->GetConsoleVariables()->Save();
        uint32_t finalHash = SohUtils::Hash(configJson.dump());
        PathEngineLog("Configuration loaded. Hash: {}", finalHash);
        return true;
    } catch (std::exception& e) {
        PathEngineLog("Failed to load config file: {}", e.what());
        return false;
    } catch (...) {
        PathEngineLog("Failed to load config file");
        return false;
    }
    return false;
}

extern "C" uint32_t Ship_GetInterpolationFPS() {
    return OTRGlobals::Instance->GetInterpolationFPS();
}

// Number of interpolated frames
extern "C" uint32_t Ship_GetInterpolationFrameCount() {
    return ceil((float)Ship_GetInterpolationFPS() / 20.0f);
}
