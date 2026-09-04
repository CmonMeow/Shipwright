#include "platform/client/NativeFramePresenter.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <vector>

#include <engine/config/ConsoleVariable.h>
#include <rendering/debug/GfxDebugger.h>
#include <rendering/GameRenderer.h>
#include <rendering/interpreter.h>

#include "platform/SettingsKeys.h"
#include "rendering/FrameInterpolation.h"
#include "platform/client/NativeAudioWorker.h"
#include "variables.h"

namespace Game::Client {

NativeFramePresenter::NativeFramePresenter(Engine::Rendering::GameRenderer& renderer,
                                           Engine::ConsoleVariable& consoleVariables,
                                           Engine::Rendering::GfxDebugger& graphicsDebugger,
                                           NativeAudioWorker& audioWorker)
    : mRenderer(renderer), mConsoleVariables(consoleVariables), mGraphicsDebugger(graphicsDebugger),
      mAudioWorker(audioWorker) {
}

uint32_t NativeFramePresenter::GetInterpolationFps() const {
    if (mConsoleVariables.GetInteger(CVAR_SETTING("MatchRefreshRate"), 0) ||
        mConsoleVariables.GetInteger(CVAR_VSYNC_ENABLED, 1) ||
        !mRenderer.CanDisableVerticalSync()) {
        return mRenderer.GetCurrentRefreshRate();
    }
    return mConsoleVariables.GetInteger(CVAR_SETTING("InterpolationFPS"), 20);
}

uint32_t NativeFramePresenter::GetInterpolationFrameCount() const {
    const uint32_t updateRate = static_cast<uint32_t>(std::clamp(static_cast<int>(R_UPDATE_RATE), 1, 3));
    return PresentationFrameBudget::FrameCount(GetInterpolationFps(), 60U / updateRate);
}

void NativeFramePresenter::Process(Gfx* commands) {
    mAudioWorker.BeginFrame();

    const int targetFps = static_cast<int>(GetInterpolationFps());
    const int updateRate = std::clamp(static_cast<int>(R_UPDATE_RATE), 1, 3);
    const int simulationFps = 60 / updateRate;
    const int presentationFps = targetFps == 20 || simulationFps > targetFps ? simulationFps : targetFps;

    if (mLastPresentationFps != presentationFps || mLastUpdateRate != updateRate) {
        mInterpolationTime = 0;
    }

    std::vector<std::unordered_map<Mtx*, MtxF>> matrixReplacements;
    const int nextNativeFrame = presentationFps;
    while (mInterpolationTime + simulationFps <= nextNativeFrame) {
        mInterpolationTime += simulationFps;
        if (mInterpolationTime != nextNativeFrame) {
            matrixReplacements.push_back(
                FrameInterpolation_Interpolate(static_cast<float>(mInterpolationTime) / nextNativeFrame));
        } else {
            matrixReplacements.emplace_back();
        }
    }
    mInterpolationTime -= presentationFps;

    mRenderer.SetTargetFps(presentationFps);

    if (mGraphicsDebugger.IsDebugging()) {
        matrixReplacements.clear();
        matrixReplacements.emplace_back();
    }

    auto* interpreter = mRenderer.GetInterpreter();
    if (interpreter != nullptr) {
        interpreter->mInterpolationIndex = 0;
        mFrameBudget.BeginBatch(presentationFps, simulationFps);

        using Clock = std::chrono::steady_clock;
        const auto batchStart = Clock::now();
        for (size_t index = 0; index < matrixReplacements.size(); ++index) {
            const bool newestNativeState = index + 1 == matrixReplacements.size();
            if (!newestNativeState) {
                const double elapsedSeconds =
                    std::chrono::duration<double>(Clock::now() - batchStart).count();
                if (!mFrameBudget.CanPresentIntermediate(elapsedSeconds)) {
                    continue;
                }
            }

            const auto presentStart = Clock::now();
            interpreter->mInterpolationIndex = static_cast<int>(index);
            mRenderer.DrawAndRunGraphicsCommands(commands, matrixReplacements[index]);
            const double presentSeconds =
                std::chrono::duration<double>(Clock::now() - presentStart).count();
            mFrameBudget.ObservePresent(presentSeconds);
        }
    }

    mLastPresentationFps = presentationFps;
    mLastUpdateRate = updateRate;
    mAudioWorker.WaitForFrame();
}

} // namespace Game::Client
