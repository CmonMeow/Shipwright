#include "ServerGameplayPacketIngress.h"

#include "FishingNetworkAdapter.h"
#include "PlayerSimulationNetworkAdapter.h"
#include "ProjectileNetworkAdapter.h"
#include "SceneNetworkAdapter.h"
#include "ServerGameplayCommandService.h"
#include "WorldPvpNetworkAdapter.h"

namespace Game::Multiplayer {

ServerGameplayPacketIngress::ServerGameplayPacketIngress(
    ServerGameplayCommandService& commands)
    : mCommands(commands) {
}

void ServerGameplayPacketIngress::EnterScene(
    int32_t sender, NetworkSceneEntryIntentPacket packet) {
    if (sender < 0 || !SceneNetworkAdapter::IsSane(packet)) return;
    mCommands.ExecuteSceneEntry(sender, SceneNetworkAdapter::ToCommand(packet));
}

void ServerGameplayPacketIngress::SubmitPlayerCommand(
    int32_t sender, NetworkPlayerCommandPacket packet) {
    if (sender < 0 || !PlayerSimulationNetworkAdapter::IsSane(packet)) return;
    mCommands.SubmitPlayerCommand(
        sender, PlayerSimulationNetworkAdapter::ToCommand(packet));
}

void ServerGameplayPacketIngress::SelectWeapon(
    int32_t sender, NetworkWeaponSelectionIntentPacket packet) {
    if (sender < 0 || !PlayerSimulationNetworkAdapter::IsSane(packet)) return;
    mCommands.SelectWeapon(
        sender, PlayerSimulationNetworkAdapter::ToCommand(packet));
}

void ServerGameplayPacketIngress::SubmitStructureAction(
    int32_t sender, NetworkStructureActionPacket packet) {
    if (sender < 0 || !WorldPvpNetworkAdapter::IsSane(packet)) return;
    mCommands.ExecuteStructureAction(
        sender, WorldPvpNetworkAdapter::ToCommand(packet));
}

void ServerGameplayPacketIngress::SubmitFishingPresentation(
    int32_t sender, NetworkFishingPresentationIntentPacket packet) {
    if (sender < 0 || !FishingNetworkAdapter::IsSane(packet)) return;
    mCommands.ExecuteFishingPresentation(
        sender, FishingNetworkAdapter::ToIntent(packet));
}

void ServerGameplayPacketIngress::SubmitFishAction(
    int32_t sender, NetworkFishIntentPacket packet) {
    if (sender < 0 || !FishingNetworkAdapter::IsSane(packet)) return;
    mCommands.ExecuteFishAction(sender, FishingNetworkAdapter::ToCommand(packet));
}

void ServerGameplayPacketIngress::SubmitLureControl(
    int32_t sender, NetworkLureControlIntentPacket packet) {
    if (sender < 0 || !FishingNetworkAdapter::IsSane(packet)) return;
    mCommands.ExecuteLureControl(sender, FishingNetworkAdapter::ToCommand(packet));
}

void ServerGameplayPacketIngress::FireProjectile(
    int32_t sender, NetworkArrowFireIntentPacket packet) {
    if (sender < 0 || !ProjectileNetworkAdapter::IsSane(packet)) return;
    const Game::Simulation::ArrowFireCommand command =
        ProjectileNetworkAdapter::ToCommand(packet);
    const Game::Simulation::ArrowFireDecision decision =
        mCommands.ExecuteArrowFire(sender, command);
    mCommands.SendProjectileIntentResult(
        sender, command.sequence, command.lifeEpoch, decision.projectileId,
        NETWORK_PROJECTILE_INTENT_ARROW_FIRE, decision.accepted);
}

} // namespace Game::Multiplayer
