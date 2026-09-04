#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "platform/client/NativeAudioWorker.h"
#include "platform/client/NativeFramePresenter.h"
#include <engine/audio/AudioChannelsSetting.h>

union Gfx;
struct PlayState;
class ActorDB;

namespace Engine {
class Audio;
class Config;
class Console;
class ConsoleVariable;
class CrashHandler;
class ResourceManager;

namespace Rendering {
class GfxDebugger;
class GameRenderer;
}

}

namespace Game::Multiplayer {
class NativeClientNetworkSession;
}

struct Input;
class OpenGLPresentation;

namespace Game::Client {
class MultiplayerInteractionPort;
}

struct WindowBounds {
    uint32_t width = 640;
    uint32_t height = 480;
    int32_t x = 100;
    int32_t y = 100;
};

class Application final {
  public:
    Application(const std::filesystem::path& executableDirectory, Input& input);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    WindowBounds LoadWindowBounds() const;
    void AttachPresentation(OpenGLPresentation& presentation);
    void SaveWindowBounds(WindowBounds bounds);
    void Start();
    void Shutdown();
    void RunFrame();
    void PumpMoveLoop();
    void BeginFrame();
    void ToggleCollisionVisualization();
    void UpdateGameplay(PlayState* play);
    void PresentGraphics(Gfx* commands);
    uint32_t GetPresentationFps() const;
    uint32_t GetPresentationFrameCount() const;
    void SetAudioChannels(AudioChannelsSetting channels);
    void PreparePixelDepth(float x, float y);
    uint16_t GetPixelDepth(float x, float y) const;
    float GetAspectRatio() const;
    uint32_t GetRenderWidth() const;
    uint32_t GetRenderHeight() const;
    bool IsGraphicsDebugging() const;
    bool IsGraphicsDebuggingRequested() const;
    void DebugDisplayList(void* commands);
    Engine::ResourceManager& Resources() const;
    Engine::ConsoleVariable& ConsoleVariables() const;
    Game::Client::MultiplayerInteractionPort& MultiplayerInteraction() const;

  private:
    void RequireGameArchive() const;
    void InitializeSubsystems();

    std::string mApplicationName;
    std::string mGameArchivePath;
    Input& mInput;
    std::unique_ptr<Engine::CrashHandler> mCrashHandler;
    std::unique_ptr<Engine::Config> mConfig;
    std::unique_ptr<Engine::ConsoleVariable> mConsoleVariables;
    std::unique_ptr<Engine::ResourceManager> mResourceManager;
    std::unique_ptr<Engine::Console> mConsole;
    std::unique_ptr<Engine::Audio> mAudio;
    std::unique_ptr<Engine::Rendering::GfxDebugger> mGraphicsDebugger;
    std::unique_ptr<Engine::Rendering::GameRenderer> mRenderer;
    std::unique_ptr<ActorDB> mActorDatabase;
    std::unique_ptr<Game::Multiplayer::NativeClientNetworkSession> mNetworkSession;
    std::unique_ptr<Game::Client::NativeAudioWorker> mAudioWorker;
    std::unique_ptr<Game::Client::NativeFramePresenter> mFramePresenter;
    bool mRetainedBindingsActive = false;
    bool mGameStarted = false;
    bool mAudioStarted = false;
    bool mClientStarted = false;
};
