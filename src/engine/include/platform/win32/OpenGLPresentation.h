#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>

namespace Engine {
class ConsoleVariable;
}

// Rendering-side access to the HWND and device context created and destroyed
// directly by WinMain. This object does not own the native window or message loop.
class OpenGLPresentation final {
  public:
    OpenGLPresentation(HWND window, HDC deviceContext, const int32_t& width, const int32_t& height,
                       Engine::ConsoleVariable& variables, std::string title);
    ~OpenGLPresentation();

    uint32_t RefreshRate() const;
    uint32_t Width() const;
    uint32_t Height() const;
    void SwapBuffersBegin();
    void SwapBuffersEnd();
    void SetTargetFps(int fps);
    bool CanDisableVsync() const;

  private:
    void ApplySwapInterval();

    HWND mWindow = nullptr;
    HDC mDeviceContext = nullptr;
    const int32_t& mWidth;
    const int32_t& mHeight;
    Engine::ConsoleVariable& mVariables;
    HANDLE mFrameTimer = nullptr;
    LARGE_INTEGER mClockFrequency{};
    LARGE_INTEGER mLastPresent{};
    LARGE_INTEGER mTitleSampleTime{};
    uint32_t mTitleFrameCount = 0;
    uint32_t mTargetFps = 60;
    bool mSwapIntervalApplied = false;
    bool mVsyncEnabled = true;
    std::string mBaseTitle;
};

#endif
