#include "Network/NetworkRuntime.h"

#include <runtime/log/Log.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <conio.h>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

std::atomic_bool gRunning{ true };

BOOL WINAPI ConsoleHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT ||
        event == CTRL_LOGOFF_EVENT || event == CTRL_SHUTDOWN_EVENT) {
        gRunning = false;
        return TRUE;
    }
    return FALSE;
}

void DrainRelayQueues(SoH::Network::NetworkRuntime& network) {
    NetworkChatLine chat;
    while (network.PollChat(chat)) {
        std::printf("%s\n", chat.text.c_str());
    }
    NetworkPlayerStatePacket player;
    while (network.PollPlayerState(player)) {
    }
    NetworkPlayerRemovePacket removal;
    while (network.PollPlayerRemove(removal)) {
    }
    NetworkDynamicObjectStatePacket objectState;
    while (network.PollDynamicObjectState(objectState)) {
    }
    NetworkProjectileStatePacket projectile;
    while (network.PollProjectileState(projectile)) {
    }
    NetworkPlayerRespawnPacket respawn;
    while (network.PollPlayerRespawn(respawn)) {
    }
    NetworkVoicePacket voice;
    while (network.PollVoice(voice)) {
    }
}

bool PollConsoleLine(std::string& line) {
    static std::string draft;
    while (_kbhit()) {
        const int character = _getch();
        if (character == '\r' || character == '\n') {
            std::putchar('\n');
            line = draft;
            draft.clear();
            return true;
        }
        if (character == 8) {
            if (!draft.empty()) {
                draft.pop_back();
                std::fputs("\b \b", stdout);
            }
            continue;
        }
        if (character >= 32 && character < 127 && draft.size() < CHAT_MAX_MESSAGE_CHARS) {
            draft.push_back(static_cast<char>(character));
            std::putchar(character);
        }
    }
    return false;
}

void RunConsoleCommand(const std::string& input, SoH::Network::NetworkRuntime& network) {
    const std::string line = TrimWhitespace(input);
    if (line.empty()) {
        return;
    }
    if (_stricmp(line.c_str(), "/quit") == 0 || _stricmp(line.c_str(), "/exit") == 0) {
        gRunning = false;
        return;
    }
    network.SendChat(line);
}

} // namespace

int main(int argc, char** argv) {
    ClearLog();
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    uint16_t port = DEFAULT_NETWORK_PORT;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] ? argv[index] : "";
        if ((argument == "--port" || argument == "-p") && index + 1 < argc) {
            const long parsed = std::strtol(argv[++index], nullptr, 10);
            if (parsed > 0 && parsed <= 49151) {
                port = static_cast<uint16_t>(parsed);
            }
        }
    }

    SoH::Network::NetworkRuntime network;
    if (!network.Host(port, "Game Dedicated Server")) {
        Error("Dedicated server failed to host on port %u", static_cast<unsigned>(port));
        std::fprintf(stderr, "Unable to host on UDP port %u.\n", static_cast<unsigned>(port));
        return EXIT_FAILURE;
    }

    std::printf("Game dedicated server listening on UDP port %u.\n", static_cast<unsigned>(port));
    Error("Dedicated server hosting secure session on port %u", static_cast<unsigned>(port));

    while (gRunning) {
        network.Update();
        DrainRelayQueues(network);
        std::string consoleLine;
        if (PollConsoleLine(consoleLine)) {
            RunConsoleCommand(consoleLine, network);
        }
        Sleep(10);
    }

    network.Disconnect();
    Error("Dedicated server stopped");
    return EXIT_SUCCESS;
}
