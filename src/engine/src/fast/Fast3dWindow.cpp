#include <runtime/log/Log.hpp>
#include "fast/Fast3dWindow.h"

#include "engine/Context.h"
#include "engine/config/Config.h"
#include "engine/config/ConsoleVariable.h"
#include "fast/interpreter.h"
#include "fast/backends/gfx_opengl.h"
#include "platform/win32/Win32OpenGLWindow.h"
#include "fast/backends/gfx_window_manager_api.h"

namespace Fast {

extern void GfxSetInstance(std::shared_ptr<Interpreter> gfx);

Fast3dWindow::Fast3dWindow(void* applicationInstance, int showCommand)
    : Engine::Window(), mApplicationInstance(applicationInstance),
      mShowCommand(showCommand) {
    mWindowManagerApi = nullptr;
    mRenderingApi = nullptr;
    mInterpreter = std::make_shared<Interpreter>();
    GfxSetInstance(mInterpreter);

}

Fast3dWindow::~Fast3dWindow() {
    WriteLog("destruct fast3dwindow");
    mInterpreter->Destroy();
    delete mRenderingApi;
    delete mWindowManagerApi;
}

void Fast3dWindow::Init() {
    bool isFullscreen;
    uint32_t width, height;
    int32_t posX, posY;

    isFullscreen = Engine::Context::GetInstance()->GetConfig()->GetBool("Window.Fullscreen.Enabled", false);
    posX = Engine::Context::GetInstance()->GetConfig()->GetInt("Window.PositionX", 100);
    posY = Engine::Context::GetInstance()->GetConfig()->GetInt("Window.PositionY", 100);

    if (isFullscreen) {
        width = Engine::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Width", 1920);
        height = Engine::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Height", 1080);
    } else {
        width = Engine::Context::GetInstance()->GetConfig()->GetInt("Window.Width", 640);
        height = Engine::Context::GetInstance()->GetConfig()->GetInt("Window.Height", 480);
    }
    InitWindowManager();
    mWindowManagerApi->Init(Engine::Context::GetInstance()->GetName().c_str(),
                            mRenderingApi->GetName(), isFullscreen, width,
                            height, posX, posY);
    mRenderingApi->Init();
    mInterpreter->InitializeRenderer(mWindowManagerApi, mRenderingApi, width,
                                     height);
    mWindowManagerApi->SetFullscreenChangedCallback(OnFullscreenChanged);

    SetTextureFilter((FilteringMode)Engine::Context::GetInstance()->GetConsoleVariables()->GetInteger(
        CVAR_TEXTURE_FILTER, FILTER_THREE_POINT));
}

int32_t Fast3dWindow::GetTargetFps() {
    return mInterpreter->GetTargetFps();
}

void Fast3dWindow::SetTargetFps(int32_t fps) {
    mInterpreter->SetTargetFps(fps);
}

void Fast3dWindow::SetMaximumFrameLatency(int32_t latency) {
    mInterpreter->SetMaxFrameLatency(latency);
}

void Fast3dWindow::GetPixelDepthPrepare(float x, float y) {
    mInterpreter->GetPixelDepthPrepare(x, y);
}

uint16_t Fast3dWindow::GetPixelDepth(float x, float y) {
    return mInterpreter->GetPixelDepth(x, y);
}

void Fast3dWindow::InitWindowManager() {
#ifdef _WIN32
    mRenderingApi = new GfxRenderingAPIOGL();
    mWindowManagerApi = new Win32OpenGLWindow(
        static_cast<HINSTANCE>(mApplicationInstance), mShowCommand);
#else
    WriteLog("No supported rendering backend for this platform");
#endif
}

void Fast3dWindow::SetTextureFilter(FilteringMode filteringMode) {
    mInterpreter->GetCurrentRenderingAPI()->SetTextureFilter(filteringMode);
}

void Fast3dWindow::EnableSRGBMode() {
    mInterpreter->mRapi->SetSrgbMode();
}

void Fast3dWindow::SetRendererUCode(UcodeHandlers ucode) {
    gfx_set_target_ucode(ucode);
}

void Fast3dWindow::Close() {
    mWindowManagerApi->Close();
}

void Fast3dWindow::StartFrame() {
    mInterpreter->StartFrame();
}

void Fast3dWindow::EndFrame() {
    mInterpreter->EndFrame();
}

bool Fast3dWindow::IsFrameReady() {
    return mWindowManagerApi->IsFrameReady();
}

bool Fast3dWindow::DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements) {
    // Skip dropped frames
    if (!IsFrameReady()) {
        return false;
    }

    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    mInterpreter->mCurDimensions.width = width;
    mInterpreter->mCurDimensions.height = height > 0 ? height : 1;
    mInterpreter->mGameWindowViewport = { 0, 0, width, height };
    mInterpreter->StartFrame();
    mInterpreter->Run(commands, mtxReplacements);
    mInterpreter->EndFrame();
    return true;
}

void Fast3dWindow::HandleEvents() {
    mWindowManagerApi->HandleEvents();
}

void Fast3dWindow::SetCursorVisibility(bool visible) {
    mWindowManagerApi->SetCursorVisibility(visible);
}

uint32_t Fast3dWindow::GetWidth() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return width;
}

uint32_t Fast3dWindow::GetHeight() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return height;
}

float Fast3dWindow::GetAspectRatio() {
    return mInterpreter->mCurDimensions.aspect_ratio;
}

int32_t Fast3dWindow::GetPosX() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return posX;
}

int32_t Fast3dWindow::GetPosY() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return posY;
}

void Fast3dWindow::SetMousePos(Engine::Coords pos) {
    mWindowManagerApi->SetMousePos(pos.x, pos.y);
}

Engine::Coords Fast3dWindow::GetMousePos() {
    int32_t x, y;
    mWindowManagerApi->GetMousePos(&x, &y);
    return { x, y };
}

Engine::Coords Fast3dWindow::GetMouseDelta() {
    int32_t x, y;
    mWindowManagerApi->GetMouseDelta(&x, &y);
    return { x, y };
}

Engine::CoordsF Fast3dWindow::GetMouseWheel() {
    float x, y;
    mWindowManagerApi->GetMouseWheel(&x, &y);
    return { x, y };
}

bool Fast3dWindow::GetMouseState(uint32_t button) {
    return mWindowManagerApi->GetMouseState(button);
}

void Fast3dWindow::SetMouseCapture(bool capture) {
    mWindowManagerApi->SetMouseCapture(capture);
}

bool Fast3dWindow::IsMouseCaptured() {
    return mWindowManagerApi->IsMouseCaptured();
}

uint32_t Fast3dWindow::GetCurrentRefreshRate() {
    uint32_t refreshRate;
    mWindowManagerApi->GetActiveWindowRefreshRate(&refreshRate);
    return refreshRate;
}

bool Fast3dWindow::SupportsWindowedFullscreen() {
    return true;
}

bool Fast3dWindow::CanDisableVerticalSync() {
    return mWindowManagerApi->CanDisableVsync();
}

void Fast3dWindow::SetResolutionMultiplier(float multiplier) {
    mInterpreter->SetResolutionMultiplier(multiplier);
}

void Fast3dWindow::SetMsaaLevel(uint32_t value) {
    mInterpreter->SetMsaaLevel(value);
}

void Fast3dWindow::SetFullscreen(bool isFullscreen) {
    // Save current window position before fullscreening
    SaveWindowToConfig();
    mWindowManagerApi->SetFullscreen(isFullscreen);
}

bool Fast3dWindow::IsFullscreen() {
    return mWindowManagerApi->IsFullscreen();
}

bool Fast3dWindow::IsRunning() {
    return mWindowManagerApi->IsRunning();
}

uintptr_t Fast3dWindow::GetGfxFrameBuffer() {
    return mInterpreter->mGfxFrameBuffer;
}

const char* Fast3dWindow::GetKeyName(int32_t scancode) {
    return mWindowManagerApi->GetKeyName(scancode);
}

void Fast3dWindow::OnFullscreenChanged(bool isNowFullscreen) {
    std::shared_ptr<Window> wnd = Engine::Context::GetInstance()->GetWindow();

    // Re-save fullscreen enabled after
    Engine::Context::GetInstance()->GetConfig()->SetBool("Window.Fullscreen.Enabled", isNowFullscreen);
}

std::weak_ptr<Interpreter> Fast3dWindow::GetInterpreterWeak() const {
    return mInterpreter;
}

} // namespace Fast
