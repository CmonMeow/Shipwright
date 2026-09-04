#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform/win32/OpenGLPresentation.h"

#include "engine/config/ConsoleVariable.h"
#include "rendering/Overlay.h"

#include <algorithm>
#include <cstdio>
#include <utility>

OpenGLPresentation::OpenGLPresentation(HWND window, HDC deviceContext, const int32_t& width, const int32_t& height,
                                       Engine::ConsoleVariable& variables, std::string title)
    : mWindow(window), mDeviceContext(deviceContext), mWidth(width), mHeight(height), mVariables(variables),
      mBaseTitle(std::move(title)) {
    QueryPerformanceFrequency(&mClockFrequency);
    QueryPerformanceCounter(&mLastPresent);
    mTitleSampleTime = mLastPresent;
    mFrameTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (mFrameTimer == nullptr) {
        mFrameTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
}

OpenGLPresentation::~OpenGLPresentation() {
    Engine::Rendering::Overlay::Clear();
    if (mFrameTimer != nullptr) {
        CloseHandle(mFrameTimer);
    }
}

uint32_t OpenGLPresentation::RefreshRate() const {
    DEVMODEA mode{};
    mode.dmSize = sizeof(mode);
    return EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1
               ? mode.dmDisplayFrequency
               : 60;
}

uint32_t OpenGLPresentation::Width() const {
    return static_cast<uint32_t>(std::max(mWidth, 0));
}

uint32_t OpenGLPresentation::Height() const {
    return static_cast<uint32_t>(std::max(mHeight, 0));
}

void OpenGLPresentation::ApplySwapInterval() {
    using SwapIntervalProc = BOOL(WINAPI*)(int);
    auto swapInterval = reinterpret_cast<SwapIntervalProc>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (swapInterval != nullptr) {
        swapInterval(mVsyncEnabled ? 1 : 0);
    }
    mSwapIntervalApplied = true;
}

void OpenGLPresentation::SwapBuffersBegin() {
    const bool nextVsyncEnabled = mVariables.GetInteger(CVAR_VSYNC_ENABLED, 1) != 0;
    if (nextVsyncEnabled != mVsyncEnabled) {
        mVsyncEnabled = nextVsyncEnabled;
        ApplySwapInterval();
    }
    if (!mSwapIntervalApplied) {
        ApplySwapInterval();
    }
    if (!mVsyncEnabled && mTargetFps > 0 && mClockFrequency.QuadPart > 0) {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        const LONGLONG target = mClockFrequency.QuadPart / mTargetFps;
        const LONGLONG remaining = target - (now.QuadPart - mLastPresent.QuadPart);
        if (remaining > mClockFrequency.QuadPart / 1000 && mFrameTimer != nullptr) {
            LARGE_INTEGER due{};
            due.QuadPart = -(remaining * 10000000 / mClockFrequency.QuadPart);
            SetWaitableTimer(mFrameTimer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(mFrameTimer, INFINITE);
        }
    }
}

void OpenGLPresentation::SwapBuffersEnd() {
    Engine::Rendering::Overlay::Render(static_cast<uint32_t>(std::max(mWidth, 0)),
                                       static_cast<uint32_t>(std::max(mHeight, 0)));
    SwapBuffers(mDeviceContext);
    QueryPerformanceCounter(&mLastPresent);
    ++mTitleFrameCount;
    const LONGLONG titleElapsed = mLastPresent.QuadPart - mTitleSampleTime.QuadPart;
    if (mClockFrequency.QuadPart > 0 && titleElapsed >= mClockFrequency.QuadPart) {
        const double seconds = static_cast<double>(titleElapsed) / mClockFrequency.QuadPart;
        const double fps = mTitleFrameCount / seconds;
        const Engine::Rendering::Overlay::NetworkTelemetry network =
            Engine::Rendering::Overlay::GetNetworkTelemetry();
        char title[512];
        std::snprintf(title, sizeof(title), "%s | FPS: %.1f | Net In: %.2f KB/s Out: %.2f KB/s | Ping: %d ms",
                      mBaseTitle.c_str(), fps, network.inboundBytesPerSecond / 1024.0,
                      network.outboundBytesPerSecond / 1024.0, network.active ? network.latencyMilliseconds : 0);
        SetWindowTextA(mWindow, title);
        mTitleFrameCount = 0;
        mTitleSampleTime = mLastPresent;
    }
}

void OpenGLPresentation::SetTargetFps(int fps) {
    mTargetFps = std::max(fps, 1);
}

bool OpenGLPresentation::CanDisableVsync() const {
    return true;
}

#endif
