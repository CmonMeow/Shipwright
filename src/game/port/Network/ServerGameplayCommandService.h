#pragma once

#include "ServerReplicationEventPublisher.h"
#include "../../platform/simulation/ServerGameplayIngress.h"

namespace SoH::Network {

// Authenticated semantic command boundary. Protocol dispatch validates packet
// shape and binds the transport sender; this service performs one authoritative
// ingress operation and publishes only accepted outcomes.
class ServerGameplayCommandService final {
  public:
    ServerGameplayCommandService(
        Game::Simulation::ServerWorld& world,
        ClientReplicationInbox& clientInbox,
        ServerReplicationInterestPublisher& interestPublisher,
        ServerReplicationEventPublisher& eventPublisher);

    void SetDelivery(ServerReplicationDelivery delivery);
    bool SubmitPlayerCommand(int32_t player,
                             Game::Simulation::PlayerCommand command);
    bool SelectWeapon(int32_t player,
                      Game::Simulation::WeaponSelectionCommand command);
    bool ExecuteFishingPresentation(
        int32_t player, Game::Replication::FishingPresentationIntent intent);
    bool ExecuteLureControl(int32_t player,
                            Game::Simulation::LureControlCommand command);
    bool ExecuteFishAction(int32_t player,
                           Game::Simulation::FishActionCommand command);
    bool ExecuteStructureAction(
        int32_t player, Game::Simulation::StructureActionCommand command);
    bool ExecuteSceneEntry(int32_t player,
                           Game::Simulation::SceneEntryCommand command);
    Game::Simulation::ArrowFireDecision ExecuteArrowFire(
        int32_t player, Game::Simulation::ArrowFireCommand command);
    void SendSceneEntryState(int32_t player, uint32_t requestSequence,
                             bool accepted);
    void SendProjectileIntentResult(int32_t player, uint32_t sequence,
                                    uint32_t lifeEpoch, int32_t projectileId,
                                    uint8_t intentKind, bool accepted);

  private:
    void Deliver(int32_t player, NetAppMessageType type,
                 const NetworkMessageRaw& raw, NetMsgFlags flags) const;

    Game::Simulation::ServerWorld& mWorld;
    Game::Simulation::ServerGameplayIngress mIngress;
    ClientReplicationInbox& mClientInbox;
    ServerReplicationInterestPublisher& mInterestPublisher;
    ServerReplicationEventPublisher& mEventPublisher;
    ServerReplicationDelivery mDelivery;
};

} // namespace SoH::Network
