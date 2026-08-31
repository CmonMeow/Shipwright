#include "ServerAdministrationService.h"

#include "ServerCommandParser.h"

#include <algorithm>
#include <charconv>
#include <string_view>
#include <utility>

namespace SoH::Network {

namespace {

bool EqualIgnoringCase(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
                      [](unsigned char a, unsigned char b) {
                          if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a - 'A' + 'a');
                          if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b - 'A' + 'a');
                          return a == b;
                      });
}

void Send(const ServerAdministrationContext& context, int32_t playerId,
          const std::string& message) {
    if (context.sendResult) context.sendResult(playerId, message);
}

void Broadcast(const ServerAdministrationContext& context, const std::string& message) {
    if (context.broadcastSystem) context.broadcastSystem(message);
}

} // namespace

ServerAdministrationService::ServerAdministrationService(
    std::string banFile, std::string administratorFile)
    : mModeration(std::move(banFile), std::move(administratorFile)) {
}

void ServerAdministrationService::Load() {
    mModeration.Load();
    // A disk serial was the old local bootstrap identifier, but it was sent by
    // clients and therefore could be copied. Only the server's own serial may
    // migrate that one local entry to the persistent signing-key identity.
    mModeration.MigrateGameMasterIdentity(LocalMachineSerialId(),
                                          LocalIdentityId());
}

bool ServerAdministrationService::IsBanned(const std::string& identity) const {
    return mModeration.IsBanned(identity);
}

bool ServerAdministrationService::IsAdministrator(const std::string& identity) const {
    return mModeration.IsGameMaster(identity);
}

const ServerAdministrationPlayer* ServerAdministrationService::ResolvePlayer(
    const std::string& reference,
    const std::vector<ServerAdministrationPlayer>& players) const {
    int32_t numericId = -1;
    const char* begin = reference.data();
    const char* end = begin + reference.size();
    const auto numeric = std::from_chars(begin, end, numericId);
    if (numeric.ec == std::errc{} && numeric.ptr == end && numericId > 0) {
        const auto found = std::find_if(players.begin(), players.end(),
            [numericId](const ServerAdministrationPlayer& player) {
                return player.playerId == numericId;
            });
        return found == players.end() ? nullptr : &*found;
    }

    const auto identity = std::find_if(players.begin(), players.end(),
        [&reference](const ServerAdministrationPlayer& player) {
            return player.playerId > 0 && !player.identity.empty() &&
                   EqualIgnoringCase(player.identity, reference);
        });
    if (identity != players.end()) return &*identity;

    const auto name = std::find_if(players.begin(), players.end(),
        [&reference](const ServerAdministrationPlayer& player) {
            return player.playerId > 0 && !player.name.empty() &&
                   EqualIgnoringCase(player.name, reference);
        });
    return name == players.end() ? nullptr : &*name;
}

bool ServerAdministrationService::CallerIsAdministrator(
    int32_t playerId,
    const std::vector<ServerAdministrationPlayer>& players) const {
    if (playerId == 0) return true;
    const auto caller = std::find_if(players.begin(), players.end(),
        [playerId](const ServerAdministrationPlayer& player) {
            return player.playerId == playerId;
        });
    return caller != players.end() && !caller->identity.empty() &&
           IsAdministrator(caller->identity);
}

void ServerAdministrationService::SendIdentityList(
    int32_t playerId, const std::string& label,
    const std::vector<std::string>& identities,
    const ServerAdministrationContext& context) const {
    Send(context, playerId, label + ": " + std::to_string(identities.size()));
    for (const std::string& identity : identities) {
        Send(context, playerId, identity);
    }
}

void ServerAdministrationService::Execute(
    int32_t playerId, const std::string& command,
    const ServerAdministrationContext& context) {
    const ParsedServerCommand parsed = ServerCommandParser::Parse(command);
    if (!parsed.Valid()) {
        Send(context, playerId, parsed.error);
        return;
    }

    const std::vector<ServerAdministrationPlayer> players =
        context.players ? context.players() : std::vector<ServerAdministrationPlayer>{};
    const bool isAdministrator = CallerIsAdministrator(playerId, players);
    if (parsed.access == ServerCommandAccess::Administrator && !isAdministrator) {
        Send(context, playerId, "admin only command");
        return;
    }

    switch (parsed.kind) {
        case ServerCommandKind::Team: {
            Game::Simulation::TeamId team = Game::Simulation::TeamId::Neutral;
            const char* teamName = "neutral";
            if (parsed.team == ServerCommandTeam::Red) {
                team = Game::Simulation::TeamId::Red;
                teamName = "red";
            } else if (parsed.team == ServerCommandTeam::Blue) {
                team = Game::Simulation::TeamId::Blue;
                teamName = "blue";
            }
            if (!context.setTeam || !context.setTeam(playerId, team)) {
                Send(context, playerId, "player simulation is not ready");
            } else {
                Send(context, playerId, std::string("team set to ") + teamName);
            }
            return;
        }
        case ServerCommandKind::Help:
            Send(context, playerId, "player commands: /team red|blue|neutral");
            if (isAdministrator) {
                Send(context, playerId,
                     "admin commands: /users /kick /ban /unban /admin /unadmin /admins /bans");
            }
            return;
        case ServerCommandKind::Kick:
        case ServerCommandKind::Ban: {
            const ServerAdministrationPlayer* target = ResolvePlayer(parsed.argument, players);
            if (!target) {
                Send(context, playerId, "player not found");
                return;
            }
            const bool ban = parsed.kind == ServerCommandKind::Ban;
            if (ban && target->identity.empty()) {
                Send(context, playerId, "player has no identity yet");
                return;
            }
            if (ban) mModeration.Ban(target->identity);
            const std::string result = target->name + (ban ? " was banned" : " was kicked");
            Broadcast(context, result);
            if (context.disconnectPlayer) {
                context.disconnectPlayer(target->playerId, ban, result);
            }
            return;
        }
        case ServerCommandKind::GrantAdministrator: {
            const ServerAdministrationPlayer* target = ResolvePlayer(parsed.argument, players);
            if (!target) {
                Send(context, playerId, "player not found");
                return;
            }
            if (target->identity.empty()) {
                Send(context, playerId, "player has no identity yet");
                return;
            }
            mModeration.GrantGameMaster(target->identity);
            Broadcast(context, target->name + " is now an admin");
            return;
        }
        case ServerCommandKind::RevokeAdministrator: {
            std::string identity = parsed.argument;
            if (const ServerAdministrationPlayer* target = ResolvePlayer(parsed.argument, players)) {
                identity = target->identity;
            }
            std::string removed;
            if (!mModeration.RevokeGameMaster(identity, &removed)) {
                Send(context, playerId, "admin identity not found");
                return;
            }
            Broadcast(context, removed + " is no longer an admin");
            return;
        }
        case ServerCommandKind::Unban: {
            std::string removed;
            if (!mModeration.Unban(parsed.argument, &removed)) {
                Send(context, playerId, "banned identity not found");
                return;
            }
            Broadcast(context, removed + " was unbanned");
            return;
        }
        case ServerCommandKind::Users:
            Send(context, playerId, "users online: " + std::to_string(players.size()));
            for (const ServerAdministrationPlayer& player : players) {
                std::string line = "#" + std::to_string(player.playerId) + " " + player.name;
                if (!player.identity.empty()) line += " [" + player.identity + "]";
                line += " " + std::to_string(player.latencyMilliseconds) + " ms";
                Send(context, playerId, line);
            }
            return;
        case ServerCommandKind::ListAdministrators:
            SendIdentityList(playerId, "admins", mModeration.GameMasters(), context);
            return;
        case ServerCommandKind::ListBans:
            SendIdentityList(playerId, "bans", mModeration.Bans(), context);
            return;
        default:
            Send(context, playerId, parsed.error);
            return;
    }
}

} // namespace SoH::Network
