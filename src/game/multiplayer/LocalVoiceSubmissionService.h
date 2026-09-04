#pragma once

#include "NetworkProtocol.h"
#include "platform/client/LocalVoiceFrameStream.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Game::Multiplayer {

enum class LocalVoiceSubmissionRole : uint8_t {
    Inactive,
    Client,
    Host,
};

struct LocalVoiceSubmissionDelivery {
    std::function<LocalVoiceSubmissionRole()> role;
    std::function<bool(const NetworkVoiceIntentPacket&)> sendToServer;
    std::function<std::vector<int32_t>()> hostObservers;
    std::function<bool(int32_t, const std::string&)> sendHostPayload;
};

// Owns local voice identity and role-dependent routing. Capture supplies only
// one encoded Opus frame; transport supplies delivery callbacks. NetworkRuntime
// therefore cannot manufacture protocol metadata or duplicate host fan-out.
class LocalVoiceSubmissionService final {
  public:
    explicit LocalVoiceSubmissionService(LocalVoiceSubmissionDelivery delivery);

    bool Submit(std::vector<uint8_t> opusData);
    void Reset();

  private:
    LocalVoiceSubmissionDelivery mDelivery;
    Game::Client::LocalVoiceFrameStream mFrames;
};

} // namespace Game::Multiplayer
