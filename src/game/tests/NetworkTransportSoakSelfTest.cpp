#include "multiplayer/Win32NetworkPlatform.h"

#include "multiplayer/NetworkRuntime.h"
#include "multiplayer/netTransport.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

using Game::Multiplayer::NetworkRuntime;

constexpr unsigned short kPort = 47779;
constexpr int kClientCount = 4;
constexpr uint32_t kFinalCommand = 600;
constexpr unsigned kDisposableDropInterval = 17;

Game::Simulation::PlayerCommand MakeCommand(uint32_t sequence, int clientIndex) {
    constexpr float kBinaryAngleToRadians = 3.14159265358979323846f / 32768.0f;
    Game::Simulation::PlayerCommand command{};
    command.sequence = sequence;
    command.actionSequence = sequence % 30 == 0 ? sequence : 0;
    const int strafeBand = static_cast<int>((sequence / 45 + static_cast<uint32_t>(clientIndex)) % 3) - 1;
    command.moveX = static_cast<float>(strafeBand * 80) / 85.0f;
    command.moveY = static_cast<float>((sequence / 90) % 2 == 0 ? 85 : -70) / 85.0f;
    command.headingRadians = static_cast<int16_t>(
        (sequence * 173 + clientIndex * 0x4000) & 0xFFFF) * kBinaryAngleToRadians;
    command.heldActions = (sequence % 60) < 30 ? NETWORK_ACTION_BLOCK : 0;
    command.pressedActions = sequence % 30 == 0 ? NETWORK_ACTION_PRIMARY : 0;
    return command;
}

int RunHost() {
    NetworkRuntime network(110);
    if (!network.Host(kPort, "Encrypted multi-client transport soak") ||
        !network.ConfigureSceneSpawn({ 110, {}, 0.0f })) {
        return 10;
    }

    Game::Client::LocalSceneEntryRequest hostEntry{};
    hostEntry.sequence = 1;
    hostEntry.sceneId = 110;
    if (!network.SendSceneEntryIntent(hostEntry)) return 11;

    std::array<bool, kClientCount> completed{};
    std::map<int32_t, uint32_t> acknowledged;
    uint32_t combatResults = 0;
    bool faultsActive = false;
    bool acknowledgementsSent = false;
    const uint64_t timeout = GetTickCount64() + 30000;
    while (GetTickCount64() < timeout) {
        network.Update();
        if (!faultsActive && network.Players().size() == kClientCount + 1 && network.IsSecure()) {
            ConfigureNetworkTestPacketLoss(kDisposableDropInterval, true);
            faultsActive = true;
        }

        NetworkChatLine line{};
        while (network.PollChat(line)) {
            for (int i = 0; i < kClientCount; ++i) {
                completed[i] = completed[i] ||
                               line.text.find("transport-soak-complete-" + std::to_string(i)) != std::string::npos;
            }
        }
        Game::Simulation::PlayerSnapshot snapshot{};
        while (network.PollPlayerSnapshot(snapshot)) {
            if (snapshot.ownerPlayerId > 0) {
                acknowledged[snapshot.ownerPlayerId] =
                    (std::max)(acknowledged[snapshot.ownerPlayerId],
                               snapshot.lastProcessedCommand);
            }
        }
        Game::Simulation::CombatResultEvent combat{};
        while (network.PollCombatResult(combat)) ++combatResults;

        const bool allClientsComplete =
            std::all_of(completed.begin(), completed.end(), [](bool complete) { return complete; });
        const bool allCommandsAcknowledged =
            acknowledged.size() == kClientCount &&
            std::all_of(acknowledged.begin(), acknowledged.end(), [](const auto& entry) {
                return entry.second >= kFinalCommand;
            });
        if (!acknowledgementsSent && allClientsComplete && allCommandsAcknowledged) {
            acknowledgementsSent = network.SendChat("transport-soak-ack");
        }
        if (acknowledgementsSent) {
            for (int i = 0; i < 120; ++i) {
                network.Update();
                Sleep(5);
            }
            const NetworkTestFaultStats faults = GetNetworkTestFaultStats();
            network.Disconnect();
            Error("transport_soak host clients=%zu combat=%u considered=%llu dropped=%llu reliableDropped=%llu",
                  acknowledged.size(), combatResults, faults.considered, faults.dropped, faults.reliableDropped);
            return faultsActive && faults.dropped >= 1 && faults.reliableDropped == 1 ? 0 : 12;
        }
        Sleep(5);
    }
    const NetworkTestFaultStats faults = GetNetworkTestFaultStats();
    Error("transport_soak host timeout players=%zu acknowledged=%zu combat=%u faults=%d dropped=%llu reliableDropped=%llu",
          network.Players().size(), acknowledged.size(), combatResults, faultsActive, faults.dropped,
          faults.reliableDropped);
    network.Disconnect();
    return 13;
}

int RunClient(int clientIndex) {
    NetworkRuntime network;
    if (!network.Connect("127.0.0.1:47779")) return 20;

    bool faultsActive = false;
    bool teamSent = false;
    bool sceneSent = false;
    bool sceneAccepted = false;
    bool authoritativePlayerReady = false;
    bool completionSent = false;
    bool hostAcknowledged = false;
    uint32_t nextCommand = 1;
    uint32_t authoritativeAck = 0;
    uint64_t nextSendAt = 0;
    const uint64_t timeout = GetTickCount64() + 28000;
    while (GetTickCount64() < timeout) {
        network.Update();
        if (!faultsActive && network.IsSecure()) {
            ConfigureNetworkTestPacketLoss(kDisposableDropInterval, true);
            faultsActive = true;
        }
        if (faultsActive && !teamSent) {
            teamSent = network.SendChat(clientIndex % 2 == 0 ? "/team red" : "/team blue");
        }
        if (teamSent && !sceneSent) {
            Game::Client::LocalSceneEntryRequest entry{};
            entry.sequence = 1;
            entry.sceneId = 110;
            sceneSent = network.SendSceneEntryIntent(entry);
        }

        Game::Client::LocalSceneAuthority sceneState{};
        while (network.PollSceneEntryState(sceneState)) {
            sceneAccepted = sceneAccepted || (sceneState.accepted && sceneState.sceneId == 110);
        }

        const uint64_t now = GetTickCount64();
        if (sceneAccepted && authoritativePlayerReady && nextCommand <= kFinalCommand && now >= nextSendAt) {
            if (network.SendPlayerCommand(MakeCommand(nextCommand, clientIndex))) ++nextCommand;
            nextSendAt = now + 8;
        }

        Game::Simulation::PlayerSnapshot snapshot{};
        while (network.PollPlayerSnapshot(snapshot)) {
            if (snapshot.ownerPlayerId == network.LocalPlayerId()) {
                authoritativePlayerReady = authoritativePlayerReady || snapshot.lifeEpoch != 0;
                authoritativeAck = (std::max)(authoritativeAck, snapshot.lastProcessedCommand);
            }
        }
        if (nextCommand > kFinalCommand && authoritativeAck < kFinalCommand && now >= nextSendAt) {
            network.SendPlayerCommand(MakeCommand(kFinalCommand, clientIndex));
            nextSendAt = now + 50;
        }
        if (!completionSent && authoritativeAck >= kFinalCommand) {
            completionSent = network.SendChat("transport-soak-complete-" + std::to_string(clientIndex));
        }
        NetworkChatLine line{};
        while (network.PollChat(line)) {
            hostAcknowledged = hostAcknowledged || line.text.find("transport-soak-ack") != std::string::npos;
        }
        if (completionSent && hostAcknowledged) {
            const NetworkTestFaultStats faults = GetNetworkTestFaultStats();
            network.Disconnect();
            Error("transport_soak client=%d ack=%u considered=%llu dropped=%llu reliableDropped=%llu", clientIndex,
                  authoritativeAck, faults.considered, faults.dropped, faults.reliableDropped);
            return faultsActive && faults.dropped > 1 && faults.reliableDropped == 1 ? 0 : 21;
        }
        Sleep(2);
    }
    const NetworkTestFaultStats faults = GetNetworkTestFaultStats();
    Error("transport_soak client timeout client=%d secure=%d team=%d sceneSent=%d sceneAccepted=%d playerReady=%d next=%u ack=%u complete=%d hostAck=%d dropped=%llu reliableDropped=%llu",
          clientIndex, network.IsSecure(), teamSent, sceneSent, sceneAccepted, authoritativePlayerReady, nextCommand,
          authoritativeAck, completionSent, hostAcknowledged, faults.dropped, faults.reliableDropped);
    network.Disconnect();
    return 22;
}

bool StartChild(const std::string& executable, const std::string& argument, PROCESS_INFORMATION& process) {
    std::string command = '"' + executable + "\" " + argument;
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    std::memset(&process, 0, sizeof(process));
    return CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                          &startup, &process) != FALSE;
}

int RunParent() {
    char executable[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, executable, sizeof(executable))) return 30;

    std::array<PROCESS_INFORMATION, kClientCount + 1> children{};
    if (!StartChild(executable, "--host", children[0])) return 31;
    Sleep(200);
    for (int i = 0; i < kClientCount; ++i) {
        if (!StartChild(executable, "--client " + std::to_string(i), children[i + 1])) return 32;
        Sleep(75);
    }

    std::array<HANDLE, kClientCount + 1> handles{};
    for (size_t i = 0; i < children.size(); ++i) handles[i] = children[i].hProcess;
    const DWORD wait = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), TRUE, 35000);
    bool passed = wait != WAIT_TIMEOUT;
    for (auto& child : children) {
        DWORD exitCode = 33;
        GetExitCodeProcess(child.hProcess, &exitCode);
        if (wait == WAIT_TIMEOUT) TerminateProcess(child.hProcess, 33);
        passed = passed && exitCode == 0;
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
    }
    return passed ? 0 : 34;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--host") == 0) return RunHost();
    if (argc == 3 && std::strcmp(argv[1], "--client") == 0) return RunClient(std::atoi(argv[2]));
    ClearLog();
    return RunParent();
}
