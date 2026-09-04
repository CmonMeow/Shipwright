#pragma once

#include <cstdint>
#include <string>

namespace Game::Multiplayer {

enum class ServerCommandKind : uint8_t {
    Invalid,
    Team,
    Help,
    Kick,
    Ban,
    GrantAdministrator,
    RevokeAdministrator,
    Unban,
    Users,
    ListAdministrators,
    ListBans,
    Unknown,
};

enum class ServerCommandAccess : uint8_t {
    Player,
    Administrator,
};

enum class ServerCommandTeam : uint8_t {
    Neutral,
    Red,
    Blue,
};

struct ParsedServerCommand {
    ServerCommandKind kind = ServerCommandKind::Invalid;
    ServerCommandAccess access = ServerCommandAccess::Player;
    ServerCommandTeam team = ServerCommandTeam::Neutral;
    std::string argument;
    std::string error;

    bool Valid() const { return kind != ServerCommandKind::Invalid && error.empty(); }
};

class ServerCommandParser final {
  public:
    static ParsedServerCommand Parse(const std::string& text);
};

} // namespace Game::Multiplayer
