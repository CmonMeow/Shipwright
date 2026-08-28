#include <runtime/log/Log.hpp>
#include "engine/window/Window.h"
#include <string>
#include <fstream>
#include <iostream>
#include "engine/Context.h"
#include "engine/config/Config.h"

#ifdef __APPLE__
#include "engine/utils/AppleFolderManager.h"
#endif

namespace Engine {

Window::Window() {
    mConfig = Context::GetInstance()->GetConfig();
}

Window::~Window() {
    WriteLog("destruct window");
}

void Window::ToggleFullscreen() {
    SetFullscreen(!IsFullscreen());
}

float Window::GetCurrentAspectRatio() {
    return (float)GetWidth() / (float)GetHeight();
}

void Window::SaveWindowToConfig() {
    // This accepts conf in because it can be run in the destruction of LUS.
    mConfig->SetBool("Window.Fullscreen.Enabled", IsFullscreen());
    if (IsFullscreen()) {
        mConfig->SetInt("Window.Fullscreen.Width", (int32_t)GetWidth());
        mConfig->SetInt("Window.Fullscreen.Height", (int32_t)GetHeight());
    } else {
        mConfig->SetInt("Window.Width", (int32_t)GetWidth());
        mConfig->SetInt("Window.Height", (int32_t)GetHeight());
        mConfig->SetInt("Window.PositionX", GetPosX());
        mConfig->SetInt("Window.PositionY", GetPosY());
    }
}

bool Window::ShouldAutoCaptureMouse() {
    return mAutoCaptureMouse;
}

void Window::SetAutoCaptureMouse(bool capture) {
    mAutoCaptureMouse = capture;
}

bool Window::ShouldForceCursorVisibility() {
    return mForceCursorVisibility;
}

void Window::SetForceCursorVisibility(bool visible) {
    mForceCursorVisibility = visible;
}

} // namespace Engine
