#include <sysdef.h>

#include "Network/ShipwrightNetworkRuntime.h"

#include <Windows.h>

#include <cstring>
#include <string>

namespace {

using SoH::Network::ShipwrightNetworkRuntime;

constexpr unsigned short kRuntimePort = 47778;

NetworkPlayerStatePacket MakeState(int value) {
    NetworkPlayerStatePacket packet{};
    packet.sceneId = 42;
    packet.roomId = 3;
    packet.sequence = value;
    packet.x = static_cast<float>(value);
    packet.y = static_cast<float>(value * 2);
    packet.z = static_cast<float>(value * 3);
    packet.rotationY = 12345;
    packet.speed = 2.5f;
    packet.stateFlags = NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_GROUNDED;
    packet.modelGroup = 2;
    packet.itemAction = 3;
    for (int limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb) {
        packet.jointTable[limb][0] = static_cast<short>(value + limb);
    }
    return packet;
}

NetworkVoicePacket MakeVoice(int value) {
    NetworkVoicePacket packet;
    packet.sequence = static_cast<unsigned __int32>(value);
    packet.codec = VOICE_CODEC_ADPCM;
    packet.sampleRate = VOICE_SAMPLE_RATE;
    packet.frameSamples = VOICE_SAMPLES_PER_PACKET;
    packet.data.assign(4 + VOICE_SAMPLES_PER_PACKET / 2, static_cast<unsigned char>(value));
    return packet;
}

int RunHost() {
    ShipwrightNetworkRuntime network;
    if (!network.Host(kRuntimePort, "Shipwright secure runtime test")) {
        return 10;
    }

    bool chatReceived = false;
    bool privateReceived = false;
    bool stateReceived = false;
    bool voiceReceived = false;
    bool responseSent = false;
    bool clientComplete = false;

    const unsigned __int64 timeout = GetTickCount64() + 15000;
    while (GetTickCount64() < timeout && !clientComplete) {
        network.Update();

        NetworkChatLine line;
        while (network.PollChat(line)) {
            chatReceived = chatReceived || line.text.find("runtime-client-chat") != std::string::npos;
            privateReceived = privateReceived || line.text.find("runtime-client-private") != std::string::npos;
            clientComplete = clientComplete || line.text.find("runtime-client-complete") != std::string::npos;
        }

        NetworkPlayerStatePacket state{};
        while (network.PollPlayerState(state)) {
            stateReceived = state.playerId > 0 && state.x == 111 && state.y == 222;
        }
        NetworkVoicePacket voice;
        while (network.PollVoice(voice)) {
            voiceReceived = voice.playerId > 0 && voice.sequence == 444 && !voice.data.empty();
        }

        if (!responseSent && network.IsSecure() && chatReceived && privateReceived && stateReceived && voiceReceived) {
            const auto players = network.Players();
            int32_t clientId = -1;
            for (const auto& player : players) {
                if (player.playerId > 0) {
                    clientId = player.playerId;
                }
            }
            if (clientId > 0 && network.SendPrivateChat(clientId, "runtime-host-private")) {
                network.SendChat("runtime-host-chat");
                network.SendPlayerState(MakeState(555));
                network.SendVoice(MakeVoice(777));
                responseSent = true;
            }
        }
        Sleep(5);
    }

    network.Disconnect();
    return clientComplete && responseSent ? 0 : 11;
}

int RunClient() {
    ShipwrightNetworkRuntime network;
    if (!network.Connect("127.0.0.1:47778")) {
        return 20;
    }

    bool initialSent = false;
    bool privateSent = false;
    bool chatReceived = false;
    bool privateReceived = false;
    bool stateReceived = false;
    bool voiceReceived = false;

    const unsigned __int64 timeout = GetTickCount64() + 15000;
    while (GetTickCount64() < timeout) {
        network.Update();
        if (!initialSent && network.IsSecure() && network.LocalPlayerId() > 0) {
            initialSent = network.SendChat("runtime-client-chat") && network.SendPlayerState(MakeState(111)) &&
                          network.SendVoice(MakeVoice(444));
        }
        if (initialSent && !privateSent) {
            privateSent = network.SendPrivateChat(0, "runtime-client-private");
        }

        NetworkChatLine line;
        while (network.PollChat(line)) {
            chatReceived = chatReceived || line.text.find("runtime-host-chat") != std::string::npos;
            privateReceived = privateReceived || line.text.find("runtime-host-private") != std::string::npos;
        }
        NetworkPlayerStatePacket state{};
        while (network.PollPlayerState(state)) {
            stateReceived = state.playerId == 0 && state.x == 555 && state.y == 1110;
        }
        NetworkVoicePacket voice;
        while (network.PollVoice(voice)) {
            voiceReceived = voice.playerId == 0 && voice.sequence == 777 && !voice.data.empty();
        }

        if (initialSent && privateSent && chatReceived && privateReceived && stateReceived && voiceReceived) {
            // Keep pumping past one complete telemetry interval so the byte
            // counters are converted into the rates shown in the title bar.
            for (int i = 0; i < 220; ++i) {
                if ((i % 10) == 0) {
                    network.SendChat("runtime-telemetry");
                }
                network.Update();
                Sleep(5);
            }
            if (network.InboundBytesPerSecond() <= 0 || network.OutboundBytesPerSecond() <= 0) {
                Error("Runtime telemetry failed: in=%d B/s out=%d B/s", network.InboundBytesPerSecond(),
                      network.OutboundBytesPerSecond());
                return 22;
            }
            network.SendChat("runtime-client-complete");
            for (int i = 0; i < 100; ++i) {
                network.Update();
                Sleep(5);
            }
            network.Disconnect();
            return 0;
        }
        Sleep(5);
    }

    network.Disconnect();
    return 21;
}

bool StartChild(const std::string& executable, const char* argument, PROCESS_INFORMATION& process) {
    std::string command = '"' + executable + "\" " + argument;
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    std::memset(&process, 0, sizeof(process));
    return CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                          &process) != FALSE;
}

int RunParent() {
    char executable[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, executable, sizeof(executable))) {
        return 30;
    }

    PROCESS_INFORMATION host{};
    PROCESS_INFORMATION client{};
    if (!StartChild(executable, "--host", host)) {
        return 31;
    }
    Sleep(200);
    if (!StartChild(executable, "--client", client)) {
        TerminateProcess(host.hProcess, 32);
        CloseHandle(host.hThread);
        CloseHandle(host.hProcess);
        return 32;
    }

    HANDLE processes[] = { host.hProcess, client.hProcess };
    const DWORD wait = WaitForMultipleObjects(2, processes, TRUE, 20000);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(host.hProcess, 33);
        TerminateProcess(client.hProcess, 33);
    }

    DWORD hostExit = 33;
    DWORD clientExit = 33;
    GetExitCodeProcess(host.hProcess, &hostExit);
    GetExitCodeProcess(client.hProcess, &clientExit);
    CloseHandle(host.hThread);
    CloseHandle(host.hProcess);
    CloseHandle(client.hThread);
    CloseHandle(client.hProcess);

    if (wait == WAIT_TIMEOUT || hostExit != 0 || clientExit != 0) {
        Error("Secure runtime self-test failed: wait=%lu host=%lu client=%lu", wait, hostExit, clientExit);
        return 34;
    }
    Error("Secure runtime self-test passed: encrypted state, chat, private E2E text, and voice");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--host") == 0) {
        return RunHost();
    }
    if (argc == 2 && std::strcmp(argv[1], "--client") == 0) {
        return RunClient();
    }
    ClearLog();
    return RunParent();
}
