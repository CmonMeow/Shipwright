#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform/win32/SettingsWindow.h"

#include "engine/config/ConsoleVariable.h"
#include "platform/client/MultiplayerInteractionPort.h"
#include "platform/win32/Input.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>

static constexpr const char* SettingsWindowClassName = "Win32Settings";
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
static constexpr int FpsValues[] = { 20, 30, 60, 120, 144, 240 };

static HMENU ControlId(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

static LRESULT CALLBACK SettingsWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrA(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
        self = static_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrA(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr ? self->WindowProc(window, message, wParam, lParam)
                           : DefWindowProcA(window, message, wParam, lParam);
}

SettingsWindow::SettingsWindow(HINSTANCE instance, HWND ownerWindow, Engine::ConsoleVariable& variables,
                               Input& input, Game::Client::MultiplayerInteractionPort& multiplayer)
    : mInstance(instance), mOwnerWindow(ownerWindow), mVariables(variables), mInput(input),
      mMultiplayer(multiplayer) {
}

SettingsWindow::~SettingsWindow() {
    Destroy();
}

void SettingsWindow::Init() {
    WNDCLASSEXA settingsClass{};
    settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.lpfnWndProc = SettingsWindowProc;
    settingsClass.hInstance = mInstance;
    settingsClass.hIcon = LoadIconA(mInstance, MAKEINTRESOURCEA(1));
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    settingsClass.lpszClassName = SettingsWindowClassName;
    RegisterClassExA(&settingsClass);

    mWindow = CreateWindowExA(WS_EX_TOOLWINDOW, SettingsWindowClassName, "Game Settings",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 520, 470,
                              mOwnerWindow, nullptr, mInstance, this);
    if (mWindow == nullptr) {
        return;
    }

    CreateWindowExA(0, "STATIC", "Display", WS_CHILD | WS_VISIBLE, 20, 18, 320, 22, mWindow, nullptr, mInstance,
                    nullptr);
    mVsyncCheck = CreateWindowExA(0, "BUTTON", "Vertical sync", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 50,
                                  150, 24, mWindow, ControlId(VsyncControl), mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Interpolation rate", WS_CHILD | WS_VISIBLE, 20, 94, 150, 22, mWindow, nullptr,
                    mInstance, nullptr);
    mFpsCombo = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 190, 90,
                                150, 180, mWindow, ControlId(FpsControl), mInstance, nullptr);
    for (int fps : FpsValues) {
        const std::string label = std::to_string(fps) + " FPS";
        SendMessageA(mFpsCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    CreateWindowExA(0, "STATIC", "Multiplayer", WS_CHILD | WS_VISIBLE, 20, 135, 460, 22, mWindow, nullptr, mInstance,
                    nullptr);
    mNetworkStatusLabel = CreateWindowExA(0, "STATIC", "Offline", WS_CHILD | WS_VISIBLE, 20, 162, 460, 22, mWindow,
                                          nullptr, mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Address", WS_CHILD | WS_VISIBLE, 20, 198, 70, 22, mWindow, nullptr, mInstance,
                    nullptr);
    mNetworkAddressEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "127.0.0.1",
                                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 92, 194, 245, 25, mWindow,
                                          ControlId(NetworkAddressControl), mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Port", WS_CHILD | WS_VISIBLE, 350, 198, 42, 22, mWindow, nullptr, mInstance,
                    nullptr);
    mNetworkPortEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "777",
                                       WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 395, 194, 85, 25, mWindow,
                                       ControlId(NetworkPortControl), mInstance, nullptr);
    SendMessageA(mNetworkAddressEdit, EM_SETLIMITTEXT, 255, 0);
    SendMessageA(mNetworkPortEdit, EM_SETLIMITTEXT, 5, 0);
    CreateWindowExA(0, "BUTTON", "Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 232, 145, 28, mWindow,
                    ControlId(NetworkHostControl), mInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 177, 232, 145, 28, mWindow,
                    ControlId(NetworkConnectControl), mInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Disconnect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 335, 232, 145, 28, mWindow,
                    ControlId(NetworkDisconnectControl), mInstance, nullptr);
    mChatCheck = CreateWindowExA(0, "BUTTON", "Show text chat", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 276,
                                 200, 24, mWindow, ControlId(NetworkChatControl), mInstance, nullptr);
    mVoiceCheck = CreateWindowExA(0, "BUTTON", "Voice chat", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 276, 200,
                                  24, mWindow, ControlId(NetworkVoiceControl), mInstance, nullptr);
    mPushToTalkCheck = CreateWindowExA(0, "BUTTON", "Push to talk (Shift)",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 308, 200, 24, mWindow,
                                       ControlId(NetworkPushToTalkControl), mInstance, nullptr);
    mCameraInvertYCheck = CreateWindowExA(0, "BUTTON", "Invert camera Y",
                                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 308, 200, 24, mWindow,
                                          ControlId(CameraInvertYControl), mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Enter opens chat.  / opens multiplayer commands.", WS_CHILD | WS_VISIBLE, 20, 354,
                    460, 22, mWindow, nullptr, mInstance, nullptr);
    CreateWindowExA(0, "STATIC", "Press Escape to return to the game.", WS_CHILD | WS_VISIBLE, 20, 390, 460, 24,
                    mWindow, nullptr, mInstance, nullptr);
    SyncControls();
}

void SettingsWindow::SyncControls() {
    if (mWindow == nullptr) {
        return;
    }
    auto* variables = &mVariables;
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
    const std::string port =
        std::to_string(std::clamp(variables->GetInteger("gSettings.MultiplayerPort", 777), 1, 49151));
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

void SettingsWindow::SyncNetworkStatus() {
    if (mNetworkStatusLabel == nullptr) {
        return;
    }
    const std::string status = Game::Client::FormatMultiplayerConnectionStatus(mMultiplayer.Status());
    SetWindowTextA(mNetworkStatusLabel, status.c_str());
}

void SettingsWindow::Toggle() {
    if (mWindow == nullptr) {
        return;
    }
    mVisible = !mVisible;
    mInput.SetGameInputBlocked(mVisible);
    ShowWindow(mWindow, mVisible ? SW_SHOWNORMAL : SW_HIDE);
    if (mVisible) {
        SyncControls();
        SetForegroundWindow(mWindow);
    } else {
        SetForegroundWindow(mOwnerWindow);
        SetFocus(mOwnerWindow);
    }
}

bool SettingsWindow::IsVisible() const {
    return mVisible;
}

bool SettingsWindow::OwnsWindow(HWND window) const {
    return mWindow != nullptr && (window == mWindow || IsChild(mWindow, window));
}

LRESULT SettingsWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
            Toggle();
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                Toggle();
                return 0;
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == VsyncControl && HIWORD(wParam) == BN_CLICKED) {
                auto* variables = &mVariables;
                variables->SetInteger(CVAR_VSYNC_ENABLED,
                                      SendMessageA(mVsyncCheck, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
                variables->Save();
            } else if (LOWORD(wParam) == FpsControl && HIWORD(wParam) == CBN_SELCHANGE) {
                const LRESULT selection = SendMessageA(mFpsCombo, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && selection < static_cast<LRESULT>(std::size(FpsValues))) {
                    auto* variables = &mVariables;
                    variables->SetInteger("gSettings.MatchRefreshRate", 0);
                    variables->SetInteger("gSettings.InterpolationFPS", FpsValues[selection]);
                    variables->Save();
                }
            } else if (HIWORD(wParam) == BN_CLICKED &&
                       (LOWORD(wParam) == NetworkChatControl || LOWORD(wParam) == NetworkVoiceControl ||
                        LOWORD(wParam) == NetworkPushToTalkControl || LOWORD(wParam) == CameraInvertYControl)) {
                auto* variables = &mVariables;
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
                auto* variables = &mVariables;
                char address[256]{};
                char portText[16]{};
                GetWindowTextA(mNetworkAddressEdit, address, static_cast<int>(std::size(address)));
                GetWindowTextA(mNetworkPortEdit, portText, static_cast<int>(std::size(portText)));
                const int port = std::clamp(std::atoi(portText), 1, 49151);
                variables->SetString("gSettings.MultiplayerAddress", address[0] ? address : "127.0.0.1");
                variables->SetInteger("gSettings.MultiplayerPort", port);
                variables->Save();
                if (LOWORD(wParam) == NetworkHostControl) {
                    mMultiplayer.Host(static_cast<uint16_t>(port));
                } else if (LOWORD(wParam) == NetworkConnectControl) {
                    mMultiplayer.Connect(address[0] ? address : "127.0.0.1", static_cast<uint16_t>(port));
                } else {
                    mMultiplayer.Disconnect();
                }
                SyncNetworkStatus();
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

void SettingsWindow::Destroy() {
    if (mVisible) {
        mInput.SetGameInputBlocked(false);
        mVisible = false;
    }
    if (mWindow != nullptr) {
        DestroyWindow(mWindow);
        mWindow = nullptr;
    }
    if (mInstance != nullptr) {
        UnregisterClassA(SettingsWindowClassName, mInstance);
    }
}

#endif
