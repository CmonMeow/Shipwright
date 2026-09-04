#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Engine {
class ConsoleVariable;
}

namespace Game::Client {
class MultiplayerInteractionPort;
}

struct Input;

class SettingsWindow final {
  public:
    SettingsWindow(HINSTANCE instance, HWND ownerWindow, Engine::ConsoleVariable& variables,
                   Input& input, Game::Client::MultiplayerInteractionPort& multiplayer);
    ~SettingsWindow();

    void Init();
    void Toggle();
    bool IsVisible() const;
    bool OwnsWindow(HWND window) const;
    void SyncNetworkStatus();
    LRESULT WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

  private:
    void SyncControls();
    void Destroy();

    HINSTANCE mInstance = nullptr;
    HWND mOwnerWindow = nullptr;
    HWND mWindow = nullptr;
    HWND mVsyncCheck = nullptr;
    HWND mFpsCombo = nullptr;
    HWND mNetworkStatusLabel = nullptr;
    HWND mNetworkAddressEdit = nullptr;
    HWND mNetworkPortEdit = nullptr;
    HWND mChatCheck = nullptr;
    HWND mVoiceCheck = nullptr;
    HWND mPushToTalkCheck = nullptr;
    HWND mCameraInvertYCheck = nullptr;
    Engine::ConsoleVariable& mVariables;
    Input& mInput;
    Game::Client::MultiplayerInteractionPort& mMultiplayer;
    bool mVisible = false;
};

#endif
