#pragma once

#include <cstdint>

#include "platform/client/PresentationFrameBudget.h"

union Gfx;

namespace Engine {
class ConsoleVariable;

namespace Rendering {
class GfxDebugger;
class GameRenderer;
}
}

namespace Game::Client {

class NativeAudioWorker;

class NativeFramePresenter final {
  public:
    NativeFramePresenter(Engine::Rendering::GameRenderer& renderer, Engine::ConsoleVariable& consoleVariables,
                         Engine::Rendering::GfxDebugger& graphicsDebugger, NativeAudioWorker& audioWorker);

    void Process(Gfx* commands);
    uint32_t GetInterpolationFps() const;
    uint32_t GetInterpolationFrameCount() const;

  private:
    Engine::Rendering::GameRenderer& mRenderer;
    Engine::ConsoleVariable& mConsoleVariables;
    Engine::Rendering::GfxDebugger& mGraphicsDebugger;
    NativeAudioWorker& mAudioWorker;
    PresentationFrameBudget mFrameBudget;
    int mLastPresentationFps = 0;
    int mLastUpdateRate = 0;
    int mInterpolationTime = 0;
};

} // namespace Game::Client
