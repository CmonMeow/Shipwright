#pragma once

#include "NetworkProtocol.h"

#include <cstdint>

namespace SoH::Network {

class ServerGameplayCommandService;

// Wire-facing admission boundary for gameplay packets received from an
// authenticated transport peer. It validates packet shape, converts wire
// values into semantic commands, and invokes server authority exactly once.
class ServerGameplayPacketIngress final {
  public:
    explicit ServerGameplayPacketIngress(
        ServerGameplayCommandService& commands);

    void EnterScene(int32_t sender, NetworkSceneEntryIntentPacket packet);
    void SubmitPlayerCommand(int32_t sender, NetworkPlayerCommandPacket packet);
    void SelectWeapon(int32_t sender,
                      NetworkWeaponSelectionIntentPacket packet);
    void SubmitStructureAction(int32_t sender,
                               NetworkStructureActionPacket packet);
    void SubmitFishingPresentation(
        int32_t sender, NetworkFishingPresentationIntentPacket packet);
    void SubmitFishAction(int32_t sender, NetworkFishIntentPacket packet);
    void SubmitLureControl(int32_t sender,
                           NetworkLureControlIntentPacket packet);
    void FireProjectile(int32_t sender, NetworkArrowFireIntentPacket packet);

  private:
    ServerGameplayCommandService& mCommands;
};

} // namespace SoH::Network
