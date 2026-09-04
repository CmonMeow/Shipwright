#include "multiplayer/ui/MultiplayerUI.h"

#include "../platform/client/MultiplayerCommandProcessor.h"
#include "../platform/client/MultiplayerInteractionPort.h"
#include "multiplayer/ui/MultiplayerHudRenderer.h"
#include "global.h"

#include <engine/config/ConsoleVariable.h>
#include <platform/win32/Input.h>
#include <rendering/Overlay.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kChatEnabled = "gSettings.MultiplayerChatEnabled";

constexpr size_t kVisibleChatRows =
    Game::Client::kMultiplayerChatVisibleRows;
constexpr size_t kChatHistoryLines =
    Game::Client::kMultiplayerChatHistoryLines;
constexpr size_t kChatMessageCharacters =
    Game::Client::kMultiplayerChatMessageCharacters;
constexpr size_t kChatLineCharacters =
    Game::Client::kMultiplayerChatLineCharacters;

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
    return Game::Client::SanitiseMultiplayerText(
        result, kChatMessageCharacters);
}

} // namespace

struct MultiplayerUI::Impl {
    Impl(Game::Client::MultiplayerInteractionPort& interactionPort, Engine::ConsoleVariable& consoleVariables,
         Engine::Rendering::GameRenderer& gameRenderer, Input& win32Input)
        : interaction(interactionPort), commands(interactionPort), renderer(gameRenderer), variables(consoleVariables),
          input(win32Input) {
    }

    Game::Client::MultiplayerInteractionPort& interaction;
    Game::Client::MultiplayerCommandProcessor commands;
    Game::UI::MultiplayerHudRenderer renderer;
    Engine::ConsoleVariable& variables;
    Input& input;
    std::vector<Game::UI::MultiplayerHudLine> history;
    std::string draft;
    std::string notice;
    std::string gameplayNotice;
    std::string lastStatus;
    size_t cursor = 0;
    size_t scrollOffset = 0;
    bool active = false;
    bool suppressOpeningSlash = false;

    Engine::ConsoleVariable& Variables() const {
        return variables;
    }

    void AddLine(
        std::string text,
        Game::Client::MultiplayerChatKind kind =
            Game::Client::MultiplayerChatKind::System) {
        text = Game::Client::SanitiseMultiplayerText(
            text, kChatLineCharacters);
        if (text.empty()) {
            return;
        }
        Game::UI::MultiplayerHudLineKind hudKind =
            Game::UI::MultiplayerHudLineKind::Player;
        if (kind == Game::Client::MultiplayerChatKind::System) {
            hudKind = Game::UI::MultiplayerHudLineKind::System;
        } else if (kind == Game::Client::MultiplayerChatKind::Private) {
            hudKind = Game::UI::MultiplayerHudLineKind::Private;
        }
        history.push_back({ std::move(text), hudKind });
        while (history.size() > kChatHistoryLines) {
            history.erase(history.begin());
        }
        scrollOffset = 0;
    }

    void DrainNetworkChat() {
        Game::Client::MultiplayerChatMessage line;
        while (interaction.PollChat(line)) {
            AddLine(std::move(line.text), line.kind);
        }
    }

    void RunCommand(const std::string& command) {
        const Game::Client::MultiplayerCommandResult result =
            commands.Execute(command);
        if (result.clearHistory) {
            history.clear();
        }
        notice = result.notice;
    }

    void Submit() {
        const std::string text = Trim(draft);
        draft.clear();
        cursor = 0;
        active = false;
        suppressOpeningSlash = false;
        input.SetTextInputCaptured(false);
        if (text.empty()) {
            return;
        }
        if (text[0] == '/') {
            RunCommand(text);
        } else if (interaction.SendChat(text)) {
            notice = "message sent";
        } else {
            notice = "not connected";
        }
    }

    void UpdateStatus(const Game::Client::MultiplayerConnectionStatus& connection) {
        const std::string status = Game::Client::FormatMultiplayerConnectionStatus(connection);
        if (status != lastStatus) {
            lastStatus = status;
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
        uint8_t ignored = 0;
        while (input.PopTextInput(ignored)) {
        }
        input.SetTextInputCaptured(true);
    }

    void UpdateInput() {
        if (input.IsGameInputBlocked()) {
            return;
        }
        if (!active) {
            if (input.ConsumePress(VK_RETURN)) {
                Open();
            } else if (input.ConsumePress(VK_OEM_2)) {
                Open("/");
            }
            return;
        }

        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 && input.ConsumePress('V')) {
            std::string text = ClipboardText();
            const size_t available =
                kChatMessageCharacters -
                std::min(draft.size(), kChatMessageCharacters);
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
            if (character >= 32 && character != 127 &&
                draft.size() < kChatMessageCharacters) {
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
            Submit();
        }

        const int32_t wheel = input.ConsumeMouseWheel();
        if (wheel != 0) {
            const size_t maxOffset = history.size() > kVisibleChatRows
                                         ? history.size() - kVisibleChatRows
                                         : 0;
            if (wheel > 0) {
                scrollOffset = std::min(maxOffset, scrollOffset + 3);
            } else {
                scrollOffset = scrollOffset > 3 ? scrollOffset - 3 : 0;
            }
        }
    }

    void Update() {
        Engine::Rendering::Overlay::BeginFrame();
        const Game::Client::MultiplayerConnectionStatus connection =
            interaction.Status();
        Engine::Rendering::Overlay::SetNetworkTelemetry(
            connection.Active(), connection.latencyMilliseconds,
            connection.inboundBytesPerSecond,
            connection.outboundBytesPerSecond);
        DrainNetworkChat();
        UpdateStatus(connection);
        UpdateInput();
        interaction.UpdateVoice(active);
        Game::UI::MultiplayerHudView view{};
        view.history = history;
        view.historyScrollOffset = scrollOffset;
        view.visibleHistoryRows = kVisibleChatRows;
        view.draft = draft;
        view.cursor = cursor;
        view.inputActive = active;
        view.passiveChatEnabled = Variables().GetInteger(kChatEnabled, 1) != 0;
        view.statusNotice = notice;
        view.gameplayNotice = gameplayNotice;
        view.lifeVisible = gPlayState != nullptr;
        view.health = gSaveContext.health;
        view.healthCapacity = gSaveContext.healthCapacity;
        view.healthPerSegment = FULL_HEART_HEALTH;
        renderer.Draw(view);
    }

    void Shutdown() {
        input.SetTextInputCaptured(false);
        Engine::Rendering::Overlay::SetNetworkTelemetry(false, 0, 0, 0);
        Engine::Rendering::Overlay::Clear();
        renderer.Reset();
        history.clear();
        gameplayNotice.clear();
    }
};

MultiplayerUI::MultiplayerUI(
    Game::Client::MultiplayerInteractionPort& interaction, Engine::ConsoleVariable& variables,
    Engine::Rendering::GameRenderer& renderer, Input& input)
    : mImpl(std::make_unique<Impl>(interaction, variables, renderer, input)) {
}

MultiplayerUI::~MultiplayerUI() {
    Shutdown();
}

void MultiplayerUI::Update() {
    mImpl->Update();
}

void MultiplayerUI::ShowNotification(const char* text) {
    mImpl->gameplayNotice = text == nullptr ? "" : text;
}

void MultiplayerUI::ClearNotification() {
    mImpl->gameplayNotice.clear();
}

void MultiplayerUI::Shutdown() {
    if (mImpl) {
        mImpl->Shutdown();
    }
}
