#pragma once

#ifdef _WIN32

#include "fast/backends/gfx_window_manager_api.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <string>

namespace Fast {

class Win32OpenGLWindow final : public GfxWindowBackend {
  public:
    Win32OpenGLWindow(HINSTANCE instance, int showCommand);
    ~Win32OpenGLWindow() override = default;

    void Init(const char* gameName, const char* apiName, bool startFullScreen, uint32_t width, uint32_t height,
              int32_t posX, int32_t posY) override;
    void Close() override;
    void SetKeyboardCallbacks(bool (*onKeyDown)(int), bool (*onKeyUp)(int), void (*onAllKeysUp)()) override;
    void SetMouseCallbacks(bool (*onMouseButtonDown)(int), bool (*onMouseButtonUp)(int)) override;
    void SetFullscreenChangedCallback(void (*onFullscreenChanged)(bool)) override;
    void SetFullscreen(bool fullscreen) override;
    void GetActiveWindowRefreshRate(uint32_t* refreshRate) override;
    void SetCursorVisibility(bool visible) override;
    void SetMousePos(int32_t posX, int32_t posY) override;
    void GetMousePos(int32_t* x, int32_t* y) override;
    void GetMouseDelta(int32_t* x, int32_t* y) override;
    void GetMouseWheel(float* x, float* y) override;
    bool GetMouseState(uint32_t btn) override;
    void SetMouseCapture(bool capture) override;
    bool IsMouseCaptured() override;
    void GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) override;
    void HandleEvents() override;
    bool IsFrameReady() override;
    void SwapBuffersBegin() override;
    void SwapBuffersEnd() override;
    double GetTime() override;
    int GetTargetFps() override;
    void SetTargetFps(int fps) override;
    void SetMaxFrameLatency(int latency) override;
    const char* GetKeyName(int scancode) override;
    bool CanDisableVsync() override;
    bool IsRunning() override;
    void Destroy() override;
    bool IsFullscreen() override;

    LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

  private:
    void KeyDown(LPARAM lParam);
    void KeyUp(LPARAM lParam);
    void MouseDown(int button);
    void MouseUp(int button);
    void UpdateClientSize();
    void ApplySwapInterval();
    void ToggleSettingsWindow();
    void CreateSettingsWindow();
    void SyncSettingsControls();
    void SyncNetworkStatus();

    HWND mWindow = nullptr;
    HWND mSettingsWindow = nullptr;
    HWND mFullscreenCheck = nullptr;
    HWND mVsyncCheck = nullptr;
    HWND mFpsCombo = nullptr;
    HWND mNetworkStatusLabel = nullptr;
    HWND mNetworkAddressEdit = nullptr;
    HWND mNetworkPortEdit = nullptr;
    HWND mChatCheck = nullptr;
    HWND mVoiceCheck = nullptr;
    HWND mPushToTalkCheck = nullptr;
    HWND mCameraInvertYCheck = nullptr;
    HDC mDeviceContext = nullptr;
    HGLRC mRenderContext = nullptr;
    HINSTANCE mInstance = nullptr;
    int mShowCommand = SW_SHOWNORMAL;
    HANDLE mFrameTimer = nullptr;
    LARGE_INTEGER mClockFrequency{};
    LARGE_INTEGER mClockStart{};
    LARGE_INTEGER mLastPresent{};
    LARGE_INTEGER mTitleSampleTime{};
    POINT mPreviousMouse{};
    std::array<bool, 5> mMouseButtons{};
    float mMouseWheelX = 0.0f;
    float mMouseWheelY = 0.0f;
    uint32_t mWidth = 640;
    uint32_t mHeight = 480;
    int32_t mPosX = 100;
    int32_t mPosY = 100;
    bool mMouseCaptured = false;
    bool mHavePreviousMouse = false;
    bool mSwapIntervalApplied = false;
    bool mSettingsVisible = false;
    uint32_t mTitleFrameCount = 0;
    std::string mBaseTitle;
    void (*mOnAllKeysUp)() = nullptr;
    std::string mKeyName;
};

} // namespace Fast

#endif
