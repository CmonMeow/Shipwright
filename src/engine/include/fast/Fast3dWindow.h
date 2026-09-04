#pragma once
#include "engine/window/Window.h"

union Gfx;
#include "interpreter.h"

namespace Fast {
class Fast3dWindow : public Engine::Window {
  public:
    Fast3dWindow(void* applicationInstance, int showCommand);
    ~Fast3dWindow();

    void Init() override;
    void Close() override;
    void StartFrame() override;
    void EndFrame() override;
    bool IsFrameReady() override;
    void HandleEvents() override;
    void SetCursorVisibility(bool visible) override;
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    int32_t GetPosX() override;
    int32_t GetPosY() override;
    float GetAspectRatio() override;
    void SetMousePos(Engine::Coords pos) override;
    Engine::Coords GetMousePos() override;
    Engine::Coords GetMouseDelta() override;
    Engine::CoordsF GetMouseWheel() override;
    bool GetMouseState(uint32_t button) override;
    void SetMouseCapture(bool capture) override;
    bool IsMouseCaptured() override;
    uint32_t GetCurrentRefreshRate() override;
    bool SupportsWindowedFullscreen() override;
    bool CanDisableVerticalSync() override;
    void SetResolutionMultiplier(float multiplier) override;
    void SetMsaaLevel(uint32_t value) override;
    void SetFullscreen(bool isFullscreen) override;
    bool IsFullscreen() override;
    bool IsRunning() override;
    uintptr_t GetGfxFrameBuffer() override;
    const char* GetKeyName(int32_t scancode) override;

    void InitWindowManager();
    int32_t GetTargetFps();
    void SetTargetFps(int32_t fps);
    void SetMaximumFrameLatency(int32_t latency);
    void GetPixelDepthPrepare(float x, float y);
    uint16_t GetPixelDepth(float x, float y);
    void SetTextureFilter(FilteringMode filteringMode);
    void SetRendererUCode(UcodeHandlers ucode);
    void EnableSRGBMode();
    bool DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements);

    std::weak_ptr<Interpreter> GetInterpreterWeak() const;

  protected:
    static void OnFullscreenChanged(bool isNowFullscreen);

  private:
    GfxRenderingAPI* mRenderingApi;
    GfxWindowBackend* mWindowManagerApi;
    std::shared_ptr<Interpreter> mInterpreter = nullptr;
    void* mApplicationInstance = nullptr;
    int mShowCommand = 0;
};
} // namespace Fast
