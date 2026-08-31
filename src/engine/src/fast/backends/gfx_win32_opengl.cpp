#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <GL/gl.h>

#include "fast/backends/gfx_win32_opengl.h"
#include "engine/Context.h"
#include "engine/config/ConsoleVariable.h"
#include "engine/input/Win32Input.h"
#include "engine/window/Overlay.h"
#include "engine/window/FileDropMgr.h"
#include "engine/window/Window.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace Fast {

static constexpr const char* WindowClassName = "Win32OpenGL";
static constexpr const char* SettingsWindowClassName = "Win32Settings";
static constexpr int FullscreenControl = 1001;
static constexpr int VsyncControl = 1002;
static constexpr int FpsControl = 1003;
static constexpr int NetworkAddressControl = 1101;
static constexpr int NetworkPortControl = 1102;
static constexpr int NetworkHostControl = 1103;
static constexpr int NetworkConnectControl = 1104;
static constexpr int NetworkDisconnectControl = 1105;
static constexpr int NetworkChatControl = 1106;
static constexpr int NetworkVoiceControl = 1107;
static constexpr int NetworkPushToTalkControl = 1108;
static constexpr int CameraInvertYControl = 1109;
static constexpr UINT_PTR MoveLoopTimer = 0x534F48;
static constexpr int FpsValues[] = { 20, 30, 60, 120, 144, 240 };

static LRESULT CALLBACK Win32OpenGLWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<GfxWindowBackendWin32OpenGL*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
        self = static_cast<GfxWindowBackendWin32OpenGL*>(create->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr ? self->WindowProc(hwnd, message, wParam, lParam)
                           : DefWindowProcA(hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK Win32SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<GfxWindowBackendWin32OpenGL*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
        self = static_cast<GfxWindowBackendWin32OpenGL*>(create->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr ? self->SettingsWindowProc(hwnd, message, wParam, lParam)
                           : DefWindowProcA(hwnd, message, wParam, lParam);
}

void GfxWindowBackendWin32OpenGL::Init(const char* gameName, const char*, bool startFullScreen,
                                       uint32_t width, uint32_t height, int32_t posX, int32_t posY) {
    mWidth = width;
    mHeight = height;
    mPosX = posX;
    mPosY = posY;
    mTargetFps = 60;
    QueryPerformanceFrequency(&mClockFrequency);
    QueryPerformanceCounter(&mClockStart);
    mLastPresent = mClockStart;
    mTitleSampleTime = mClockStart;

    mInstance = GetModuleHandleA(nullptr);
    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_DBLCLKS;
    windowClass.lpfnWndProc = Win32OpenGLWindowProc;
    windowClass.hInstance = mInstance;
    windowClass.hIcon = LoadIconA(mInstance, MAKEINTRESOURCEA(1));
    windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = WindowClassName;
    windowClass.hIconSm = windowClass.hIcon;
    if (!RegisterClassExA(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return;
    }

    mBaseTitle = gameName;
    RECT windowRect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    mWindow = CreateWindowExA(0, WindowClassName, mBaseTitle.c_str(), WS_OVERLAPPEDWINDOW, posX, posY,
                              windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr,
                              mInstance, this);
    if (mWindow == nullptr) {
        return;
    }
    ShowWindow(mWindow, SW_SHOWNORMAL);

    mDeviceContext = GetDC(mWindow);
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
    const int format = ChoosePixelFormat(mDeviceContext, &pixelFormat);
    if (format == 0 || !SetPixelFormat(mDeviceContext, format, &pixelFormat)) {
        Close();
        return;
    }
    mRenderContext = wglCreateContext(mDeviceContext);
    if (mRenderContext == nullptr || !wglMakeCurrent(mDeviceContext, mRenderContext)) {
        Close();
        return;
    }

    DragAcceptFiles(mWindow, TRUE);
    RAWINPUTDEVICE rawMouse{};
    rawMouse.usUsagePage = 0x01;
    rawMouse.usUsage = 0x02;
    rawMouse.dwFlags = 0;
    rawMouse.hwndTarget = mWindow;
    RegisterRawInputDevices(&rawMouse, 1, sizeof(rawMouse));
    mFrameTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (mFrameTimer == nullptr) {
        mFrameTimer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    if (startFullScreen) {
        SetFullscreen(true);
    }
    CreateSettingsWindow();
    UpdateWindow(mWindow);
    UpdateClientSize();
}

LRESULT GfxWindowBackendWin32OpenGL::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
        case WM_DESTROY:
            Close();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                SetMouseCapture(false);
                Engine::GetWin32Input().Clear();
                if (mOnAllKeysUp != nullptr) {
                    mOnAllKeysUp();
                }
            }
            return 0;
        case WM_ENTERSIZEMOVE:
            Engine::Overlay::PumpMoveLoop();
            SetTimer(hwnd, MoveLoopTimer, 16, nullptr);
            return 0;
        case WM_EXITSIZEMOVE:
            KillTimer(hwnd, MoveLoopTimer);
            Engine::Overlay::PumpMoveLoop();
            return 0;
        case WM_TIMER:
            if (wParam == MoveLoopTimer) {
                Engine::Overlay::PumpMoveLoop();
                return 0;
            }
            return DefWindowProcA(hwnd, message, wParam, lParam);
        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN) {
                SetFullscreen(!IsZoomed(hwnd));
            }
            if (wParam != VK_F4) {
                Engine::GetWin32Input().KeyDown(static_cast<uint8_t>(wParam));
                KeyDown(lParam);
            }
            return 0;
        case WM_SYSKEYUP:
            Engine::GetWin32Input().KeyUp(static_cast<uint8_t>(wParam));
            KeyUp(lParam);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (Engine::GetWin32Input().IsTextInputCaptured()) {
                    Engine::GetWin32Input().KeyDown(VK_ESCAPE);
                    KeyDown(lParam);
                } else {
                    ToggleSettingsWindow();
                }
                return 0;
            }
            if (wParam == VK_F11) {
                SetFullscreen(!mFullScreen);
                return 0;
            }
            Engine::GetWin32Input().KeyDown(static_cast<uint8_t>(wParam));
            KeyDown(lParam);
            return 0;
        case WM_KEYUP:
            Engine::GetWin32Input().KeyUp(static_cast<uint8_t>(wParam));
            KeyUp(lParam);
            return 0;
        case WM_CHAR:
            if (wParam <= UINT8_MAX) {
                Engine::GetWin32Input().TextInput(static_cast<uint8_t>(wParam));
            }
            return 0;
        case WM_MOUSEMOVE:
            Engine::GetWin32Input().mouse.x = GET_X_LPARAM(lParam);
            Engine::GetWin32Input().mouse.y = GET_Y_LPARAM(lParam);
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
                        Engine::GetWin32Input().AddMouseDelta(raw->data.mouse.lLastX, raw->data.mouse.lLastY);
                    }
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            Engine::GetWin32Input().KeyDown(VK_LBUTTON);
            SetCapture(hwnd);
            MouseDown(0);
            return 0;
        case WM_LBUTTONUP:
            Engine::GetWin32Input().KeyUp(VK_LBUTTON);
            if (!mMouseCaptured && !mMouseButtons[2]) ReleaseCapture();
            MouseUp(0);
            return 0;
        case WM_MBUTTONDOWN:
            Engine::GetWin32Input().KeyDown(VK_MBUTTON);
            SetCapture(hwnd);
            MouseDown(1);
            return 0;
        case WM_MBUTTONUP:
            Engine::GetWin32Input().KeyUp(VK_MBUTTON);
            MouseUp(1);
            return 0;
        case WM_RBUTTONDOWN:
            Engine::GetWin32Input().KeyDown(VK_RBUTTON);
            SetCapture(hwnd);
            MouseDown(2);
            return 0;
        case WM_RBUTTONUP:
            Engine::GetWin32Input().KeyUp(VK_RBUTTON);
            if (!mMouseCaptured && !mMouseButtons[0]) ReleaseCapture();
            MouseUp(2);
            return 0;
        case WM_XBUTTONDOWN:
            Engine::GetWin32Input().KeyDown(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
            MouseDown(2 + GET_XBUTTON_WPARAM(wParam));
            return TRUE;
        case WM_XBUTTONUP:
            Engine::GetWin32Input().KeyUp(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
            MouseUp(2 + GET_XBUTTON_WPARAM(wParam));
            return TRUE;
        case WM_MOUSEWHEEL:
            Engine::GetWin32Input().AddMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            mMouseWheelY += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            return 0;
        case WM_MOUSEHWHEEL:
            mMouseWheelX += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            return 0;
        case WM_SIZE:
            mWidth = LOWORD(lParam);
            mHeight = HIWORD(lParam);
            return 0;
        case WM_MOVE:
            mPosX = GET_X_LPARAM(lParam);
            mPosY = GET_Y_LPARAM(lParam);
            return 0;
        case WM_DROPFILES: {
            char fileName[MAX_PATH]{};
            DragQueryFileA(reinterpret_cast<HDROP>(wParam), 0, fileName, MAX_PATH);
            Engine::Context::GetInstance()->GetFileDropMgr()->SetDroppedFile(fileName);
            DragFinish(reinterpret_cast<HDROP>(wParam));
            return 0;
        }
        case WM_ENDSESSION:
            if (wParam == TRUE) Close();
            return 0;
        default:
            return DefWindowProcA(hwnd, message, wParam, lParam);
    }
}

void GfxWindowBackendWin32OpenGL::CreateSettingsWindow() {
    WNDCLASSEXA settingsClass{};
    settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.lpfnWndProc = Win32SettingsWindowProc;
    settingsClass.hInstance = mInstance;
    settingsClass.hIcon = LoadIconA(mInstance, MAKEINTRESOURCEA(1));
    settingsClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    settingsClass.lpszClassName = SettingsWindowClassName;
    RegisterClassExA(&settingsClass);

    mSettingsWindow = CreateWindowExA(WS_EX_TOOLWINDOW, SettingsWindowClassName, "Game Settings",
                                      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 520, 470,
                                      mWindow, nullptr, mInstance, this);
    if (mSettingsWindow == nullptr) {
        return;
    }

    CreateWindowExA(0, "STATIC", "Display", WS_CHILD | WS_VISIBLE, 20, 18, 320, 22, mSettingsWindow, nullptr,
                    mInstance, nullptr);
    mFullscreenCheck = CreateWindowExA(0, "BUTTON", "Fullscreen", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 50,
                                       150, 24, mSettingsWindow, reinterpret_cast<HMENU>(FullscreenControl), mInstance,
                                       nullptr);
    mVsyncCheck = CreateWindowExA(0, "BUTTON", "Vertical sync", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 190, 50,
                                  150, 24, mSettingsWindow, reinterpret_cast<HMENU>(VsyncControl), mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Interpolation rate", WS_CHILD | WS_VISIBLE, 20, 94, 150, 22, mSettingsWindow,
                    nullptr, mInstance, nullptr);
    mFpsCombo = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 190, 90,
                                150, 180, mSettingsWindow, reinterpret_cast<HMENU>(FpsControl), mInstance, nullptr);
    for (int fps : FpsValues) {
        const std::string label = std::to_string(fps) + " FPS";
        SendMessageA(mFpsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    CreateWindowExA(0, "STATIC", "Multiplayer", WS_CHILD | WS_VISIBLE, 20, 135, 460, 22, mSettingsWindow, nullptr,
                    mInstance, nullptr);
    mNetworkStatusLabel = CreateWindowExA(0, "STATIC", "Offline", WS_CHILD | WS_VISIBLE, 20, 162, 460, 22,
                                          mSettingsWindow, nullptr, mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Address", WS_CHILD | WS_VISIBLE, 20, 198, 70, 22, mSettingsWindow, nullptr,
                    mInstance, nullptr);
    mNetworkAddressEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "127.0.0.1", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                          92, 194, 245, 25, mSettingsWindow,
                                          reinterpret_cast<HMENU>(NetworkAddressControl), mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Port", WS_CHILD | WS_VISIBLE, 350, 198, 42, 22, mSettingsWindow, nullptr, mInstance,
                    nullptr);
    mNetworkPortEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "777",
                                       WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 395, 194, 85, 25,
                                       mSettingsWindow, reinterpret_cast<HMENU>(NetworkPortControl), mInstance,
                                       nullptr);
    SendMessageA(mNetworkAddressEdit, EM_SETLIMITTEXT, 255, 0);
    SendMessageA(mNetworkPortEdit, EM_SETLIMITTEXT, 5, 0);
    CreateWindowExA(0, "BUTTON", "Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 232, 145, 28, mSettingsWindow,
                    reinterpret_cast<HMENU>(NetworkHostControl), mInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 177, 232, 145, 28,
                    mSettingsWindow, reinterpret_cast<HMENU>(NetworkConnectControl), mInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Disconnect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 335, 232, 145, 28,
                    mSettingsWindow, reinterpret_cast<HMENU>(NetworkDisconnectControl), mInstance, nullptr);
    mChatCheck = CreateWindowExA(0, "BUTTON", "Show text chat", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 276,
                                 200, 24, mSettingsWindow, reinterpret_cast<HMENU>(NetworkChatControl), mInstance,
                                 nullptr);
    mVoiceCheck = CreateWindowExA(0, "BUTTON", "Voice chat", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 276,
                                  200, 24, mSettingsWindow, reinterpret_cast<HMENU>(NetworkVoiceControl), mInstance,
                                  nullptr);
    mPushToTalkCheck = CreateWindowExA(0, "BUTTON", "Push to talk (Shift)",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 308, 200, 24, mSettingsWindow,
                                       reinterpret_cast<HMENU>(NetworkPushToTalkControl), mInstance, nullptr);
    mCameraInvertYCheck = CreateWindowExA(0, "BUTTON", "Invert camera Y",
                                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 308, 200, 24,
                                          mSettingsWindow, reinterpret_cast<HMENU>(CameraInvertYControl), mInstance,
                                          nullptr);
    CreateWindowExA(0, "STATIC", "Enter opens chat.  / opens multiplayer commands.", WS_CHILD | WS_VISIBLE, 20,
                    354, 460, 22, mSettingsWindow, nullptr, mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Press Escape to return to the game.", WS_CHILD | WS_VISIBLE, 20, 390, 460, 24,
                    mSettingsWindow, nullptr, mInstance, nullptr);
    SyncSettingsControls();
}

void GfxWindowBackendWin32OpenGL::SyncSettingsControls() {
    if (mSettingsWindow == nullptr) {
        return;
    }
    auto variables = Engine::Context::GetInstance()->GetConsoleVariables();
    SendMessageA(mFullscreenCheck, BM_SETCHECK, mFullScreen ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(mVsyncCheck, BM_SETCHECK,
                 variables->GetInteger(CVAR_VSYNC_ENABLED, 1) ? BST_CHECKED : BST_UNCHECKED, 0);
    const int fps = variables->GetInteger("gSettings.InterpolationFPS", 20);
    int selection = 0;
    for (int i = 0; i < static_cast<int>(std::size(FpsValues)); ++i) {
        if (FpsValues[i] == fps) {
            selection = i;
            break;
        }
    }
    SendMessageA(mFpsCombo, CB_SETCURSEL, selection, 0);
    const std::string address = variables->GetString("gSettings.MultiplayerAddress", "127.0.0.1");
    SetWindowTextA(mNetworkAddressEdit, address.c_str());
    const std::string port = std::to_string(std::clamp(variables->GetInteger("gSettings.MultiplayerPort", 777), 1,
                                                       49151));
    SetWindowTextA(mNetworkPortEdit, port.c_str());
    SendMessageA(mChatCheck, BM_SETCHECK,
                 variables->GetInteger("gSettings.MultiplayerChatEnabled", 1) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(mVoiceCheck, BM_SETCHECK,
                 variables->GetInteger("gSettings.MultiplayerVoiceEnabled", 1) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(mPushToTalkCheck, BM_SETCHECK,
                 variables->GetInteger("gSettings.MultiplayerVoicePushToTalk", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(mCameraInvertYCheck, BM_SETCHECK,
                 variables->GetInteger("gSettings.CameraInvertY", 0) ? BST_CHECKED : BST_UNCHECKED, 0);
    SyncNetworkStatus();
}

void GfxWindowBackendWin32OpenGL::SyncNetworkStatus() {
    if (mNetworkStatusLabel == nullptr) {
        return;
    }
    const std::string status = Engine::Context::GetInstance()->GetConsoleVariables()->GetString(
        "gSettings.MultiplayerStatus", "Offline");
    SetWindowTextA(mNetworkStatusLabel, status.c_str());
}

void GfxWindowBackendWin32OpenGL::ToggleSettingsWindow() {
    if (mSettingsWindow == nullptr) {
        return;
    }
    mSettingsVisible = !mSettingsVisible;
    Engine::GetWin32Input().SetGameInputBlocked(mSettingsVisible);
    ShowWindow(mSettingsWindow, mSettingsVisible ? SW_SHOWNORMAL : SW_HIDE);
    if (mSettingsVisible) {
        SyncSettingsControls();
        SetForegroundWindow(mSettingsWindow);
    } else {
        SetForegroundWindow(mWindow);
        SetFocus(mWindow);
    }
}

LRESULT GfxWindowBackendWin32OpenGL::SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
            ToggleSettingsWindow();
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ToggleSettingsWindow();
                return 0;
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == FullscreenControl && HIWORD(wParam) == BN_CLICKED) {
                SetFullscreen(SendMessageA(mFullscreenCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
            } else if (LOWORD(wParam) == VsyncControl && HIWORD(wParam) == BN_CLICKED) {
                auto variables = Engine::Context::GetInstance()->GetConsoleVariables();
                variables->SetInteger(CVAR_VSYNC_ENABLED,
                                      SendMessageA(mVsyncCheck, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
                variables->Save();
            } else if (LOWORD(wParam) == FpsControl && HIWORD(wParam) == CBN_SELCHANGE) {
                const LRESULT selection = SendMessageA(mFpsCombo, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && selection < static_cast<LRESULT>(std::size(FpsValues))) {
                    auto variables = Engine::Context::GetInstance()->GetConsoleVariables();
                    variables->SetInteger("gSettings.MatchRefreshRate", 0);
                    variables->SetInteger("gSettings.InterpolationFPS", FpsValues[selection]);
                    variables->Save();
                }
            } else if (HIWORD(wParam) == BN_CLICKED &&
                       (LOWORD(wParam) == NetworkChatControl || LOWORD(wParam) == NetworkVoiceControl ||
                        LOWORD(wParam) == NetworkPushToTalkControl || LOWORD(wParam) == CameraInvertYControl)) {
                auto variables = Engine::Context::GetInstance()->GetConsoleVariables();
                HWND check = mChatCheck;
                const char* name = "gSettings.MultiplayerChatEnabled";
                if (LOWORD(wParam) == NetworkVoiceControl) {
                    check = mVoiceCheck;
                    name = "gSettings.MultiplayerVoiceEnabled";
                } else if (LOWORD(wParam) == NetworkPushToTalkControl) {
                    check = mPushToTalkCheck;
                    name = "gSettings.MultiplayerVoicePushToTalk";
                } else if (LOWORD(wParam) == CameraInvertYControl) {
                    check = mCameraInvertYCheck;
                    name = "gSettings.CameraInvertY";
                }
                variables->SetInteger(name, SendMessageA(check, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
                variables->Save();
            } else if (HIWORD(wParam) == BN_CLICKED &&
                       (LOWORD(wParam) == NetworkHostControl || LOWORD(wParam) == NetworkConnectControl ||
                        LOWORD(wParam) == NetworkDisconnectControl)) {
                auto variables = Engine::Context::GetInstance()->GetConsoleVariables();
                char address[256]{};
                char portText[16]{};
                GetWindowTextA(mNetworkAddressEdit, address, static_cast<int>(std::size(address)));
                GetWindowTextA(mNetworkPortEdit, portText, static_cast<int>(std::size(portText)));
                const int port = std::clamp(std::atoi(portText), 1, 49151);
                variables->SetString("gSettings.MultiplayerAddress", address[0] ? address : "127.0.0.1");
                variables->SetInteger("gSettings.MultiplayerPort", port);
                variables->SetInteger("gSettings.MultiplayerAction", LOWORD(wParam) == NetworkHostControl      ? 1
                                                                   : LOWORD(wParam) == NetworkConnectControl ? 2
                                                                                                               : 3);
                variables->Save();
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

void GfxWindowBackendWin32OpenGL::KeyDown(LPARAM lParam) {
    if (mOnKeyDown != nullptr) mOnKeyDown(static_cast<int>((lParam >> 16) & 0x1ff));
}
void GfxWindowBackendWin32OpenGL::KeyUp(LPARAM lParam) {
    if (mOnKeyUp != nullptr) mOnKeyUp(static_cast<int>((lParam >> 16) & 0x1ff));
}
void GfxWindowBackendWin32OpenGL::MouseDown(int button) {
    if (button < 0 || button >= static_cast<int>(mMouseButtons.size())) return;
    mMouseButtons[button] = true;
    if (mOnMouseButtonDown != nullptr) mOnMouseButtonDown(button);
}
void GfxWindowBackendWin32OpenGL::MouseUp(int button) {
    if (button < 0 || button >= static_cast<int>(mMouseButtons.size())) return;
    mMouseButtons[button] = false;
    if (mOnMouseButtonUp != nullptr) mOnMouseButtonUp(button);
}

void GfxWindowBackendWin32OpenGL::Close() { mIsRunning = false; }
void GfxWindowBackendWin32OpenGL::SetKeyboardCallbacks(bool (*down)(int), bool (*up)(int), void (*allUp)()) {
    mOnKeyDown = down; mOnKeyUp = up; mOnAllKeysUp = allUp;
}
void GfxWindowBackendWin32OpenGL::SetMouseCallbacks(bool (*down)(int), bool (*up)(int)) {
    mOnMouseButtonDown = down; mOnMouseButtonUp = up;
}
void GfxWindowBackendWin32OpenGL::SetFullscreenChangedCallback(void (*changed)(bool)) { mOnFullscreenChanged = changed; }
void GfxWindowBackendWin32OpenGL::SetFullscreen(bool fullscreen) {
    if (mWindow == nullptr || fullscreen == mFullScreen) return;
    ShowWindow(mWindow, fullscreen ? SW_MAXIMIZE : SW_NORMAL);
    mFullScreen = fullscreen;
    if (mOnFullscreenChanged != nullptr) mOnFullscreenChanged(fullscreen);
}
void GfxWindowBackendWin32OpenGL::GetActiveWindowRefreshRate(uint32_t* refreshRate) {
    DEVMODEA mode{}; mode.dmSize = sizeof(mode);
    *refreshRate = EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1
                       ? mode.dmDisplayFrequency : 60;
}
void GfxWindowBackendWin32OpenGL::SetCursorVisibility(bool visible) {
    int value;
    int attempts = 0;
    if (visible) do { value = ShowCursor(TRUE); } while (value < 0 && ++attempts < 15);
    else do { value = ShowCursor(FALSE); } while (value >= 0 && ++attempts < 15);
}
void GfxWindowBackendWin32OpenGL::SetMousePos(int32_t x, int32_t y) {
    POINT point{ x, y }; ClientToScreen(mWindow, &point); SetCursorPos(point.x, point.y);
}
void GfxWindowBackendWin32OpenGL::GetMousePos(int32_t* x, int32_t* y) {
    POINT point{}; GetCursorPos(&point); ScreenToClient(mWindow, &point); *x = point.x; *y = point.y;
}
void GfxWindowBackendWin32OpenGL::GetMouseDelta(int32_t* x, int32_t* y) {
    POINT point{}; GetCursorPos(&point); ScreenToClient(mWindow, &point);
    *x = mHavePreviousMouse ? point.x - mPreviousMouse.x : 0;
    *y = mHavePreviousMouse ? point.y - mPreviousMouse.y : 0;
    mPreviousMouse = point; mHavePreviousMouse = true;
}
void GfxWindowBackendWin32OpenGL::GetMouseWheel(float* x, float* y) {
    *x = mMouseWheelX; *y = mMouseWheelY; mMouseWheelX = 0.0f; mMouseWheelY = 0.0f;
}
bool GfxWindowBackendWin32OpenGL::GetMouseState(uint32_t btn) {
    return btn < mMouseButtons.size() && mMouseButtons[btn];
}
void GfxWindowBackendWin32OpenGL::SetMouseCapture(bool capture) {
    mMouseCaptured = capture;
    if (capture) {
        RECT rect{}; GetClientRect(mWindow, &rect);
        POINT topLeft{ rect.left, rect.top }, bottomRight{ rect.right, rect.bottom };
        ClientToScreen(mWindow, &topLeft); ClientToScreen(mWindow, &bottomRight);
        RECT screenRect{ topLeft.x, topLeft.y, bottomRight.x - 1, bottomRight.y - 1 };
        ClipCursor(&screenRect); SetCapture(mWindow); SetCursorVisibility(false);
    } else {
        ClipCursor(nullptr); ReleaseCapture(); SetCursorVisibility(true); mHavePreviousMouse = false;
    }
}
bool GfxWindowBackendWin32OpenGL::IsMouseCaptured() { return mMouseCaptured; }
void GfxWindowBackendWin32OpenGL::UpdateClientSize() {
    RECT client{}; GetClientRect(mWindow, &client); mWidth = client.right - client.left; mHeight = client.bottom - client.top;
}
void GfxWindowBackendWin32OpenGL::GetDimensions(uint32_t* width, uint32_t* height, int32_t* x, int32_t* y) {
    UpdateClientSize(); *width = mWidth; *height = mHeight; *x = mPosX; *y = mPosY;
}
void GfxWindowBackendWin32OpenGL::HandleEvents() {
    MSG message{};
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) { Close(); break; }
        if (mSettingsVisible && message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE &&
            (message.hwnd == mSettingsWindow || IsChild(mSettingsWindow, message.hwnd))) {
            ToggleSettingsWindow();
            continue;
        }
        TranslateMessage(&message); DispatchMessageA(&message);
    }
    if (mSettingsVisible) {
        SyncNetworkStatus();
    }
    const bool gameWindowFocused = GetForegroundWindow() == mWindow;
    const bool shouldCaptureMouse = gameWindowFocused && !mSettingsVisible &&
                                    !Engine::GetWin32Input().IsGameInputBlocked() &&
                                    !Engine::GetWin32Input().IsTextInputCaptured();
    if (shouldCaptureMouse != mMouseCaptured) {
        // Own and clip the pointer while gameplay has focus so raw mouse aim
        // cannot leave the client and deliver weapon clicks to another app.
        SetMouseCapture(shouldCaptureMouse);
    }
    Engine::GetWin32Input().RefreshKeyboardState(gameWindowFocused);
}
bool GfxWindowBackendWin32OpenGL::IsFrameReady() { return true; }
void GfxWindowBackendWin32OpenGL::ApplySwapInterval() {
    using SwapIntervalProc = BOOL(WINAPI*)(int);
    auto swapInterval = reinterpret_cast<SwapIntervalProc>(wglGetProcAddress("wglSwapIntervalEXT"));
    if (swapInterval != nullptr) swapInterval(mVsyncEnabled ? 1 : 0);
    mSwapIntervalApplied = true;
}
void GfxWindowBackendWin32OpenGL::SwapBuffersBegin() {
    const bool nextVsyncEnabled =
        Engine::Context::GetInstance()->GetConsoleVariables()->GetInteger(CVAR_VSYNC_ENABLED, 1) != 0;
    if (nextVsyncEnabled != mVsyncEnabled) {
        mVsyncEnabled = nextVsyncEnabled;
        ApplySwapInterval();
    }
    if (!mSwapIntervalApplied) ApplySwapInterval();
    if (!mVsyncEnabled && mTargetFps > 0 && mClockFrequency.QuadPart > 0) {
        LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
        const LONGLONG target = mClockFrequency.QuadPart / mTargetFps;
        const LONGLONG remaining = target - (now.QuadPart - mLastPresent.QuadPart);
        if (remaining > mClockFrequency.QuadPart / 1000 && mFrameTimer != nullptr) {
            LARGE_INTEGER due{}; due.QuadPart = -(remaining * 10000000 / mClockFrequency.QuadPart);
            SetWaitableTimer(mFrameTimer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(mFrameTimer, INFINITE);
        }
    }
}
void GfxWindowBackendWin32OpenGL::SwapBuffersEnd() {
    // FinishRender runs between SwapBuffersBegin and SwapBuffersEnd. Draw the
    // native Game UI last so the scene cannot composite over the text.
    Engine::Overlay::Render(mWidth, mHeight);
    SwapBuffers(mDeviceContext);
    QueryPerformanceCounter(&mLastPresent);
    ++mTitleFrameCount;
    const LONGLONG titleElapsed = mLastPresent.QuadPart - mTitleSampleTime.QuadPart;
    if (mWindow != nullptr && mClockFrequency.QuadPart > 0 && titleElapsed >= mClockFrequency.QuadPart) {
        const double seconds = static_cast<double>(titleElapsed) / mClockFrequency.QuadPart;
        const double fps = mTitleFrameCount / seconds;
        const Engine::Overlay::NetworkTelemetry network = Engine::Overlay::GetNetworkTelemetry();
        char title[512];
        std::snprintf(title, sizeof(title), "%s | FPS: %.1f | Net In: %.2f KB/s Out: %.2f KB/s | Ping: %d ms",
                      mBaseTitle.c_str(), fps, network.inboundBytesPerSecond / 1024.0,
                      network.outboundBytesPerSecond / 1024.0, network.active ? network.latencyMilliseconds : 0);
        SetWindowTextA(mWindow, title);
        mTitleFrameCount = 0;
        mTitleSampleTime = mLastPresent;
    }
}
double GfxWindowBackendWin32OpenGL::GetTime() {
    LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
    return mClockFrequency.QuadPart ? static_cast<double>(now.QuadPart - mClockStart.QuadPart) / mClockFrequency.QuadPart : 0.0;
}
int GfxWindowBackendWin32OpenGL::GetTargetFps() { return static_cast<int>(mTargetFps); }
void GfxWindowBackendWin32OpenGL::SetTargetFps(int fps) { mTargetFps = std::max(fps, 1); }
void GfxWindowBackendWin32OpenGL::SetMaxFrameLatency(int) {}
const char* GfxWindowBackendWin32OpenGL::GetKeyName(int scancode) {
    char name[128]{};
    if (GetKeyNameTextA(scancode << 16, name, sizeof(name)) > 0) mKeyName = name;
    else mKeyName = "Unknown";
    return mKeyName.c_str();
}
bool GfxWindowBackendWin32OpenGL::CanDisableVsync() { return true; }
bool GfxWindowBackendWin32OpenGL::IsRunning() { return mIsRunning; }
void GfxWindowBackendWin32OpenGL::Destroy() {
    Engine::GetWin32Input().SetGameInputBlocked(false);
    Engine::Overlay::Clear();
    if (mSettingsWindow != nullptr) { DestroyWindow(mSettingsWindow); mSettingsWindow = nullptr; }
    if (mRenderContext != nullptr) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(mRenderContext); mRenderContext = nullptr; }
    if (mDeviceContext != nullptr && mWindow != nullptr) { ReleaseDC(mWindow, mDeviceContext); mDeviceContext = nullptr; }
    if (mWindow != nullptr) { DestroyWindow(mWindow); mWindow = nullptr; }
    if (mInstance != nullptr) UnregisterClassA(WindowClassName, mInstance);
    if (mInstance != nullptr) UnregisterClassA(SettingsWindowClassName, mInstance);
    if (mFrameTimer != nullptr) { CloseHandle(mFrameTimer); mFrameTimer = nullptr; }
}
bool GfxWindowBackendWin32OpenGL::IsFullscreen() { return mFullScreen; }

} // namespace Fast

#endif
