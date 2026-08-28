#include "PathEngineMultiplayerUI.h"

#include "Network/ShipwrightNetworkRuntime.h"
#include "Network/VoiceChat.h"

#include <libultraship/log/PathEngineLog.h>
#include <libultraship/bridge/windowbridge.h>
#include <ship/Context.h>
#include <ship/config/ConsoleVariable.h>
#include <ship/input/Win32Input.h>
#include <ship/window/PathEngineOverlay.h>
#include <ship/window/Window.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kNetworkAction = "gSettings.MultiplayerAction";
constexpr const char* kNetworkAddress = "gSettings.MultiplayerAddress";
constexpr const char* kNetworkPort = "gSettings.MultiplayerPort";
constexpr const char* kNetworkStatus = "gSettings.MultiplayerStatus";
constexpr const char* kChatEnabled = "gSettings.MultiplayerChatEnabled";
constexpr const char* kVoiceEnabled = "gSettings.MultiplayerVoiceEnabled";
constexpr const char* kVoicePushToTalk = "gSettings.MultiplayerVoicePushToTalk";

std::string Trim(const std::string& text) {
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

std::string ClipboardText() {
    std::string result;
    if (!IsClipboardFormatAvailable(CF_TEXT) || !OpenClipboard(nullptr)) {
        return result;
    }
    HANDLE handle = GetClipboardData(CF_TEXT);
    if (handle != nullptr) {
        const char* text = static_cast<const char*>(GlobalLock(handle));
        if (text != nullptr) {
            result = text;
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return SanitiseChatText(result);
}

} // namespace

struct PathEngineMultiplayerUI::Impl {
    std::unique_ptr<cVoiceChat> voice;
    std::deque<NetworkChatLine> history;
    std::string draft;
    std::string notice;
    std::string gameplayNotice;
    std::string lastStatus;
    size_t cursor = 0;
    size_t viewStart = 0;
    size_t scrollOffset = 0;
    bool active = false;
    bool suppressOpeningSlash = false;

    Ship::ConsoleVariable& Variables() const {
        return *Ship::Context::GetInstance()->GetConsoleVariables();
    }

    void AddLine(std::string text, ChatLineKind kind = CLKSystem) {
        text = SanitiseChatLine(text);
        if (text.empty()) {
            return;
        }
        history.push_back({ std::move(text), kind });
        while (history.size() > CHAT_MAX_HISTORY_LINES) {
            history.pop_front();
        }
        scrollOffset = 0;
    }

    void DrainNetworkChat(SoH::Network::ShipwrightNetworkRuntime& runtime) {
        NetworkChatLine line;
        while (runtime.PollChat(line)) {
            AddLine(std::move(line.text), line.kind);
        }
    }

    int32_t FindPlayer(const SoH::Network::ShipwrightNetworkRuntime& runtime, const std::string& reference) const {
        char* end = nullptr;
        const long numeric = std::strtol(reference.c_str(), &end, 10);
        if (end != reference.c_str() && *end == '\0') {
            return static_cast<int32_t>(numeric);
        }
        for (const auto& player : runtime.Players()) {
            if (_stricmp(player.name.c_str(), reference.c_str()) == 0 ||
                _stricmp(player.identity.c_str(), reference.c_str()) == 0) {
                return player.playerId;
            }
        }
        return -1;
    }

    void RunCommand(const std::string& command, SoH::Network::ShipwrightNetworkRuntime& runtime) {
        const size_t split = command.find(' ');
        const std::string name = command.substr(0, split);
        const std::string argument = split == std::string::npos ? std::string() : Trim(command.substr(split + 1));

        if (_stricmp(name.c_str(), "/help") == 0) {
            notice = "/host /connect /disconnect /pm /users /kick /ban /unban /admin /unadmin /admins /bans /clear";
        } else if (_stricmp(name.c_str(), "/host") == 0) {
            uint16_t port = DEFAULT_NETWORK_PORT;
            if (!argument.empty()) {
                const long parsed = std::strtol(argument.c_str(), nullptr, 10);
                if (parsed > 0 && parsed <= 49151) {
                    port = static_cast<uint16_t>(parsed);
                }
            }
            runtime.Disconnect();
            notice = runtime.Host(port) ? "hosting secure session" : "unable to host session";
        } else if (_stricmp(name.c_str(), "/connect") == 0) {
            const std::string address = argument.empty() ? DEFAULT_NETWORK_ADDRESS : argument;
            runtime.Disconnect();
            notice = runtime.Connect(address) ? "connecting securely to " + address : "connection failed";
        } else if (_stricmp(name.c_str(), "/disconnect") == 0) {
            runtime.Disconnect();
            notice = "disconnected";
        } else if (_stricmp(name.c_str(), "/clear") == 0) {
            history.clear();
            notice.clear();
        } else if (_stricmp(name.c_str(), "/users") == 0 || _stricmp(name.c_str(), "/kick") == 0 ||
                   _stricmp(name.c_str(), "/ban") == 0 || _stricmp(name.c_str(), "/unban") == 0 ||
                   _stricmp(name.c_str(), "/gm") == 0 || _stricmp(name.c_str(), "/admin") == 0 ||
                   _stricmp(name.c_str(), "/ungm") == 0 || _stricmp(name.c_str(), "/unadmin") == 0 ||
                   _stricmp(name.c_str(), "/admins") == 0 || _stricmp(name.c_str(), "/gms") == 0 ||
                   _stricmp(name.c_str(), "/bans") == 0) {
            notice = runtime.SendChat(command) ? "command sent" : "not connected";
        } else if (_stricmp(name.c_str(), "/pm") == 0 || _stricmp(name.c_str(), "/w") == 0 ||
                   _stricmp(name.c_str(), "/tell") == 0) {
            const size_t messageSplit = argument.find(' ');
            if (messageSplit == std::string::npos) {
                notice = "usage: /pm name|id message";
                return;
            }
            const int32_t target = FindPlayer(runtime, argument.substr(0, messageSplit));
            const std::string message = Trim(argument.substr(messageSplit + 1));
            notice = target >= 0 && runtime.SendPrivateChat(target, message) ? "private message sent"
                                                                             : "private message failed";
        } else {
            notice = "unknown command: " + name;
        }
    }

    void Submit(SoH::Network::ShipwrightNetworkRuntime& runtime) {
        const std::string text = Trim(draft);
        draft.clear();
        cursor = 0;
        viewStart = 0;
        active = false;
        suppressOpeningSlash = false;
        Ship::GetWin32Input().SetTextInputCaptured(false);
        if (text.empty()) {
            return;
        }
        if (text[0] == '/') {
            RunCommand(text, runtime);
        } else if (runtime.SendChat(text)) {
            notice = "message sent";
        } else {
            notice = "not connected";
        }
    }

    void ProcessConnectionAction(SoH::Network::ShipwrightNetworkRuntime& runtime) {
        auto& variables = Variables();
        const int32_t action = variables.GetInteger(kNetworkAction, 0);
        if (action == 0) {
            return;
        }
        variables.SetInteger(kNetworkAction, 0);
        const uint16_t port = static_cast<uint16_t>(
            std::clamp(variables.GetInteger(kNetworkPort, DEFAULT_NETWORK_PORT), 1, 49151));
        const std::string address = variables.GetString(kNetworkAddress, DEFAULT_NETWORK_ADDRESS);
        runtime.Disconnect();
        if (action == 1) {
            notice = runtime.Host(port) ? "hosting secure session" : "unable to host session";
        } else if (action == 2) {
            notice = runtime.Connect(address) ? "connecting securely to " + address : "connection failed";
        } else {
            notice = "disconnected";
        }
        variables.Save();
    }

    void UpdateStatus(const SoH::Network::ShipwrightNetworkRuntime& runtime) {
        std::string status = "Offline";
        if (runtime.IsHost()) {
            status = "Hosting (secure) - " + std::to_string(runtime.Players().size()) + " player(s)";
        } else if (runtime.IsClient()) {
            status = runtime.IsSecure() ? "Connected (secure)" : "Connecting...";
            if (runtime.IsSecure()) {
                status += " - " + std::to_string(runtime.LatencyMilliseconds()) + " ms";
            }
        }
        if (status != lastStatus) {
            lastStatus = status;
            Variables().SetString(kNetworkStatus, status.c_str());
            if (status == "Offline" || status.rfind("Connected", 0) == 0 || status.rfind("Hosting", 0) == 0) {
                notice = status;
            }
        }
    }

    void Open(std::string initial = {}) {
        active = true;
        draft = std::move(initial);
        suppressOpeningSlash = draft == "/";
        cursor = draft.size();
        viewStart = 0;
        uint8_t ignored = 0;
        while (Ship::GetWin32Input().PopTextInput(ignored)) {
        }
        Ship::GetWin32Input().SetTextInputCaptured(true);
    }

    void UpdateInput(SoH::Network::ShipwrightNetworkRuntime& runtime) {
        auto& input = Ship::GetWin32Input();
        if (input.IsGameInputBlocked()) {
            return;
        }
        if (!active) {
            if (runtime.IsActive() && input.ConsumePress(VK_RETURN)) {
                Open();
            } else if (input.ConsumePress(VK_OEM_2)) {
                Open("/");
            }
            return;
        }

        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 && input.ConsumePress('V')) {
            std::string text = ClipboardText();
            const size_t available = CHAT_MAX_MESSAGE_CHARS - std::min(draft.size(), CHAT_MAX_MESSAGE_CHARS);
            if (text.size() > available) {
                text.resize(available);
            }
            draft.insert(cursor, text);
            cursor += text.size();
        }

        uint8_t character = 0;
        while (input.PopTextInput(character)) {
            // VK_OEM_2 opens command entry with an initial slash, and Windows
            // may deliver the matching WM_CHAR after that key press is consumed.
            // Drop that one character so `/connect` does not become `//connect`.
            if (suppressOpeningSlash) {
                suppressOpeningSlash = false;
                if (character == '/') {
                    continue;
                }
            }
            if (character >= 32 && character != 127 && draft.size() < CHAT_MAX_MESSAGE_CHARS) {
                draft.insert(cursor, 1, static_cast<char>(character));
                ++cursor;
            }
        }
        if (input.ConsumePress(VK_BACK) && cursor > 0) {
            draft.erase(cursor - 1, 1);
            --cursor;
        }
        if (input.ConsumePress(VK_DELETE) && cursor < draft.size()) {
            draft.erase(cursor, 1);
        }
        if (input.ConsumePress(VK_LEFT) && cursor > 0) {
            --cursor;
        }
        if (input.ConsumePress(VK_RIGHT) && cursor < draft.size()) {
            ++cursor;
        }
        if (input.ConsumePress(VK_HOME)) {
            cursor = 0;
        }
        if (input.ConsumePress(VK_END)) {
            cursor = draft.size();
        }
        if (input.ConsumePress(VK_ESCAPE)) {
            draft.clear();
            cursor = 0;
            active = false;
            suppressOpeningSlash = false;
            input.SetTextInputCaptured(false);
        } else if (input.ConsumePress(VK_RETURN)) {
            Submit(runtime);
        }

        const int32_t wheel = input.ConsumeMouseWheel();
        if (wheel != 0) {
            const size_t maxOffset = history.size() > CHAT_VISIBLE_ROWS ? history.size() - CHAT_VISIBLE_ROWS : 0;
            if (wheel > 0) {
                scrollOffset = std::min(maxOffset, scrollOffset + 3);
            } else {
                scrollOffset = scrollOffset > 3 ? scrollOffset - 3 : 0;
            }
        }
    }

    void Draw() {
        auto& variables = Variables();
        // A disabled passive overlay must never hide the command line while
        // the user is actively typing into it.
        if (!active && !variables.GetInteger(kChatEnabled, 1) && gameplayNotice.empty()) {
            return;
        }
        const float windowWidth = static_cast<float>(WindowGetWidth());
        const float width = std::max(120.0f, std::min(680.0f, windowWidth - 24.0f));
        constexpr float x = 12.0f;
        constexpr float y = 14.0f;
        constexpr float lineHeight = 18.0f;
        constexpr float inputHeight = 24.0f;
        const float height = CHAT_VISIBLE_ROWS * lineHeight + inputHeight + 18.0f;

        Ship::PathEngineOverlay::QueueRect(x, y, x + width, y + height, 0.02f, 0.025f, 0.025f, 0.70f);
        Ship::PathEngineOverlay::QueueRect(x, y, x + width, y + height, 0.32f, 0.38f, 0.36f, 0.95f, true);

        const size_t newest = history.size() > scrollOffset ? history.size() - scrollOffset : 0;
        const size_t first = newest > CHAT_VISIBLE_ROWS ? newest - CHAT_VISIBLE_ROWS : 0;
        const size_t visibleCharacters = width > 24.0f ? static_cast<size_t>((width - 20.0f) / 12.0f) : 1;
        const float textTop = y + height - 22.0f;
        for (size_t index = first; index < newest; ++index) {
            float red = 0.96f;
            float green = 0.96f;
            float blue = 0.96f;
            if (history[index].kind == CLKSystem) {
                red = 0.58f;
                green = 0.78f;
                blue = 0.84f;
            } else if (history[index].kind == CLKPrivate) {
                red = 0.82f;
                green = 0.70f;
                blue = 1.0f;
            }
            const std::string line = history[index].text.substr(0, visibleCharacters);
            Ship::PathEngineOverlay::QueueText(line.c_str(), x + 8.0f,
                                               textTop - static_cast<float>(index - first) * lineHeight, red, green,
                                               blue);
        }

        if (cursor < viewStart) {
            viewStart = cursor;
        }
        const size_t inputCharacters = visibleCharacters > 3 ? visibleCharacters - 3 : visibleCharacters;
        if (cursor > viewStart + inputCharacters) {
            viewStart = cursor - inputCharacters;
        }
        std::string visibleDraft = draft.substr(viewStart, inputCharacters);
        const size_t localCursor = std::min(cursor - viewStart, visibleDraft.size());
        const std::string inputLine = active ? "> " + visibleDraft.substr(0, localCursor) + "|" +
                                                   visibleDraft.substr(localCursor)
                                             : "> Enter: chat   /: command";
        Ship::PathEngineOverlay::QueueRect(x + 6.0f, y + 6.0f, x + width - 6.0f, y + inputHeight + 4.0f,
                                           0.07f, 0.085f, 0.08f, 0.92f);
        Ship::PathEngineOverlay::QueueText(inputLine.c_str(), x + 10.0f, y + 12.0f, active ? 0.95f : 0.60f,
                                           active ? 0.96f : 0.68f, active ? 0.92f : 0.66f);
        if (!notice.empty()) {
            Ship::PathEngineOverlay::QueueText(notice.c_str(), x + 8.0f, y + height + 4.0f, 0.95f, 0.82f, 0.48f);
        }
        if (!gameplayNotice.empty()) {
            const float notificationX = std::max(12.0f, (windowWidth - 420.0f) * 0.5f);
            Ship::PathEngineOverlay::QueueRect(notificationX, 42.0f, notificationX + 420.0f, 78.0f,
                                               0.02f, 0.025f, 0.025f, 0.82f);
            Ship::PathEngineOverlay::QueueText(gameplayNotice.c_str(), notificationX + 12.0f, 53.0f,
                                               0.95f, 0.90f, 0.72f);
        }
    }

    void UpdateVoice(SoH::Network::ShipwrightNetworkRuntime& runtime) {
        const bool enabled = Variables().GetInteger(kVoiceEnabled, 1) != 0;
        if (enabled && !voice) {
            voice = std::make_unique<cVoiceChat>();
            Error("PathEngine voice chat initialized");
        }
        if (!voice) {
            return;
        }
        const bool pushToTalk = Variables().GetInteger(kVoicePushToTalk, 0) != 0;
        const bool talkDown = Ship::GetWin32Input().Pressed(VK_SHIFT) && !active &&
                              !Ship::GetWin32Input().IsGameInputBlocked();
        voice->update(runtime, talkDown, enabled, pushToTalk);
    }

    void Update(SoH::Network::ShipwrightNetworkRuntime& runtime) {
        Ship::PathEngineOverlay::BeginFrame();
        ProcessConnectionAction(runtime);
        DrainNetworkChat(runtime);
        UpdateStatus(runtime);
        UpdateInput(runtime);
        UpdateVoice(runtime);
        Draw();
    }

    void Shutdown() {
        Ship::GetWin32Input().SetTextInputCaptured(false);
        Ship::PathEngineOverlay::Clear();
        voice.reset();
        history.clear();
        gameplayNotice.clear();
    }
};

PathEngineMultiplayerUI::PathEngineMultiplayerUI() : mImpl(std::make_unique<Impl>()) {
}

PathEngineMultiplayerUI::~PathEngineMultiplayerUI() {
    Shutdown();
}

void PathEngineMultiplayerUI::Update(SoH::Network::ShipwrightNetworkRuntime& runtime) {
    mImpl->Update(runtime);
}

void PathEngineMultiplayerUI::ShowNotification(const char* text) {
    mImpl->gameplayNotice = text == nullptr ? "" : text;
}

void PathEngineMultiplayerUI::ClearNotification() {
    mImpl->gameplayNotice.clear();
}

void PathEngineMultiplayerUI::Shutdown() {
    if (mImpl) {
        mImpl->Shutdown();
    }
}
