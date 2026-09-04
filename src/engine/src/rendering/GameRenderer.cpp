#include <runtime/log/Log.hpp>

#include "rendering/GameRenderer.h"

#include "engine/config/ConsoleVariable.h"
#include "rendering/backends/gfx_opengl.h"
#include "platform/win32/OpenGLPresentation.h"

namespace Engine::Rendering {
void GfxSetInstance(Interpreter* gfx);

GameRenderer::GameRenderer(ConsoleVariable& variables, ResourceManager& resources, GfxDebugger& debugger,
                           OpenGLPresentation& presentation)
    : mVariables(variables), mPresentation(presentation),
      mRenderingApi(std::make_unique<GfxRenderingAPIOGL>(resources, variables)),
      mInterpreter(std::make_unique<Interpreter>(resources, variables, debugger)) {
    GfxSetInstance(mInterpreter.get());
}

GameRenderer::~GameRenderer() {
    WriteLog("destruct game renderer");
    if (mInterpreter) {
        mInterpreter->ReleaseGraphicsResources();
    }
    GfxSetInstance(nullptr);
    if (mRendererInitialized) {
        mRenderingApi->Shutdown();
        mRendererInitialized = false;
    }
}

void GameRenderer::Init() {
    const uint32_t width = mPresentation.Width();
    const uint32_t height = mPresentation.Height();
    mRenderingApi->Init();
    mRendererInitialized = true;
    mInterpreter->InitializeRenderer(&mPresentation, mRenderingApi.get(), width, height);

    SetTextureFilter(static_cast<Engine::Rendering::FilteringMode>(
        mVariables.GetInteger(CVAR_TEXTURE_FILTER, Engine::Rendering::FILTER_THREE_POINT)));
}

void GameRenderer::SetTargetFps(int32_t fps) {
    mPresentation.SetTargetFps(fps);
}

void GameRenderer::GetPixelDepthPrepare(float x, float y) {
    mInterpreter->GetPixelDepthPrepare(x, y);
}

uint16_t GameRenderer::GetPixelDepth(float x, float y) {
    return mInterpreter->GetPixelDepth(x, y);
}

void GameRenderer::SetTextureFilter(FilteringMode filteringMode) {
    mRenderingApi->SetTextureFilter(filteringMode);
}

bool GameRenderer::DrawAndRunGraphicsCommands(Gfx* commands,
                                              const std::unordered_map<Mtx*, MtxF>& matrixReplacements) {
    const uint32_t width = mPresentation.Width();
    const uint32_t height = mPresentation.Height();
    mInterpreter->mCurDimensions.width = width;
    mInterpreter->mCurDimensions.height = height > 0 ? height : 1;
    mInterpreter->mPresentationViewport = { 0, 0, width, height };
    mInterpreter->StartFrame();
    mInterpreter->Run(commands, matrixReplacements);
    mInterpreter->EndFrame();
    return true;
}

uint32_t GameRenderer::GetWidth() {
    return mPresentation.Width();
}

uint32_t GameRenderer::GetHeight() {
    return mPresentation.Height();
}

float GameRenderer::GetAspectRatio() {
    return mInterpreter->mCurDimensions.aspect_ratio;
}

uint32_t GameRenderer::GetCurrentRefreshRate() {
    return mPresentation.RefreshRate();
}

bool GameRenderer::CanDisableVerticalSync() {
    return mPresentation.CanDisableVsync();
}

Interpreter* GameRenderer::GetInterpreter() const {
    return mInterpreter.get();
}

} // namespace Engine::Rendering
