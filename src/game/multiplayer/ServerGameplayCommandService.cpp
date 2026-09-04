#include "ServerGameplayCommandService.h"

#include "PlayerLifecycleNetworkAdapter.h"
#include "ProjectileNetworkAdapter.h"
#include "SceneNetworkAdapter.h"

#include <utility>

namespace Game::Multiplayer {

namespace {

const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

ServerGameplayCommandService::ServerGameplayCommandService(
    Game::Simulation::ServerWorld& world,
    ClientReplicationInbox& clientInbox,
    ServerReplicationInterestPublisher& interestPublisher,
    ServerReplicationEventPublisher& eventPublisher)
    : mWorld(world), mIngress(world), mClientInbox(clientInbox),
      mInterestPublisher(interestPublisher), mEventPublisher(eventPublisher) {
}

void ServerGameplayCommandService::SetDelivery(
    ServerReplicationDelivery delivery) {
    mDelivery = std::move(delivery);
}

void ServerGameplayCommandService::Deliver(
    int32_t player, NetAppMessageType type, const NetworkMessageRaw& raw,
    NetMsgFlags flags) const {
    if (player > 0 && mDelivery.send) {
        mDelivery.send(player, type, raw, flags, {});
    }
}

bool ServerGameplayCommandService::SubmitPlayerCommand(
    int32_t player, Game::Simulation::PlayerCommand command) {
    return player >= 0 && mIngress.SubmitPlayerCommand(player, command);
}

bool ServerGameplayCommandService::SelectWeapon(
    int32_t player, Game::Simulation::WeaponSelectionCommand command) {
    return player >= 0 && mIngress.ExecuteWeaponSelection(player, command);
}

bool ServerGameplayCommandService::ExecuteFishingPresentation(
    int32_t player, Game::Replication::FishingPresentationIntent intent) {
    if (player < 0) return false;
    const auto admitted =
        mIngress.AdmitFishingPresentation(player, std::move(intent));
    return admitted && mEventPublisher.PublishFishingPresentation(
                           admitted->presentation,
                           admitted->authoritativePlayer);
}

bool ServerGameplayCommandService::ExecuteLureControl(
    int32_t player, Game::Simulation::LureControlCommand command) {
    if (player < 0 || !mIngress.ExecuteLureControl(player, command)) return false;
    mInterestPublisher.RefreshOwnedEntities();
    if (command.deployed) mEventPublisher.PublishFishingEvents();
    return true;
}

bool ServerGameplayCommandService::ExecuteFishAction(
    int32_t player, Game::Simulation::FishActionCommand command) {
    if (player < 0 || !mIngress.ExecuteFishAction(player, command)) return false;
    mInterestPublisher.RefreshOwnedEntities();
    return true;
}

bool ServerGameplayCommandService::ExecuteStructureAction(
    int32_t player, Game::Simulation::StructureActionCommand command) {
    if (player < 0) return false;
    const auto decision = mIngress.ExecuteStructureAction(player, command);
    if (!decision.Accepted()) return false;
    if (decision.structure) {
        mEventPublisher.PublishStructureState(*decision.structure);
    }
    return true;
}

void ServerGameplayCommandService::SendSceneEntryState(
    int32_t player, uint32_t requestSequence, bool accepted) {
    const auto snapshot = mWorld.PlayerFor(player);
    if (!snapshot) return;
    const NetworkSceneEntryStatePacket packet =
        SceneNetworkAdapter::ToPacket(*snapshot, requestSequence, accepted);
    if (player == 0) {
        mClientInbox.AcceptSceneEntryState(packet, player, snapshot->lifeEpoch);
    } else {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        Deliver(player, NAMTSceneEntryState, raw, kReliable);
    }
}

bool ServerGameplayCommandService::ExecuteSceneEntry(
    int32_t player, Game::Simulation::SceneEntryCommand command) {
    if (player < 0) return false;
    const auto outcome = mIngress.ExecuteSceneEntry(player, command);
    if (!outcome) return false;
    if (!outcome->accepted) {
        SendSceneEntryState(player, command.sequence, false);
        return false;
    }
    // Terminal state from the old scene precedes the admission reply. New-scene
    // lifecycles follow the reply on the same reliable stream.
    if (outcome->changedScene) mEventPublisher.PublishProjectileEvents();
    if (!outcome->player) return false;
    if (player == 0 && outcome->admitted) {
        const Game::Replication::ReplicatedPlayer localPlayer{
            0, outcome->player->entity, outcome->player->sceneId,
            outcome->player->position
        };
        mClientInbox.AcceptPlayerLifecycle(
            PlayerLifecycleNetworkAdapter::ToPacket(localPlayer, true));
    }
    SendSceneEntryState(player, command.sequence, true);
    if (outcome->changedScene || (player == 0 && outcome->admitted)) {
        mInterestPublisher.RefreshAll();
    }
    return true;
}

Game::Simulation::ArrowFireDecision
ServerGameplayCommandService::ExecuteArrowFire(
    int32_t player, Game::Simulation::ArrowFireCommand command) {
    if (player < 0) return {};
    const auto decision = mIngress.ExecuteArrowFire(player, command);
    if (decision.accepted) mEventPublisher.PublishProjectileEvents();
    return decision;
}

void ServerGameplayCommandService::SendProjectileIntentResult(
    int32_t player, uint32_t sequence, uint32_t lifeEpoch,
    int32_t projectileId, uint8_t intentKind, bool accepted) {
    NetworkProjectileIntentResultPacket packet{
        sequence, lifeEpoch, projectileId, intentKind,
        static_cast<uint8_t>(accepted)
    };
    if (player < 0 || !ProjectileNetworkAdapter::IsSane(packet)) return;
    if (player == 0) {
        const auto local = mWorld.PlayerFor(0);
        mClientInbox.AcceptProjectileIntentResult(
            packet, local ? local->lifeEpoch : 0);
    } else {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        Deliver(player, NAMTProjectileIntentResult, raw, kReliable);
    }
}

} // namespace Game::Multiplayer
