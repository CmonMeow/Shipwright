#include "LocalGameplayCommandService.h"

#include "ClientSessionIngress.h"
#include "FishingNetworkAdapter.h"
#include "NetworkProtocol.h"
#include "PlayerSimulationNetworkAdapter.h"
#include "ProjectileNetworkAdapter.h"
#include "SecureTransportChannel.h"
#include "ServerGameplayCommandService.h"
#include "SceneNetworkAdapter.h"
#include "WorldPvpNetworkAdapter.h"
#include "platform/simulation/ServerWorld.h"

#include <utility>

namespace Game::Multiplayer {

namespace {

const NetMsgFlags kReliable =
    static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);

} // namespace

LocalGameplayCommandService::LocalGameplayCommandService(
    ClientSessionIngress& clientIngress,
    SecureTransportChannel& transport,
    ServerGameplayCommandService& serverCommands,
    Game::Simulation::ServerWorld& serverWorld,
    LocalGameplayEndpointState endpointState)
    : mClientIngress(clientIngress), mTransport(transport),
      mServerCommands(serverCommands), mServerWorld(serverWorld),
      mEndpointState(std::move(endpointState)) {
}

bool LocalGameplayCommandService::SubmitPlayerCommand(
    Game::Simulation::PlayerCommand command, uint32_t expectedLifeEpoch) {
    const uint32_t currentLifeEpoch = CurrentLifeEpoch();
    if (expectedLifeEpoch != 0 && expectedLifeEpoch != currentLifeEpoch) return false;
    command.lifeEpoch = currentLifeEpoch;
    if (!PlayerSimulationNetworkAdapter::IsSane(command)) return false;
    const NetworkPlayerCommandPacket packet =
        PlayerSimulationNetworkAdapter::ToPacket(command);
    if (!PlayerSimulationNetworkAdapter::IsSane(packet)) return false;

    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        const NetMsgFlags flags =
            packet.pressedActions != 0 ? kReliable : NMFHighPriority;
        return LocalPlayerAdmitted() &&
               mTransport.SendToServer(NAMTPlayerIntent, raw, flags);
    }
    return IsListenServer() && mServerCommands.SubmitPlayerCommand(
                                   0, PlayerSimulationNetworkAdapter::ToCommand(packet));
}

bool LocalGameplayCommandService::SelectWeapon(
    const Game::Client::LocalWeaponSelectionRequest& request) {
    NetworkWeaponSelectionIntentPacket packet{};
    packet.sequence = request.sequence;
    packet.selectedWeapon = request.selectedWeapon;
    packet.lifeEpoch = CurrentLifeEpoch();
    if (!LocalPlayerAdmitted() ||
        !PlayerSimulationNetworkAdapter::IsSane(packet)) {
        return false;
    }
    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        return mTransport.SendToServer(NAMTWeaponSelectionIntent, raw, kReliable);
    }
    return IsListenServer() && mServerCommands.SelectWeapon(
                                   0, PlayerSimulationNetworkAdapter::ToCommand(packet));
}

bool LocalGameplayCommandService::EnterScene(
    const Game::Client::LocalSceneEntryRequest& request) {
    NetworkSceneEntryIntentPacket packet{};
    packet.sequence = request.sequence;
    packet.lifeEpoch = CurrentLifeEpoch();
    if (packet.lifeEpoch == 0 && IsListenServer() && !mServerWorld.PlayerFor(0)) {
        // A listen server has no transport identity bootstrap. Its first scene
        // request explicitly creates life epoch one.
        packet.lifeEpoch = 1;
    }
    if (!SceneNetworkAdapter::IsSane(packet)) return false;
    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        return LocalPlayerAdmitted() &&
               mTransport.SendToServer(NAMTSceneEntryIntent, raw, kReliable);
    }
    return IsListenServer() && mServerCommands.ExecuteSceneEntry(
                                   0, SceneNetworkAdapter::ToCommand(packet));
}

bool LocalGameplayCommandService::SubmitFishingPresentation(
    const Game::Replication::FishingPresentationState& presentation) {
    NetworkFishingPresentationIntentPacket packet =
        FishingNetworkAdapter::ToIntentPacket(presentation);
    packet.lifeEpoch = CurrentLifeEpoch();
    if (!FishingNetworkAdapter::IsSane(packet)) return false;
    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeFishingIntentRaw(raw, packet);
        return LocalPlayerAdmitted() &&
               mTransport.SendToServer(NAMTFishingState, raw, NMFHighPriority);
    }
    return IsListenServer() && mServerCommands.ExecuteFishingPresentation(
                                   0, FishingNetworkAdapter::ToIntent(packet));
}

bool LocalGameplayCommandService::SubmitFishAction(
    const Game::Client::LocalFishIntent& intent) {
    if (intent.request.action != Game::Client::LocalFishIntentAction::Hook &&
        intent.request.action != Game::Client::LocalFishIntentAction::Release) {
        return false;
    }
    NetworkFishIntentPacket packet{};
    packet.sequence = intent.sequence;
    packet.action = intent.request.action == Game::Client::LocalFishIntentAction::Hook
                        ? NETWORK_FISH_INTENT_HOOK
                        : NETWORK_FISH_INTENT_RELEASE;
    packet.lifeEpoch = CurrentLifeEpoch();
    if (!FishingNetworkAdapter::IsSane(packet)) return false;
    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        return LocalPlayerAdmitted() &&
               mTransport.SendToServer(NAMTFishIntent, raw, kReliable);
    }
    return IsListenServer() && mServerCommands.ExecuteFishAction(
                                   0, FishingNetworkAdapter::ToCommand(packet));
}

bool LocalGameplayCommandService::SubmitLureControl(
    const Game::Client::LocalLureControlIntent& intent) {
    NetworkLureControlIntentPacket packet{};
    packet.sequence = intent.sequence;
    if (intent.deployed) packet.controlFlags |= NETWORK_LURE_DEPLOYED;
    if (intent.reelHeld) packet.controlFlags |= NETWORK_LURE_REEL_HELD;
    packet.lifeEpoch = CurrentLifeEpoch();
    if (!FishingNetworkAdapter::IsSane(packet)) return false;
    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        return LocalPlayerAdmitted() &&
               mTransport.SendToServer(
                   NAMTLureControlIntent, raw,
                   intent.lifecycleTransition ? kReliable : NMFHighPriority);
    }
    return IsListenServer() && mServerCommands.ExecuteLureControl(
                                   0, FishingNetworkAdapter::ToCommand(packet));
}

bool LocalGameplayCommandService::FireProjectile(
    const Game::Client::LocalProjectileIntent& intent) {
    if (intent.kind != Game::Client::LocalProjectileIntentKind::FireArrow) {
        return false;
    }
    NetworkArrowFireIntentPacket packet{};
    packet.sequence = intent.sequence;
    packet.lifeEpoch = CurrentLifeEpoch();
    if (!ProjectileNetworkAdapter::IsSane(packet)) return false;
    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        return LocalPlayerAdmitted() &&
               mTransport.SendToServer(NAMTArrowFireIntent, raw, kReliable);
    }
    if (!IsListenServer()) return false;
    const Game::Simulation::ArrowFireCommand command =
        ProjectileNetworkAdapter::ToCommand(packet);
    const Game::Simulation::ArrowFireDecision decision =
        mServerCommands.ExecuteArrowFire(0, command);
    mServerCommands.SendProjectileIntentResult(
        0, command.sequence, command.lifeEpoch, decision.projectileId,
        NETWORK_PROJECTILE_INTENT_ARROW_FIRE, decision.accepted);
    return true;
}

bool LocalGameplayCommandService::SubmitStructureAction(
    const Game::Client::LocalStructureAction& action) {
    uint8_t wireAction = 0;
    switch (action.request.kind) {
        case Game::Client::LocalStructureActionKind::Build:
            wireAction = NETWORK_STRUCTURE_ACTION_BUILD;
            break;
        case Game::Client::LocalStructureActionKind::Repair:
            wireAction = NETWORK_STRUCTURE_ACTION_REPAIR;
            break;
        default:
            return false;
    }
    const NetworkStructureActionPacket packet{
        action.sequence,
        CurrentLifeEpoch(),
        action.request.structureKey,
        wireAction,
    };
    if (!WorldPvpNetworkAdapter::IsSane(packet)) return false;
    if (IsRemoteClient()) {
        NetworkMessageRaw raw;
        EncodeAppPacketRaw(raw, packet);
        return LocalPlayerAdmitted() &&
               mTransport.SendToServer(NAMTStructureAction, raw, kReliable);
    }
    return IsListenServer() && mServerCommands.ExecuteStructureAction(
                                   0, WorldPvpNetworkAdapter::ToCommand(packet));
}

bool LocalGameplayCommandService::IsRemoteClient() const {
    return mEndpointState.remoteClientActive &&
           mEndpointState.remoteClientActive();
}

bool LocalGameplayCommandService::IsListenServer() const {
    return mEndpointState.listenServerActive &&
           mEndpointState.listenServerActive();
}

bool LocalGameplayCommandService::LocalPlayerAdmitted() const {
    return IsListenServer() || mClientIngress.LocalPlayerId() >= 0;
}

uint32_t LocalGameplayCommandService::CurrentLifeEpoch() const {
    if (IsRemoteClient()) return mClientIngress.LocalLifeEpoch();
    if (!IsListenServer()) return 0;
    const auto authoritative = mServerWorld.PlayerFor(0);
    return authoritative ? authoritative->lifeEpoch : 0;
}

} // namespace Game::Multiplayer
