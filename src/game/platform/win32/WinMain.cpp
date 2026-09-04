#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>
#include <GL/gl.h>

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <engine/config/ConsoleVariable.h>
#include <platform/win32/Input.h>
#include <platform/win32/OpenGLPresentation.h>
#include <runtime/log/Log.h>
#include "platform/win32/App.h"
#include "platform/win32/SettingsWindow.h"

#include "multiplayer/NetworkVersion.h"
#include "platform/client/Application.h"
#include "platform/win32/resource.h"

Global App;
Input input;

namespace {

constexpr const char* WindowClassName = "GameClientWindow";
constexpr UINT_PTR MoveLoopTimer = 0x534F48;

void PumpMoveLoop() {
    if (App.client != nullptr) {
        App.client->PumpMoveLoop();
    }
}

std::filesystem::path ExecutableDirectory() {
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length == path.size()) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
}

void SetCursorVisible(bool visible) {
    int result;
    int attempts = 0;
    if (visible) {
        do {
            result = ShowCursor(TRUE);
        } while (result < 0 && ++attempts < 15);
    } else {
        do {
            result = ShowCursor(FALSE);
        } while (result >= 0 && ++attempts < 15);
    }
}

void SetMouseCaptured(HWND window, bool capture) {
    if (capture == App.mouseCaptured) {
        return;
    }
    App.mouseCaptured = capture;
    if (capture) {
        POINT topLeft{ 0, 0 };
        POINT bottomRight{ App.size.x, App.size.y };
        ClientToScreen(window, &topLeft);
        ClientToScreen(window, &bottomRight);
        const RECT screen{ topLeft.x, topLeft.y, bottomRight.x - 1, bottomRight.y - 1 };
        ClipCursor(&screen);
        SetCapture(window);
        SetCursorVisible(false);
    } else {
        ClipCursor(nullptr);
        ReleaseCapture();
        SetCursorVisible(true);
    }
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
        case WM_DESTROY:
            App.quit = true;
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                SetMouseCaptured(window, false);
                input.Clear();
            }
            return 0;
        case WM_ENTERSIZEMOVE:
            PumpMoveLoop();
            SetTimer(window, MoveLoopTimer, 16, nullptr);
            return 0;
        case WM_EXITSIZEMOVE:
            KillTimer(window, MoveLoopTimer);
            PumpMoveLoop();
            return 0;
        case WM_TIMER:
            if (wParam == MoveLoopTimer) {
                PumpMoveLoop();
                return 0;
            }
            return DefWindowProcA(window, message, wParam, lParam);
        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN) {
                ShowWindow(window, IsZoomed(window) ? SW_NORMAL : SW_MAXIMIZE);
            }
            if (wParam == VK_F4) {
                break;
            }
            input.KeyDown(static_cast<uint8_t>(wParam));
            return 0;
        case WM_SYSKEYUP:
            input.KeyUp(static_cast<uint8_t>(wParam));
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && !input.IsTextInputCaptured()) {
                if (App.settings != nullptr) {
                    App.settings->Toggle();
                }
                return 0;
            }
            input.KeyDown(static_cast<uint8_t>(wParam));
            return 0;
        case WM_KEYUP:
            input.KeyUp(static_cast<uint8_t>(wParam));
            return 0;
        case WM_CHAR:
            if (wParam >= 32 && wParam < 127) {
                input.TextInput(static_cast<uint8_t>(wParam));
            }
            return 0;
        case WM_MOUSEMOVE:
            input.mouse.x = GET_X_LPARAM(lParam);
            input.mouse.y = GET_Y_LPARAM(lParam);
            return 0;
        case WM_INPUT: {
            UINT size = 0;
            GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
            if (size != 0) {
                std::vector<uint8_t> data(size);
                if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, data.data(), &size,
                                    sizeof(RAWINPUTHEADER)) == size) {
                    const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(data.data());
                    if (raw->header.dwType == RIM_TYPEMOUSE) {
                        input.AddMouseDelta(raw->data.mouse.lLastX, raw->data.mouse.lLastY);
                    }
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            SetCapture(window);
            input.KeyDown(VK_LBUTTON);
            input.mouse.x = GET_X_LPARAM(lParam);
            input.mouse.y = GET_Y_LPARAM(lParam);
            return 0;
        case WM_LBUTTONUP:
            input.KeyUp(VK_LBUTTON);
            if (!App.mouseCaptured && !input.key[VK_RBUTTON]) {
                ReleaseCapture();
            }
            return 0;
        case WM_MBUTTONDOWN:
            input.KeyDown(VK_MBUTTON);
            SetCapture(window);
            return 0;
        case WM_MBUTTONUP:
            input.KeyUp(VK_MBUTTON);
            return 0;
        case WM_RBUTTONDOWN:
            SetCapture(window);
            input.KeyDown(VK_RBUTTON);
            input.mouse.x = GET_X_LPARAM(lParam);
            input.mouse.y = GET_Y_LPARAM(lParam);
            return 0;
        case WM_RBUTTONUP:
            input.KeyUp(VK_RBUTTON);
            if (!App.mouseCaptured && !input.key[VK_LBUTTON]) {
                ReleaseCapture();
            }
            return 0;
        case WM_XBUTTONDOWN: {
            const WORD button = GET_XBUTTON_WPARAM(wParam);
            input.KeyDown(button == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
            return TRUE;
        }
        case WM_XBUTTONUP: {
            const WORD button = GET_XBUTTON_WPARAM(wParam);
            input.KeyUp(button == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
            return TRUE;
        }
        case WM_MOUSEWHEEL:
            input.AddMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_SIZE:
            App.size = { LOWORD(lParam), HIWORD(lParam) };
            return 0;
        case WM_LBUTTONDBLCLK:
            break;
        case WM_ENDSESSION:
            if (wParam == TRUE) {
                App.quit = true;
            }
            return 0;
        default:
            return DefWindowProcA(window, message, wParam, lParam);
    }
    return 0;
}

bool PumpMessages(HWND gameWindow) {
    MSG message{};
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            App.quit = true;
            break;
        }
        if (App.settings != nullptr && App.settings->IsVisible() && message.message == WM_KEYDOWN &&
            message.wParam == VK_ESCAPE && App.settings->OwnsWindow(message.hwnd)) {
            App.settings->Toggle();
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    if (App.quit) {
        return false;
    }
    if (App.settings != nullptr && App.settings->IsVisible()) {
        App.settings->SyncNetworkStatus();
    }

    App.invertCameraY = App.client->ConsoleVariables().GetInteger("gSettings.CameraInvertY", 0) != 0;
    App.suppressWorldMouse = input.IsGameInputBlocked() || input.IsTextInputCaptured();
    const bool focused = GetForegroundWindow() == gameWindow;
    const bool capture = focused && (App.settings == nullptr || !App.settings->IsVisible()) &&
                         !App.suppressWorldMouse;
    SetMouseCaptured(gameWindow, capture);
    input.RefreshKeyboardState(focused);
    if (App.client != nullptr && input.ConsumePress(VK_F1)) {
        App.client->ToggleCollisionVisualization();
    }
    return true;
}

} // namespace

int PASCAL WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    ClearLog();

    auto client = std::make_unique<Application>(ExecutableDirectory(), input);
    App = {};
    App.client = client.get();
    const auto bounds = client->LoadWindowBounds();
    App.size = { static_cast<int32_t>(bounds.width), static_cast<int32_t>(bounds.height) };
    const std::string title = "v" + std::to_string(APP_PROTOCOL_VERSION);

    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_DBLCLKS;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(IDI_GAME_CLIENT));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = WindowClassName;
    windowClass.hIconSm = windowClass.hIcon;
    if (!RegisterClassExA(&windowClass)) {
        MessageBoxA(nullptr, "Failed to register the game window class.", "Startup error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    RECT frame{ 0, 0, static_cast<LONG>(bounds.width), static_cast<LONG>(bounds.height) };
    AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExA(0, WindowClassName, title.c_str(), WS_OVERLAPPEDWINDOW, bounds.x, bounds.y,
                                  frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, instance,
                                  nullptr);
    if (window == nullptr) {
        UnregisterClassA(WindowClassName, instance);
        MessageBoxA(nullptr, "Failed to create the game window.", "Startup error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    HDC deviceContext = GetDC(window);
    PIXELFORMATDESCRIPTOR pixelFormat{};
    pixelFormat.nSize = sizeof(pixelFormat);
    pixelFormat.nVersion = 1;
    pixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormat.iPixelType = PFD_TYPE_RGBA;
    pixelFormat.cColorBits = 32;
    pixelFormat.cRedBits = 8;
    pixelFormat.cGreenBits = 8;
    pixelFormat.cBlueBits = 8;
    pixelFormat.cAlphaBits = 8;
    pixelFormat.cDepthBits = 24;
    pixelFormat.cStencilBits = 8;
    pixelFormat.iLayerType = PFD_MAIN_PLANE;
    const int format = ChoosePixelFormat(deviceContext, &pixelFormat);
    HGLRC renderContext = nullptr;
    if (format != 0 && SetPixelFormat(deviceContext, format, &pixelFormat)) {
        renderContext = wglCreateContext(deviceContext);
    }
    if (renderContext == nullptr || !wglMakeCurrent(deviceContext, renderContext)) {
        if (renderContext != nullptr) {
            wglDeleteContext(renderContext);
        }
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        UnregisterClassA(WindowClassName, instance);
        MessageBoxA(nullptr, "Failed to create the OpenGL context.", "Startup error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    RAWINPUTDEVICE rawMouse{};
    rawMouse.usUsagePage = 0x01;
    rawMouse.usUsage = 0x02;
    rawMouse.hwndTarget = window;
    RegisterRawInputDevices(&rawMouse, 1, sizeof(rawMouse));

    auto presentation = std::make_unique<OpenGLPresentation>(window, deviceContext, App.size.x, App.size.y,
                                                             client->ConsoleVariables(), title);
    client->AttachPresentation(*presentation);
    client->Start();
    auto settingsWindow = std::make_unique<SettingsWindow>(
        instance, window, client->ConsoleVariables(), input, client->MultiplayerInteraction());
    settingsWindow->Init();
    App.settings = settingsWindow.get();

    ShowWindow(window, showCommand == SW_SHOWMAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    UpdateWindow(window);
    while (PumpMessages(window)) {
        client->RunFrame();
        input.EndFrame();
    }

    RECT windowBounds{};
    if (GetWindowRect(window, &windowBounds) && App.size.x > 0 && App.size.y > 0) {
        client->SaveWindowBounds({
            static_cast<uint32_t>(App.size.x),
            static_cast<uint32_t>(App.size.y),
            windowBounds.left,
            windowBounds.top,
        });
    }

    App.settings = nullptr;
    settingsWindow.reset();
    client->Shutdown();
    App.client = nullptr;
    SetMouseCaptured(window, false);
    client.reset();
    presentation.reset();

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(renderContext);
    ReleaseDC(window, deviceContext);
    DestroyWindow(window);
    UnregisterClassA(WindowClassName, instance);
    return EXIT_SUCCESS;
}
