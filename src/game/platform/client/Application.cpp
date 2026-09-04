#include "platform/client/Application.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>

#include <engine/audio/Audio.h>
#include <engine/config/Config.h>
#include <engine/config/ConsoleVariable.h>
#include <engine/debug/Console.h>
#include <engine/debug/CrashHandler.h>
#include <engine/resource/ResourceManager.h>
#include <rendering/GameRenderer.h>
#include <rendering/debug/GfxDebugger.h>
#include <rendering/interpreter.h>

#include "runtime/actors/ActorDB.h"
#include "functions.h"
#include "gameplay/Controls.h"
#include "multiplayer/NativeClientNetworkSession.h"
#include "debug/collision/colViewer.h"
#include "multiplayer/NetworkVersion.h"
#include "platform/client/GameResourceRegistry.h"
#include "platform/client/RetainedGameBridge.h"

namespace {

std::string LocateGameArchive(const std::filesystem::path& executableDirectory) {
    const std::filesystem::path workingCopy = std::filesystem::current_path() / "oot.o2r";
    if (std::filesystem::exists(workingCopy)) {
        return workingCopy.string();
    }
    return (executableDirectory / "oot.o2r").string();
}

} // namespace

Application::Application(const std::filesystem::path& executableDirectory, Input& input)
    : mApplicationName("v" + std::to_string(APP_PROTOCOL_VERSION)),
      mGameArchivePath(LocateGameArchive(executableDirectory)),
      mInput(input),
      mCrashHandler(std::make_unique<Engine::CrashHandler>(mApplicationName)),
      mConfig(std::make_unique<Engine::Config>("settings.json")),
      mConsoleVariables(std::make_unique<Engine::ConsoleVariable>(*mConfig)),
      mResourceManager(std::make_unique<Engine::ResourceManager>()), mConsole(std::make_unique<Engine::Console>()),
      mGraphicsDebugger(std::make_unique<Engine::Rendering::GfxDebugger>()) {
    // Remove settings for retired UI and controller systems so stale mappings and window state cannot survive.
    mConfig->EraseBlock("CVars.gSettings.Controllers");
    mConfig->EraseBlock("CVars.gSettings.AdvancedResolution");
    mConfig->EraseBlock("CVars.gSettings.Menu");
    mConfig->EraseBlock("CVars.gOpenWindows");
    mConfig->Erase("CVars.gSettings.OverlayFont");
    mConfig->Save();

    RequireGameArchive();
    mResourceManager->Init({ mGameArchivePath }, {}, 3);
    mConsole->Init();
}

Application::~Application() {
    Shutdown();
    mConfig->Save();
}

WindowBounds Application::LoadWindowBounds() const {
    return {
        static_cast<uint32_t>(std::max(mConfig->GetInt("Window.Width", 640), 1)),
        static_cast<uint32_t>(std::max(mConfig->GetInt("Window.Height", 480), 1)),
        mConfig->GetInt("Window.PositionX", 100),
        mConfig->GetInt("Window.PositionY", 100),
    };
}

void Application::AttachPresentation(OpenGLPresentation& presentation) {
    mRenderer = std::make_unique<Engine::Rendering::GameRenderer>(
        *mConsoleVariables, *mResourceManager, *mGraphicsDebugger, presentation);
    mRenderer->Init();
    mNetworkSession =
        std::make_unique<Game::Multiplayer::NativeClientNetworkSession>(*mConsoleVariables, *mRenderer, mInput);
}

void Application::SaveWindowBounds(WindowBounds bounds) {
    mConfig->SetInt("Window.Width", static_cast<int32_t>(bounds.width));
    mConfig->SetInt("Window.Height", static_cast<int32_t>(bounds.height));
    mConfig->SetInt("Window.PositionX", bounds.x);
    mConfig->SetInt("Window.PositionY", bounds.y);
}

void Application::Start() {
    if (mClientStarted) {
        return;
    }

    RetainedGame_Bind(this);
    mRetainedBindingsActive = true;

    InitializeSubsystems();
    InitColViewer();

    mActorDatabase = std::make_unique<ActorDB>();
    ActorDB::Instance = mActorDatabase.get();

    mAudioWorker->Start();
    mAudioStarted = true;
    mNetworkSession->RegisterActors();
    mNetworkSession->Initialize();
    mClientStarted = true;

    const uint64_t seedTime =
        static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    std::srand(static_cast<unsigned int>(seedTime ^ (seedTime >> 32)));

    Game_Initialize();
    mGameStarted = true;
}

void Application::Shutdown() {
    if (mClientStarted) {
        mNetworkSession->Shutdown();
        mClientStarted = false;
    }
    if (mAudioStarted) {
        mAudioWorker->Stop();
        mAudioStarted = false;
    }
    if (ActorDB::Instance == mActorDatabase.get()) {
        ActorDB::Instance = nullptr;
    }
    mActorDatabase.reset();
    if (mGameStarted) {
        Game_Shutdown();
        mGameStarted = false;
    }
    if (mRetainedBindingsActive) {
        RetainedGame_Bind(nullptr);
        mRetainedBindingsActive = false;
    }
}

void Application::RunFrame() {
    controls.Update(mInput);
    Graph_RunFrame();
}

void Application::PumpMoveLoop() {
    if (mNetworkSession != nullptr) {
        mNetworkSession->PumpMoveLoop();
    }
}

void Application::BeginFrame() {
    // Transport remains active through native menus, scene loads, and death.
    mNetworkSession->UpdateTransport();
}

void Application::ToggleCollisionVisualization() {
    ToggleColViewer();
}

void Application::UpdateGameplay(PlayState* play) {
    mNetworkSession->UpdateGameplay(play);
}

void Application::PresentGraphics(Gfx* commands) {
    mFramePresenter->Process(commands);
}

uint32_t Application::GetPresentationFps() const {
    return mFramePresenter->GetInterpolationFps();
}

uint32_t Application::GetPresentationFrameCount() const {
    return mFramePresenter->GetInterpolationFrameCount();
}

void Application::SetAudioChannels(AudioChannelsSetting channels) {
    if (mAudio != nullptr) {
        mAudio->SetAudioChannels(channels);
    }
}

void Application::PreparePixelDepth(float x, float y) {
    mRenderer->GetPixelDepthPrepare(x, y);
}

uint16_t Application::GetPixelDepth(float x, float y) const {
    return mRenderer->GetPixelDepth(x, y);
}

float Application::GetAspectRatio() const {
    return mRenderer->GetAspectRatio();
}

uint32_t Application::GetRenderWidth() const {
    uint32_t width = 0;
    uint32_t height = 0;
    mRenderer->GetInterpreter()->GetCurDimensions(&width, &height);
    return width;
}

uint32_t Application::GetRenderHeight() const {
    uint32_t width = 0;
    uint32_t height = 0;
    mRenderer->GetInterpreter()->GetCurDimensions(&width, &height);
    return height;
}

bool Application::IsGraphicsDebugging() const {
    return mGraphicsDebugger->IsDebugging();
}

bool Application::IsGraphicsDebuggingRequested() const {
    return mGraphicsDebugger->IsDebuggingRequested();
}

void Application::DebugDisplayList(void* commands) {
    mGraphicsDebugger->DebugDisplayList(static_cast<Engine::Rendering::F3DGfx*>(commands));
}

Engine::ResourceManager& Application::Resources() const {
    return *mResourceManager;
}

Engine::ConsoleVariable& Application::ConsoleVariables() const {
    return *mConsoleVariables;
}

Game::Client::MultiplayerInteractionPort& Application::MultiplayerInteraction() const {
    return mNetworkSession->Interaction();
}

void Application::RequireGameArchive() const {
    if (std::filesystem::exists(mGameArchivePath)) {
        return;
    }

    MessageBoxA(nullptr, "The required combined oot.o2r archive is missing.", "No Ocarina of Time archive",
                MB_OK | MB_ICONERROR);
    std::exit(EXIT_FAILURE);
}

void Application::InitializeSubsystems() {
    mAudio = std::make_unique<Engine::Audio>(
        Engine::AudioSettings{ .SampleRate = 32000, .SampleLength = 1024, .DesiredBuffered = 1680 });
    mAudio->Init();
    mAudioWorker = std::make_unique<Game::Client::NativeAudioWorker>(*mAudio);
    mFramePresenter = std::make_unique<Game::Client::NativeFramePresenter>(
        *mRenderer, *mConsoleVariables, *mGraphicsDebugger, *mAudioWorker);

    Game::Client::RegisterGameResourceFactories(*mResourceManager->GetResourceLoader());
    const auto versions = mResourceManager->GetArchiveManager()->GetGameVersions();
    if (!Game::Client::SupportsGameVersions(versions)) {
        MessageBoxA(nullptr, "The packaged game archive has an unsupported version.", "Invalid game archive",
                    MB_OK | MB_ICONERROR);
        std::exit(EXIT_FAILURE);
    }
}
