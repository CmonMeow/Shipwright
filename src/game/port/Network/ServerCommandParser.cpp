#include "ServerCommandParser.h"

#include "NetworkProtocol.h"

namespace SoH::Network {

namespace {

bool Equals(const std::string& left, const char* right) {
    return _stricmp(left.c_str(), right) == 0;
}

ParsedServerCommand RequiresArgument(ServerCommandKind kind, const std::string& argument,
                                     const char* usage) {
    ParsedServerCommand result;
    result.kind = kind;
    result.access = ServerCommandAccess::Administrator;
    result.argument = argument;
    if (argument.empty()) {
        result.kind = ServerCommandKind::Invalid;
        result.error = usage;
    }
    return result;
}

} // namespace

ParsedServerCommand ServerCommandParser::Parse(const std::string& text) {
    const std::string clean = TrimWhitespace(text);
    const size_t split = clean.find(' ');
    const std::string name = clean.substr(0, split);
    const std::string argument = split == std::string::npos
                                     ? std::string()
                                     : TrimWhitespace(clean.substr(split + 1));

    if (Equals(name, "/team")) {
        ParsedServerCommand result;
        result.kind = ServerCommandKind::Team;
        if (Equals(argument, "red")) result.team = ServerCommandTeam::Red;
        else if (Equals(argument, "blue")) result.team = ServerCommandTeam::Blue;
        else if (Equals(argument, "neutral")) result.team = ServerCommandTeam::Neutral;
        else {
            result.kind = ServerCommandKind::Invalid;
            result.error = "usage: /team red|blue|neutral";
        }
        return result;
    }
    if (Equals(name, "/help")) return { ServerCommandKind::Help };
    if (Equals(name, "/kick")) {
        return RequiresArgument(ServerCommandKind::Kick, argument, "usage: /kick name|identity|netId");
    }
    if (Equals(name, "/ban")) {
        return RequiresArgument(ServerCommandKind::Ban, argument, "usage: /ban name|identity|netId");
    }
    if (Equals(name, "/gm") || Equals(name, "/admin")) {
        return RequiresArgument(ServerCommandKind::GrantAdministrator, argument,
                                "usage: /admin name|identity|netId");
    }
    if (Equals(name, "/ungm") || Equals(name, "/unadmin")) {
        return RequiresArgument(ServerCommandKind::RevokeAdministrator, argument,
                                "usage: /unadmin name|identity|netId");
    }
    if (Equals(name, "/unban")) {
        return RequiresArgument(ServerCommandKind::Unban, argument, "usage: /unban identity");
    }

    ParsedServerCommand result;
    result.access = ServerCommandAccess::Administrator;
    if (Equals(name, "/users")) result.kind = ServerCommandKind::Users;
    else if (Equals(name, "/admins") || Equals(name, "/gms")) {
        result.kind = ServerCommandKind::ListAdministrators;
    } else if (Equals(name, "/bans")) result.kind = ServerCommandKind::ListBans;
    else {
        result.kind = ServerCommandKind::Unknown;
        result.error = "unknown command: " + name;
    }
    return result;
}

} // namespace SoH::Network
