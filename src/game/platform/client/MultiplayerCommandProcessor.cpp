#include "MultiplayerCommandProcessor.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iterator>
#include <limits>

namespace Game::Client {
namespace {

std::string Trim(const std::string& text) {
    size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

bool EqualAsciiCaseInsensitive(const std::string& left,
                               const std::string& right) {
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
                      [](unsigned char lhs, unsigned char rhs) {
                          return std::tolower(lhs) == std::tolower(rhs);
                      });
}

bool IsServerCommand(const std::string& name) {
    constexpr const char* commands[] = {
        "/users", "/kick",    "/ban",    "/unban", "/gm",
        "/admin", "/ungm",   "/unadmin", "/admins", "/gms",
        "/bans",
    };
    return std::any_of(std::begin(commands), std::end(commands),
                       [&name](const char* command) {
                           return EqualAsciiCaseInsensitive(name, command);
                       });
}

} // namespace

MultiplayerCommandProcessor::MultiplayerCommandProcessor(
    MultiplayerInteractionPort& interaction)
    : mInteraction(interaction) {
}

int32_t MultiplayerCommandProcessor::FindPlayer(
    const std::string& reference) const {
    if (reference.empty()) return -1;

    char* end = nullptr;
    errno = 0;
    const long numeric = std::strtol(reference.c_str(), &end, 10);
    if (errno == 0 && end != reference.c_str() && *end == '\0' &&
        numeric >= std::numeric_limits<int32_t>::min() &&
        numeric <= std::numeric_limits<int32_t>::max()) {
        return static_cast<int32_t>(numeric);
    }

    for (const MultiplayerPeerIdentity& player : mInteraction.Players()) {
        if (EqualAsciiCaseInsensitive(player.name, reference) ||
            EqualAsciiCaseInsensitive(player.identity, reference)) {
            return player.playerId;
        }
    }
    return -1;
}

MultiplayerCommandResult MultiplayerCommandProcessor::Execute(
    const std::string& command) const {
    const std::string cleanCommand = Trim(command);
    const size_t split = cleanCommand.find(' ');
    const std::string name = cleanCommand.substr(0, split);
    const std::string argument = split == std::string::npos
                                     ? std::string()
                                     : Trim(cleanCommand.substr(split + 1));
    MultiplayerCommandResult result{};

    if (EqualAsciiCaseInsensitive(name, "/help")) {
        result.notice = "/host /connect /disconnect /pm /users /kick /ban /unban /admin /unadmin /admins /bans /clear";
    } else if (EqualAsciiCaseInsensitive(name, "/host")) {
        uint16_t port = kDefaultMultiplayerPort;
        if (!argument.empty()) {
            char* end = nullptr;
            const long parsed = std::strtol(argument.c_str(), &end, 10);
            if (end != argument.c_str() && *end == '\0' && parsed > 0 &&
                parsed <= 49151) {
                port = static_cast<uint16_t>(parsed);
            }
        }
        result.notice = mInteraction.Host(port) ? "hosting secure session"
                                                : "unable to host session";
    } else if (EqualAsciiCaseInsensitive(name, "/connect")) {
        const std::string address = argument.empty()
                                        ? kDefaultMultiplayerAddress
                                        : argument;
        result.notice = mInteraction.Connect(address)
                            ? "connecting securely to " + address
                            : "connection failed";
    } else if (EqualAsciiCaseInsensitive(name, "/disconnect")) {
        mInteraction.Disconnect();
        result.notice = "disconnected";
    } else if (EqualAsciiCaseInsensitive(name, "/clear")) {
        result.clearHistory = true;
    } else if (IsServerCommand(name)) {
        result.notice = mInteraction.SendChat(cleanCommand) ? "command sent"
                                                            : "not connected";
    } else if (EqualAsciiCaseInsensitive(name, "/pm") ||
               EqualAsciiCaseInsensitive(name, "/w") ||
               EqualAsciiCaseInsensitive(name, "/tell")) {
        const size_t messageSplit = argument.find(' ');
        if (messageSplit == std::string::npos) {
            result.notice = "usage: /pm name|id message";
            return result;
        }
        const int32_t target = FindPlayer(argument.substr(0, messageSplit));
        const std::string message = Trim(argument.substr(messageSplit + 1));
        result.notice = target >= 0 && !message.empty() &&
                                mInteraction.SendPrivateChat(target, message)
                            ? "private message sent"
                            : "private message failed";
    } else {
        result.notice = "unknown command: " + name;
    }
    return result;
}

} // namespace Game::Client
