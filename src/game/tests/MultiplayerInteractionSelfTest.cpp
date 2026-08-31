#include "../platform/client/ClientFrameClock.h"
#include "../platform/client/MultiplayerCommandProcessor.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeInteraction final : public Game::Client::MultiplayerInteractionPort {
  public:
    bool hostResult = true;
    bool connectResult = true;
    bool chatResult = true;
    bool privateResult = true;
    uint16_t hostedPort = 0;
    std::string connectedAddress;
    std::string chat;
    int32_t privateTarget = -1;
    std::string privateMessage;
    int disconnects = 0;
    std::vector<Game::Client::MultiplayerPeerIdentity> players;

    bool Host(uint16_t port) override {
        hostedPort = port;
        return hostResult;
    }
    bool Connect(const std::string& address) override {
        connectedAddress = address;
        return connectResult;
    }
    void Disconnect() override {
        ++disconnects;
    }
    bool SendChat(const std::string& message) override {
        chat = message;
        return chatResult;
    }
    bool SendPrivateChat(int32_t targetPlayer,
                         const std::string& message) override {
        privateTarget = targetPlayer;
        privateMessage = message;
        return privateResult;
    }
    bool PollChat(Game::Client::MultiplayerChatMessage&) override {
        return false;
    }
    std::vector<Game::Client::MultiplayerPeerIdentity> Players()
        const override {
        return players;
    }
    Game::Client::MultiplayerConnectionStatus Status() const override {
        return {};
    }
    void UpdateVoice(bool) override {
    }
    void Shutdown() override {
    }
};

bool Expect(bool condition, const char* description) {
    if (!condition) std::fprintf(stderr, "failed: %s\n", description);
    return condition;
}

} // namespace

int main() {
    using namespace Game::Client;
    FakeInteraction interaction;
    interaction.players = {
        { 7, "disk:owner", "Alice" },
        { 12, "disk:peer", "Bob" },
    };
    MultiplayerCommandProcessor commands(interaction);
    bool passed = true;

    ClientFrameClock frameClock;
    passed &= Expect(frameClock.Sample(10.0) ==
                         ClientFrameClock::kDefaultDeltaSeconds,
                     "first frame uses the deterministic default delta");
    passed &= Expect(frameClock.Sample(10.02) > 0.019f &&
                         frameClock.Sample(10.04) < 0.021f,
                     "normal monotonic frame samples preserve elapsed time");
    passed &= Expect(frameClock.Sample(11.0) ==
                         ClientFrameClock::kMaximumDeltaSeconds,
                     "long frames are clamped");
    frameClock.Reset(20.0);
    passed &= Expect(frameClock.Sample(19.0) == 0.0f,
                     "clock reversal cannot produce negative simulation time");
    frameClock.Reset(30.0);
    passed &= Expect(frameClock.Sample(30.01) > 0.009f &&
                         frameClock.Sample(30.02) < 0.011f,
                     "session reset discards prior elapsed time");

    passed &= Expect(SanitiseMultiplayerText("ok\n\tbad", 16) == "okbad",
                     "chat sanitization removes control characters");
    passed &= Expect(SanitiseMultiplayerText("abcdef", 4) == "abcd",
                     "chat sanitization enforces the character limit");

    MultiplayerCommandResult result = commands.Execute(" /HOST 4200 ");
    passed &= Expect(interaction.hostedPort == 4200 &&
                         result.notice == "hosting secure session",
                     "host command is case-insensitive and parses a port");
    commands.Execute("/host invalid");
    passed &= Expect(interaction.hostedPort == kDefaultMultiplayerPort,
                     "invalid host port uses the documented default");

    result = commands.Execute("/connect");
    passed &= Expect(interaction.connectedAddress == kDefaultMultiplayerAddress &&
                         result.notice.find(kDefaultMultiplayerAddress) !=
                             std::string::npos,
                     "connect command uses the default address");
    commands.Execute("/connect 10.0.0.5:9000");
    passed &= Expect(interaction.connectedAddress == "10.0.0.5:9000",
                     "connect command preserves the requested endpoint");

    result = commands.Execute("/admin Alice");
    passed &= Expect(interaction.chat == "/admin Alice" &&
                         result.notice == "command sent",
                     "administration commands pass through the semantic port");

    result = commands.Execute("/pm aLiCe hello there");
    passed &= Expect(interaction.privateTarget == 7 &&
                         interaction.privateMessage == "hello there" &&
                         result.notice == "private message sent",
                     "private messages resolve player names without case");
    commands.Execute("/tell disk:peer identity lookup");
    passed &= Expect(interaction.privateTarget == 12 &&
                         interaction.privateMessage == "identity lookup",
                     "private messages resolve stable player identities");
    commands.Execute("/w 7 numeric lookup");
    passed &= Expect(interaction.privateTarget == 7 &&
                         interaction.privateMessage == "numeric lookup",
                     "private messages accept numeric player identifiers");

    result = commands.Execute("/pm Alice");
    passed &= Expect(result.notice == "usage: /pm name|id message",
                     "private message reports missing text");
    result = commands.Execute("/clear");
    passed &= Expect(result.clearHistory && result.notice.empty(),
                     "clear command requests UI-owned history deletion");
    result = commands.Execute("/disconnect");
    passed &= Expect(interaction.disconnects == 1 &&
                         result.notice == "disconnected",
                     "disconnect command reaches the semantic port");
    result = commands.Execute("/unknown");
    passed &= Expect(result.notice == "unknown command: /unknown",
                     "unknown command is reported deterministically");

    return passed ? 0 : 1;
}
