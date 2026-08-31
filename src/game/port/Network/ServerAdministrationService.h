#pragma once

#include "ModerationRegistry.h"
#include "../../platform/simulation/PlayerSimulation.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SoH::Network {

struct ServerAdministrationPlayer {
    int32_t playerId = -1;
    std::string identity;
    std::string name;
    int32_t latencyMilliseconds = 0;
};

struct ServerAdministrationContext {
    std::function<std::vector<ServerAdministrationPlayer>()> players;
    std::function<bool(int32_t, Game::Simulation::TeamId)> setTeam;
    std::function<void(int32_t, const std::string&)> sendResult;
    std::function<void(const std::string&)> broadcastSystem;
    std::function<void(int32_t, bool, const std::string&)> disconnectPlayer;
};

class ServerAdministrationService final {
  public:
    explicit ServerAdministrationService(std::string banFile = BAN_LIST_FILENAME,
                                         std::string administratorFile = GM_LIST_FILENAME);

    void Load();
    bool IsBanned(const std::string& identity) const;
    bool IsAdministrator(const std::string& identity) const;
    void Execute(int32_t playerId, const std::string& command,
                 const ServerAdministrationContext& context);

  private:
    const ServerAdministrationPlayer* ResolvePlayer(
        const std::string& reference,
        const std::vector<ServerAdministrationPlayer>& players) const;
    bool CallerIsAdministrator(
        int32_t playerId,
        const std::vector<ServerAdministrationPlayer>& players) const;
    void SendIdentityList(int32_t playerId, const std::string& label,
                          const std::vector<std::string>& identities,
                          const ServerAdministrationContext& context) const;

    ModerationRegistry mModeration;
};

} // namespace SoH::Network
