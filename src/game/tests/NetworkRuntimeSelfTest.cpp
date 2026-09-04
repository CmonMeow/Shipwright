#include <sysdef.h>

#include "Network/NetworkRuntime.h"
#include "Network/LocalNetworkIdentity.h"
#include "Network/netTransport.hpp"

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

using SoH::Network::NetworkRuntime;
using SoH::Network::GM_LIST_FILENAME;
using SoH::Network::LoadGameMasterList;
using SoH::Network::LocalIdentityId;
using SoH::Network::SaveGameMasterList;

constexpr unsigned short kRuntimePort = 47778;
constexpr uint64_t kExpectedRespawnMilliseconds = 5000;
constexpr unsigned kDropEveryNthDisposableDatagram = 13;

struct TestPlayerState {
    int32_t playerId{};
    int32_t sceneId{};
    int32_t roomId{};
    int32_t sequence{};
    float x{};
    float y{};
    float z{};
    short rotationX{};
    short rotationY{};
    short rotationZ{};
    short aimPitch{};
    short aimYaw{};
    float speed{};
    uint8_t selectedWeapon{};
    uint8_t fishingState{};
    float fishingRodTipOffset[3]{};
    float fishingLureDrawOffset[3]{};
    float fishingRodBendY{};
    float fishingRodBendX{};
    float fishingRodTwist{};
    float fishingRodCastX{};
    float fishingLureRot[3]{};
    float fishingLureSpin{};
    float fishingLureZOffset{};
    float fishingLureHookOffsets[2][3]{};
    float fishingLureHookRot[2][2]{};
    float fishingLineScale{};
    float fishingLineGravity{};
    uint8_t fishingLineSpooled{};
    uint8_t fishingSinkingLureSegmentIndex{};
    uint8_t fishingSinkingLureUnderwater{};
    short fishingFishRot[3]{};
    short fishingFishLimbRot[8]{};
};

Game::Replication::FishingPresentationState TestFishingPresentation(
    const TestPlayerState& state) {
    Game::Replication::FishingPresentationState presentation{};
    presentation.sequence = state.sequence;
    presentation.state = state.fishingState;
    memcpy(presentation.rodTipOffset.data(), state.fishingRodTipOffset,
           sizeof(state.fishingRodTipOffset));
    memcpy(presentation.lureDrawOffset.data(), state.fishingLureDrawOffset,
           sizeof(state.fishingLureDrawOffset));
    presentation.rodBendY = state.fishingRodBendY;
    presentation.rodBendX = state.fishingRodBendX;
    presentation.rodTwist = state.fishingRodTwist;
    presentation.rodCastX = state.fishingRodCastX;
    memcpy(presentation.lureRotation.data(), state.fishingLureRot,
           sizeof(state.fishingLureRot));
    presentation.lureSpin = state.fishingLureSpin;
    presentation.lureZOffset = state.fishingLureZOffset;
    memcpy(presentation.lureHookOffsets.data(), state.fishingLureHookOffsets,
           sizeof(state.fishingLureHookOffsets));
    memcpy(presentation.lureHookRotations.data(), state.fishingLureHookRot,
           sizeof(state.fishingLureHookRot));
    presentation.lineScale = state.fishingLineScale;
    presentation.lineGravity = state.fishingLineGravity;
    presentation.lineSpooled = state.fishingLineSpooled;
    presentation.sinkingLureSegmentIndex = state.fishingSinkingLureSegmentIndex;
    presentation.sinkingLureUnderwater = state.fishingSinkingLureUnderwater;
    memcpy(presentation.fishRotation.data(), state.fishingFishRot,
           sizeof(state.fishingFishRot));
    memcpy(presentation.fishLimbRotation.data(), state.fishingFishLimbRot,
           sizeof(state.fishingFishLimbRot));
    return presentation;
}

uint32_t FishingIdentityHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    return value ^ (value >> 16);
}

float TestPondFishLength() {
    // The deterministic inward cast settles nearest pond fish at this stable
    // map spawn. Native actor parameters are deliberately not part of identity.
    const uint32_t spawnKey =
        Game::Simulation::MakeFishSpawnKey(0x49, 3, 615, -45, -450);
    const uint32_t seed = 0x49U * 0x9E3779B9U ^ spawnKey * 0x85EBCA6BU ^
                          615U * 0xC2B2AE35U ^
                          static_cast<uint32_t>(-450) * 0x27D4EB2FU;
    return 35.0f + static_cast<float>(FishingIdentityHash(seed) & 0x00FFFFFF) /
                       16777216.0f * 4.99999f;
}

TestPlayerState MakeState(int value) {
    TestPlayerState packet{};
    // Keep the authority test outside a real scene so static world geometry
    // cannot accidentally mask the explicit projectile/player geometry below.
    packet.sceneId = 110;
    packet.roomId = 3;
    packet.sequence = value;
    packet.x = static_cast<float>(value);
    packet.y = static_cast<float>(value * 2);
    packet.z = static_cast<float>(value * 3);
    packet.rotationY = 12345;
    packet.speed = 2.5f;
    packet.selectedWeapon = 1;
    return packet;
}

bool SendPresentation(NetworkRuntime& network, const TestPlayerState& state) {
    if (state.selectedWeapon == 4) {
        network.SendFishingPresentation(TestFishingPresentation(state));
    }
    return true;
}

Game::Simulation::PlayerCommand MakeCommand(
    uint32_t sequence, uint8_t, int16_t heading,
    uint16_t heldActions = 0, uint16_t pressedActions = 0) {
    constexpr float kBinaryAngleToRadians = 3.14159265358979323846f / 32768.0f;
    Game::Simulation::PlayerCommand command{};
    command.sequence = sequence;
    command.actionSequence = pressedActions != 0 ? sequence : 0;
    command.headingRadians = static_cast<float>(heading) * kBinaryAngleToRadians;
    command.heldActions = heldActions;
    command.pressedActions = pressedActions;
    return command;
}

bool SelectWeapon(NetworkRuntime& network, uint32_t sequence, uint8_t selectedWeapon) {
    Game::Client::LocalWeaponSelectionRequest selection{};
    selection.sequence = sequence;
    selection.selectedWeapon = selectedWeapon;
    return network.SendWeaponSelection(selection);
}

Game::Client::LocalFishIntent MakeFishIntent(int value, unsigned char action) {
    Game::Client::LocalFishIntent intent{};
    intent.sequence = static_cast<uint32_t>(value + action);
    intent.request.action = action == NETWORK_FISH_INTENT_RELEASE
                                ? Game::Client::LocalFishIntentAction::Release
                                : Game::Client::LocalFishIntentAction::Hook;
    return intent;
}

Game::Client::LocalLureControlIntent MakeLureIntent(
    uint32_t sequence, unsigned char controlFlags = NETWORK_LURE_DEPLOYED) {
    return { sequence,
             (controlFlags & NETWORK_LURE_DEPLOYED) != 0,
             (controlFlags & NETWORK_LURE_REEL_HELD) != 0,
             true };
}

std::vector<uint8_t> MakeVoice(int value) {
    return std::vector<uint8_t>(4 + VOICE_SAMPLES_PER_PACKET / 2,
                                static_cast<uint8_t>(value));
}

int RunHost() {
    std::vector<std::string> originalGameMasters;
    const bool gameMasterListExisted = GetFileAttributesA(GM_LIST_FILENAME) != INVALID_FILE_ATTRIBUTES;
    LoadGameMasterList(originalGameMasters);
    std::vector<std::string> testGameMasters = originalGameMasters;
    AddUniqueString(testGameMasters, LocalIdentityId());
    SaveGameMasterList(testGameMasters);
    NetworkRuntime network;
    if (!network.Host(kRuntimePort, "Game secure runtime test")) {
        if (gameMasterListExisted) {
            SaveGameMasterList(originalGameMasters);
        } else {
            DeleteFileA(GM_LIST_FILENAME);
        }
        return 10;
    }
    if (!network.ConfigureSceneSpawn({ 0x49, { 666.0f, -87.0f, 354.0f }, 0.0f }) ||
        !network.ConfigureSceneSpawn({ 101, {}, 0.0f }) ||
        !network.ConfigureSceneSpawn({ 110, {}, 0.0f })) {
        network.Disconnect();
        return 14;
    }
    Game::Client::LocalSceneEntryRequest hostEntry{};
    hostEntry.sequence = 1;
    hostEntry.sceneId = 0x49;
    if (!network.SendSceneEntryIntent(hostEntry)) {
        network.Disconnect();
        return 15;
    }
    // The host owns the origin. Subsequent clients enter one Link body
    // diameter behind it so scripted arrows/melee exercise collision and
    // damage on a deterministic centerline rather than spawning overlapped.
    if (!network.ConfigureSceneSpawn({ 110, { 0.0f, 0.0f, -24.0f }, 0.0f })) {
        network.Disconnect();
        return 16;
    }
    if (!network.EnsureStrategicSite(
            { 7001, 110, {}, 2000.0f, Game::Simulation::TeamId::Neutral },
            Game::Simulation::StrategicSiteKind::Keep, 1).Valid() ||
        !network.EnsureStrategicSite(
            { 7000, 110, { 3000.0f, 0.0f, 3000.0f }, 200.0f,
              Game::Simulation::TeamId::Red },
             Game::Simulation::StrategicSiteKind::Camp, 2).Valid() ||
        !network.EnsureSupplyRoute({ 9000, 7000, 7001 }) ||
        !network.EnsureInfluenceAdjacency({ 9001, 1, 2 })) {
        network.Disconnect();
        if (gameMasterListExisted) {
            SaveGameMasterList(originalGameMasters);
        } else {
            DeleteFileA(GM_LIST_FILENAME);
        }
        return 12;
    }
    if (!network.EnsureStructure({ 8001, 7001, 110, {}, 500, 50 }).Valid()) {
        network.Disconnect();
        if (gameMasterListExisted) {
            SaveGameMasterList(originalGameMasters);
        } else {
            DeleteFileA(GM_LIST_FILENAME);
        }
        return 13;
    }

    bool chatReceived = false;
    bool privateReceived = false;
    bool stateReceived = false;
    bool fishingVisualReceived = false;
    bool pondPlayerSnapshotReceived = false;
    bool outOfInterestPlayerSnapshotReceived = false;
    bool hostCombatSceneEntered = false;
    bool hostWitnessSceneEntered = false;
    bool hostReturnedFromWitnessScene = false;
    bool hostAuthoritativeSnapshotReceived = false;
    bool hostObjectiveStateReceived = false;
    bool hostStrategicTopologyReceived = false;
    bool hostStructureStateReceived = false;
    bool poseEquipmentReceived = false;
    bool bowStringScaleReceived = false;
    bool voiceReceived = false;
    bool fishHookReceived = false;
    bool lureStateReceived = false;
    bool fishTelemetryCouldNotHook = false;
    bool fishTelemetryAcknowledged = false;
    bool fishHookAcknowledged = false;
    bool fishCanonicalStateReceived = false;
    bool fishCanonicalAcknowledged = false;
    bool fishReleaseReceived = false;
    bool projectileImpactWitnessed = false;
    bool projectileImpactAcknowledged = false;
    bool projectileRetiredUnexpectedly = false;
    bool arrowNativeDisplayPitchReceived = false;
    bool arrowAimedDisplayPitchReceived = false;
    bool arrowStuckDisplayPitchReceived = false;
    bool arrowDamageReceived = false;
    bool arrowDamageAcknowledged = false;
    bool clientMeleeDamageReceived = false;
    bool clientMeleeDamageAcknowledged = false;
    bool hostMeleeSent = false;
    bool hostMeleeWeaponSelected = false;
    bool clientSawHostMeleeDamage = false;
    bool witnessArrowReady = false;
    bool corpseReceived = false;
    bool responseSent = false;
    bool clientComplete = false;
    bool clientCompleteAcknowledged = false;
    bool deathRequested = false;
    bool pondTransitionRequested = false;
    bool pondTransitionGranted = false;
    bool combatTransitionRequested = false;
    bool combatTransitionGranted = false;
    bool interestOutboundRequested = false;
    bool interestOutboundGranted = false;
    bool interestReturnRequestedByClient = false;
    bool interestReturnGranted = false;
    int deathAttacksSent = 0;
    unsigned __int64 projectileStuckAt = 0;
    unsigned __int64 lastDeathAttackAt = 0;
    unsigned __int64 clientCompleteAt = 0;
    uint32_t hostCommandSequence = 1;
    uint32_t hostWeaponSelectionSequence = 1;
    const bool hostBaselineWeaponSelected =
        SelectWeapon(network, hostWeaponSelectionSequence++, 1);
    Game::Simulation::PlayerCommand hostBaselineCommand = MakeCommand(hostCommandSequence++, 1, 0);
    const bool mismatchedPredictionLifeRejected =
        !network.SendPlayerCommand(hostBaselineCommand, UINT32_MAX);
    const bool hostBaselineCommandSent = hostBaselineWeaponSelected &&
                                         network.SendPlayerCommand(hostBaselineCommand);
    bool transportFaultsActive = false;
    const auto authorizeRemoteTransition = [&](int32_t sceneId) {
        for (const auto& player : network.Players()) {
            if (player.playerId > 0 &&
                network.AuthorizeSceneTransition(player.playerId, sceneId)) {
                return true;
            }
        }
        return false;
    };

    const unsigned __int64 timeout = GetTickCount64() + 50000;
    while (GetTickCount64() < timeout) {
        network.Update();
        if (!transportFaultsActive && network.IsSecure()) {
            ConfigureNetworkTestPacketLoss(kDropEveryNthDisposableDatagram, true);
            transportFaultsActive = true;
        }

        NetworkChatLine line;
        while (network.PollChat(line)) {
            chatReceived = chatReceived || line.text.find("runtime-client-chat") != std::string::npos;
            privateReceived = privateReceived || line.text.find("runtime-client-private") != std::string::npos;
            clientComplete = clientComplete || line.text.find("runtime-client-complete") != std::string::npos;
            if (clientComplete && clientCompleteAt == 0) clientCompleteAt = GetTickCount64();
            deathRequested = deathRequested || line.text.find("runtime-request-death") != std::string::npos;
            clientSawHostMeleeDamage = clientSawHostMeleeDamage ||
                                       line.text.find("runtime-host-melee-damage") != std::string::npos;
            pondTransitionRequested = pondTransitionRequested ||
                line.text.find("runtime-request-pond-transition") != std::string::npos;
            combatTransitionRequested = combatTransitionRequested ||
                line.text.find("runtime-request-combat-transition") != std::string::npos;
            interestOutboundRequested = interestOutboundRequested ||
                line.text.find("runtime-request-interest-outbound") != std::string::npos;
            interestReturnRequestedByClient = interestReturnRequestedByClient ||
                line.text.find("runtime-request-interest-return") != std::string::npos;
        }
        if (pondTransitionRequested && !pondTransitionGranted &&
            authorizeRemoteTransition(0x49) &&
            network.AuthorizeSceneTransition(0, 0x49)) {
            Game::Client::LocalSceneEntryRequest hostPondEntry{};
            hostPondEntry.sequence = 2;
            hostPondEntry.sceneId = 0x49;
            pondTransitionGranted = network.SendSceneEntryIntent(hostPondEntry) &&
                                    network.SendChat("runtime-pond-transition-granted");
        }
        if (interestOutboundRequested && !interestOutboundGranted &&
            authorizeRemoteTransition(101)) {
            interestOutboundGranted = network.SendChat("runtime-interest-outbound-granted");
        }
        if (combatTransitionRequested && !combatTransitionGranted &&
            authorizeRemoteTransition(110)) {
            combatTransitionGranted =
                network.SendChat("runtime-combat-transition-granted");
        }
        if (interestReturnRequestedByClient && !interestReturnGranted &&
            authorizeRemoteTransition(110)) {
            interestReturnGranted = network.SendChat("runtime-interest-return-granted");
        }
        if (clientComplete && !clientCompleteAcknowledged) {
            clientCompleteAcknowledged = network.SendChat("runtime-complete-ack");
        }

        Game::Simulation::PlayerSnapshot state{};
        while (network.PollPlayerSnapshot(state)) {
            hostAuthoritativeSnapshotReceived = hostAuthoritativeSnapshotReceived ||
                                                (state.ownerPlayerId == 0 &&
                                                 state.lastProcessedCommand >=
                                                     hostBaselineCommand.sequence);
            pondPlayerSnapshotReceived = pondPlayerSnapshotReceived ||
                                         (state.ownerPlayerId > 0 && state.sceneId == 0x49);
            outOfInterestPlayerSnapshotReceived = outOfInterestPlayerSnapshotReceived ||
                                                  (hostCombatSceneEntered && state.ownerPlayerId > 0 &&
                                                   state.sceneId == 0x49);
            stateReceived = stateReceived || (state.ownerPlayerId > 0 && state.entity.Valid() &&
                                              state.sceneId == 110);
            bowStringScaleReceived = bowStringScaleReceived || state.ownerPlayerId > 0;
            poseEquipmentReceived = poseEquipmentReceived ||
                                    (state.ownerPlayerId > 0 && state.entity.Valid());
        }
        Game::Client::ReplicatedObjectiveState hostObjective{};
        while (network.PollObjectiveState(hostObjective)) {
            hostObjectiveStateReceived = hostObjectiveStateReceived ||
                                         (hostObjective.snapshot.objectiveKey == 7001 &&
                                          hostObjective.snapshot.sceneId == 110 &&
                                          hostObjective.snapshot.entity.Valid() &&
                                          hostObjective.active);
        }
        Game::Client::ReplicatedStrategicTopologyState hostTopology{};
        while (network.PollStrategicTopology(hostTopology)) {
            hostStrategicTopologyReceived = hostStrategicTopologyReceived ||
                (hostTopology.sites.size() == 2 &&
                 hostTopology.supplyRoutes.size() == 1 &&
                 hostTopology.influenceAdjacencies.size() == 1);
        }
        Game::Client::ReplicatedStructureState hostStructure{};
        while (network.PollStructureState(hostStructure)) {
            hostStructureStateReceived = hostStructureStateReceived ||
                                         (hostStructure.snapshot.structureKey == 8001 &&
                                          hostStructure.snapshot.sceneId == 110 &&
                                          hostStructure.snapshot.entity.Valid() &&
                                          hostStructure.active);
        }
        Game::Client::CorpsePresentationState corpse{};
        while (network.PollCorpseState(corpse)) {
            corpseReceived = corpseReceived ||
                             (corpse.active && corpse.entity.Valid() &&
                              corpse.sourcePlayerEntity.Valid() &&
                              corpse.sourceLifeEpoch != 0);
        }
        Game::Replication::FishingPresentationState fishingState{};
        while (network.PollFishingPresentation(fishingState)) {
            const bool serverOwnedLureEndpoint =
                std::isfinite(fishingState.lureDrawOffset[0]) &&
                std::isfinite(fishingState.lureDrawOffset[1]) &&
                std::isfinite(fishingState.lureDrawOffset[2]) &&
                std::fabs(fishingState.lureDrawOffset[0] - 17.25f) > 0.001f;
            fishingVisualReceived = fishingVisualReceived ||
                                    (fishingState.playerId > 0 && fishingState.entity.Valid() &&
                                     fishingState.sceneId == 0x49 &&
                                     (fishingState.state == 1 || fishingState.state == 3 ||
                                      fishingState.state == 4) &&
                                     serverOwnedLureEndpoint &&
                                     fishingState.lureSpin == 0.375f &&
                                     fishingState.lureZOffset == -725.0f &&
                                     fishingState.rodTipOffset[0] == 300.0f &&
                                     fishingState.sinkingLureUnderwater == 1 &&
                                     fishingState.lineGravity == 2.25f);
            fishTelemetryCouldNotHook = fishTelemetryCouldNotHook ||
                                        (fishingState.playerId > 0 && !fishHookReceived);
        }
        if (fishTelemetryCouldNotHook && !fishTelemetryAcknowledged) {
            fishTelemetryAcknowledged = network.SendChat("runtime-fish-telemetry-rejected");
        }
        Game::Client::RemoteLureEntity lureState{};
        while (network.PollLureState(lureState)) {
            lureStateReceived = lureStateReceived ||
                                (lureState.ownerPlayerId > 0 && lureState.sceneId == 0x49);
        }
        NetworkVoicePacket voice;
        while (network.PollVoice(voice)) {
            voiceReceived = voice.playerId > 0 && voice.sequence == 1 && !voice.data.empty();
        }
        Game::Client::RemoteFishEntity fishState{};
        while (network.PollFishState(fishState)) {
            fishHookReceived = fishHookReceived ||
                               (fishState.ownerPlayerId > 0 && fishState.active &&
                                fishState.entity.Valid());
            fishCanonicalStateReceived = fishCanonicalStateReceived ||
                                         (fishState.ownerPlayerId > 0 && fishState.active &&
                                          fishState.species ==
                                              Game::Simulation::FishSpecies::HylianBass &&
                                          std::fabs(fishState.length - TestPondFishLength()) < 0.001f &&
                                          fishState.identity.spawnKey ==
                                              Game::Simulation::MakeFishSpawnKey(
                                                  0x49, 3, 615, -45, -450));
            fishReleaseReceived = fishReleaseReceived ||
                                  (fishState.ownerPlayerId > 0 && !fishState.active &&
                                   fishState.entity.Valid());
        }
        Game::Client::RemoteProjectileReplicaState projectile{};
        while (network.PollProjectileState(projectile)) {
            // Native EnArrow keeps launch aim in world.rot.x, but derives the
            // rendered model pitch in shape.rot.x. A level shot must be
            // displayed at zero. The packet deliberately requests a
            // different yaw; authority must
            // keep the accepted Link aim of zero.
            arrowNativeDisplayPitchReceived = arrowNativeDisplayPitchReceived ||
                                              (projectile.logicalId.ownerPlayerId > 0 &&
                                               projectile.logicalId.projectileKind == NETWORK_PROJECTILE_ARROW &&
                                               projectile.phase == Game::Client::RemoteProjectilePhase::ArrowFlying &&
                                               projectile.rotationX == 0 && projectile.rotationY == 0);
            arrowAimedDisplayPitchReceived = arrowAimedDisplayPitchReceived ||
                                             (projectile.logicalId.ownerPlayerId > 0 &&
                                              projectile.logicalId.projectileKind == NETWORK_PROJECTILE_ARROW &&
                                              projectile.phase == Game::Client::RemoteProjectilePhase::ArrowFlying &&
                                              std::abs(static_cast<int>(static_cast<short>(
                                                  projectile.rotationX - 0x1000))) <= 2 &&
                                              projectile.rotationY == 0x4000);
            if (projectile.logicalId.ownerPlayerId > 0 &&
                projectile.logicalId.projectileKind == NETWORK_PROJECTILE_ARROW &&
                projectile.phase == Game::Client::RemoteProjectilePhase::ArrowStuck) {
                projectileImpactWitnessed = true;
                arrowStuckDisplayPitchReceived = arrowStuckDisplayPitchReceived ||
                                                 (std::abs(static_cast<int>(static_cast<short>(
                                                      projectile.rotationX - 0x1000))) <= 0x0800 &&
                                                  projectile.rotationY == 0x4000);
                if (projectileStuckAt == 0) {
                    projectileStuckAt = GetTickCount64();
                }
            }
            if (projectile.logicalId.ownerPlayerId > 0 &&
                projectile.logicalId.projectileKind == NETWORK_PROJECTILE_ARROW &&
                !projectile.active && projectileStuckAt != 0 && !clientComplete &&
                !hostReturnedFromWitnessScene) {
                projectileRetiredUnexpectedly = true;
            }
        }
        Game::Simulation::CombatResultEvent damage{};
        while (network.PollCombatResult(damage)) {
            arrowDamageReceived = arrowDamageReceived ||
                                  (damage.sourcePlayerId > 0 && damage.targetPlayerId == 0 &&
                                   damage.sourceEntity.Valid() && damage.targetEntity.Valid() &&
                                   damage.sceneId == 110 &&
                                   damage.attackKind == Game::Simulation::CombatAttackKind::Arrow &&
                                   damage.result == Game::Simulation::CombatResultKind::Damaged &&
                                   damage.hitRegion != Game::Simulation::PlayerHitRegion::None &&
                                   damage.damage == Game::Simulation::DamageForPlayerHitRegion(
                                                        8, damage.hitRegion) &&
                                   std::isfinite(damage.impactPosition.x) &&
                                   std::isfinite(damage.impactPosition.y) &&
                                   std::isfinite(damage.impactPosition.z));
            clientMeleeDamageReceived = clientMeleeDamageReceived ||
                                        (damage.sourcePlayerId > 0 && damage.targetPlayerId == 0 &&
                                         damage.sourceEntity.Valid() && damage.targetEntity.Valid() &&
                                         damage.sceneId == 110 &&
                                         damage.attackKind == Game::Simulation::CombatAttackKind::Melee &&
                                         damage.result == Game::Simulation::CombatResultKind::Damaged &&
                                         damage.hitRegion != Game::Simulation::PlayerHitRegion::None &&
                                         damage.damage == Game::Simulation::DamageForPlayerHitRegion(
                                                              16, damage.hitRegion));
        }
        if (arrowDamageReceived && !arrowDamageAcknowledged) {
            arrowDamageAcknowledged = network.SendChat("runtime-arrow-damage");
        }
        if (clientMeleeDamageReceived && !clientMeleeDamageAcknowledged) {
            clientMeleeDamageAcknowledged = network.SendChat("runtime-client-melee-damage");
        }
        if (fishHookReceived && !fishHookAcknowledged) {
            fishHookAcknowledged = network.SendChat("runtime-fish-hook");
        }
        if (fishCanonicalStateReceived && !fishCanonicalAcknowledged) {
            fishCanonicalAcknowledged = network.SendChat("runtime-fish-canonical");
        }
        if (clientMeleeDamageAcknowledged && !hostMeleeSent) {
            TestPlayerState hostMelee = MakeState(556);
            hostMelee.x = 0.0f;
            hostMelee.y = 0.0f;
            hostMelee.z = 60.0f;
            hostMelee.selectedWeapon = 2;
            if (!hostMeleeWeaponSelected) {
                hostMeleeWeaponSelected =
                    SelectWeapon(network, hostWeaponSelectionSequence++, 2);
            }
            hostMeleeSent = hostMeleeWeaponSelected && SendPresentation(network, hostMelee) &&
                            network.SendPlayerCommand(MakeCommand(
                                hostCommandSequence++, 2, static_cast<int16_t>(0x8000), 0,
                                NETWORK_ACTION_PRIMARY));
        }
        if (hostMeleeSent && clientSawHostMeleeDamage && !witnessArrowReady) {
            witnessArrowReady = authorizeRemoteTransition(101) &&
                                network.SendChat("runtime-witness-ready");
        }
        if (witnessArrowReady && !hostWitnessSceneEntered) {
            const bool hostSpawnConfigured =
                network.ConfigureSceneSpawn({ 101, {}, 0.0f });
            Game::Client::LocalSceneEntryRequest witnessEntry{};
            witnessEntry.sequence = 4;
            witnessEntry.sceneId = 101;
            const bool hostEntryAccepted = hostSpawnConfigured &&
                                           network.AuthorizeSceneTransition(0, 101) &&
                                           network.SendSceneEntryIntent(witnessEntry);
            const bool clientSpawnRestored = network.ConfigureSceneSpawn(
                { 101, { 0.0f, 0.0f, -24.0f }, 0.0f });
            hostWitnessSceneEntered = hostEntryAccepted && clientSpawnRestored;
        }
        // Limb hits deal half damage, so allow enough authoritative attacks
        // to complete the death/respawn portion even when every strike lands
        // on an arm or leg.
        if (deathRequested && deathAttacksSent < 6 &&
            (lastDeathAttackAt == 0 || GetTickCount64() - lastDeathAttackAt >= 550)) {
            if (network.SendPlayerCommand(MakeCommand(
                    hostCommandSequence++, 2, static_cast<int16_t>(0x8000), 0,
                    NETWORK_ACTION_PRIMARY))) {
                ++deathAttacksSent;
                lastDeathAttackAt = GetTickCount64();
            }
        }
        if (projectileImpactWitnessed && !projectileImpactAcknowledged) {
            projectileImpactAcknowledged = authorizeRemoteTransition(110) &&
                                             network.SendChat("runtime-impact-complete");
        }
        if (projectileImpactAcknowledged && !hostReturnedFromWitnessScene) {
            const bool hostSpawnConfigured =
                network.ConfigureSceneSpawn({ 110, {}, 0.0f });
            Game::Client::LocalSceneEntryRequest combatEntry{};
            combatEntry.sequence = 5;
            combatEntry.sceneId = 110;
            const bool hostEntryAccepted = hostSpawnConfigured &&
                                           network.AuthorizeSceneTransition(0, 110) &&
                                           network.SendSceneEntryIntent(combatEntry);
            const bool clientSpawnRestored = network.ConfigureSceneSpawn(
                { 110, { 0.0f, 0.0f, -24.0f }, 0.0f });
            hostReturnedFromWitnessScene = hostEntryAccepted && clientSpawnRestored;
        }
        if (fishReleaseReceived && !hostCombatSceneEntered) {
            const bool hostSpawnConfigured =
                network.ConfigureSceneSpawn({ 110, {}, 0.0f });
            Game::Client::LocalSceneEntryRequest combatEntry{};
            combatEntry.sequence = 3;
            combatEntry.sceneId = 110;
            const bool hostEntryAccepted = hostSpawnConfigured &&
                                           network.AuthorizeSceneTransition(0, 110) &&
                                           network.SendSceneEntryIntent(combatEntry);
            const bool clientSpawnRestored = network.ConfigureSceneSpawn(
                { 110, { 0.0f, 0.0f, -24.0f }, 0.0f });
            hostCombatSceneEntered = hostEntryAccepted && clientSpawnRestored;
        }

        if (!responseSent && network.IsSecure() && chatReceived && privateReceived && stateReceived &&
            fishingVisualReceived && voiceReceived && fishCanonicalStateReceived) {
            const auto players = network.Players();
            int32_t clientId = -1;
            for (const auto& player : players) {
                if (player.playerId > 0) {
                    clientId = player.playerId;
                }
            }
            if (clientId > 0 && network.SendPrivateChat(clientId, "runtime-host-private")) {
                network.SendChat("runtime-host-chat");
                TestPlayerState targetState = MakeState(555);
                targetState.x = 0.0f;
                targetState.y = 0.0f;
                targetState.z = 60.0f;
                SendPresentation(network, targetState);
                network.SendVoiceFrame(MakeVoice(777));
                responseSent = true;
            }
        }
        if (clientCompleteAcknowledged && GetTickCount64() - clientCompleteAt >= 500) break;
        Sleep(5);
    }

    const NetworkTestFaultStats transportFaults = GetNetworkTestFaultStats();
    const uint64_t sessionGenerationBeforeDisconnect = network.SessionGeneration();
    network.Disconnect();
    const bool sessionGenerationAdvanced =
        network.SessionGeneration() != 0 &&
        network.SessionGeneration() != sessionGenerationBeforeDisconnect;
    const bool sessionStateReset =
        !network.IsActive() && !network.IsSecure() &&
        network.LocalPlayerId() == -1 && network.Players().empty() &&
        network.InboundBytesPerSecond() == 0 &&
        network.OutboundBytesPerSecond() == 0;
    if (gameMasterListExisted) {
        SaveGameMasterList(originalGameMasters);
    } else {
        DeleteFileA(GM_LIST_FILENAME);
    }
    Error("Runtime host summary: chat=%d private=%d state=%d pondPlayer=%d outOfInterestPlayer=%d hostAuthority=%d fishingVisual=%d lure=%d fishTelemetryRejected=%d fishRelease=%d "
           "poseEquipment=%d bowString=%d voice=%d arrowPitch=%d arrowDamage=%d clientMelee=%d hostMeleeSeen=%d fishCanonical=%d "
           "impact=%d aimedPitch=%d stuckPitch=%d stuckPersistent=%d corpse=%d response=%d complete=%d",
          chatReceived, privateReceived, stateReceived, pondPlayerSnapshotReceived,
          outOfInterestPlayerSnapshotReceived, hostAuthoritativeSnapshotReceived, fishingVisualReceived,
          lureStateReceived, fishTelemetryCouldNotHook, fishReleaseReceived, poseEquipmentReceived,
          bowStringScaleReceived, voiceReceived, arrowNativeDisplayPitchReceived, arrowDamageReceived,
           clientMeleeDamageReceived, clientSawHostMeleeDamage, fishCanonicalStateReceived,
           projectileImpactWitnessed, arrowAimedDisplayPitchReceived, arrowStuckDisplayPitchReceived,
           !projectileRetiredUnexpectedly, corpseReceived, responseSent, clientComplete);
    Error("Runtime host transport faults: considered=%llu dropped=%llu reliable=%llu reliableDropped=%llu",
          transportFaults.considered, transportFaults.dropped, transportFaults.reliableConsidered,
          transportFaults.reliableDropped);
    std::fprintf(stderr,
                 "host-final: complete=%d response=%d pondPlayer=%d outOfInterestPlayer=%d hostAuthority=%d visual=%d lure=%d telemetry=%d "
                 "release=%d canonical=%d pose=%d bow=%d impact=%d pitch=%d aimed=%d stuck=%d "
                 "persistent=%d arrowDamage=%d clientMelee=%d hostMelee=%d corpse=%d faults=%llu reliableDrop=%llu\n",
                 clientComplete, responseSent, pondPlayerSnapshotReceived,
                 outOfInterestPlayerSnapshotReceived, hostAuthoritativeSnapshotReceived,
                 fishingVisualReceived,
                 lureStateReceived, fishTelemetryCouldNotHook, fishReleaseReceived,
                 fishCanonicalStateReceived, poseEquipmentReceived, bowStringScaleReceived,
                 projectileImpactWitnessed, arrowNativeDisplayPitchReceived,
                 arrowAimedDisplayPitchReceived, arrowStuckDisplayPitchReceived,
                 !projectileRetiredUnexpectedly, arrowDamageReceived, clientMeleeDamageReceived,
                 clientSawHostMeleeDamage, corpseReceived, transportFaults.dropped,
                 transportFaults.reliableDropped);
    return clientComplete && responseSent && pondPlayerSnapshotReceived &&
                   !outOfInterestPlayerSnapshotReceived &&
                   mismatchedPredictionLifeRejected && hostBaselineCommandSent &&
                   hostAuthoritativeSnapshotReceived && hostCombatSceneEntered &&
                   hostObjectiveStateReceived && hostStrategicTopologyReceived &&
                   hostStructureStateReceived &&
                   hostWitnessSceneEntered && hostReturnedFromWitnessScene && fishingVisualReceived &&
                   lureStateReceived && fishTelemetryCouldNotHook &&
                   fishReleaseReceived &&
                   fishCanonicalStateReceived &&
                   poseEquipmentReceived && bowStringScaleReceived && projectileImpactWitnessed &&
                   arrowNativeDisplayPitchReceived && arrowAimedDisplayPitchReceived &&
                    arrowStuckDisplayPitchReceived && arrowDamageReceived &&
                    clientMeleeDamageReceived &&
                    clientSawHostMeleeDamage && corpseReceived &&
                    !projectileRetiredUnexpectedly &&
                    sessionGenerationAdvanced && sessionStateReset &&
                    transportFaultsActive && transportFaults.dropped > 0 &&
                    transportFaults.reliableDropped == 1
               ? 0
               : 11;
}

int RunClient() {
    NetworkRuntime network;
    if (!network.Connect("127.0.0.1:47778")) {
        return 20;
    }

    bool fishingEquipTransitionSent = false;
    bool initialSent = false;
    bool fishingWeaponSelected = false;
    bool bowWeaponSelected = false;
    bool meleeWeaponSelected = false;
    bool adminCommandSent = false;
    bool adminCommandAcknowledged = false;
    bool teamBaselineReceived = false;
    bool teamCommandSent = false;
    bool teamCommandAcknowledged = false;
    bool teamSnapshotReceived = false;
    bool objectiveBaselineReceived = false;
    bool strategicTopologyReceived = false;
    bool objectiveCapturedReceived = false;
    bool objectiveInterestLeaveReceived = false;
    bool objectiveInterestReentryReceived = false;
    bool structureBaselineReceived = false;
    bool structurePartialReceived = false;
    bool structureActiveReceived = false;
    bool privateSent = false;
    bool chatReceived = false;
    bool privateReceived = false;
    bool stateReceived = false;
    bool voiceReceived = false;
    bool fishHookSent = false;
    bool lureIntentSent = false;
    bool lureStateReceived = false;
    bool settledLureStateReceived = false;
    uint8_t latestLurePhase = 0xFF;
    Game::Simulation::Vec3 latestLurePosition{};
    bool staleLureControlSent = false;
    bool lurePresentationSent = false;
    bool fishHookAcknowledged = false;
    bool fishTelemetryAcknowledged = false;
    bool postHookStateSent = false;
    bool fishCanonicalAcknowledged = false;
    bool fishReleaseSent = false;
    bool bowStateSent = false;
    bool bowCommandSent = false;
    bool bowPresentationConfirmed = false;
    bool rejectedProjectileSent = false;
    bool rejectedProjectileResult = false;
    bool projectileSent = false;
    bool projectileAccepted = false;
    bool localArrowTerminalReceived = false;
    bool projectileImpactAcknowledged = false;
    bool arrowDamageAcknowledged = false;
    bool clientMeleeNearMissSent = false;
    bool clientMeleeSent = false;
    bool clientMeleeDamageAcknowledged = false;
    bool hostMeleeDamageReceived = false;
    bool hostMeleeDamageAcknowledged = false;
    bool witnessArrowReady = false;
    bool witnessBowStateSent = false;
    bool witnessBowCommandSent = false;
    bool witnessBowPresentationConfirmed = false;
    bool witnessAimConfirmed = false;
    bool witnessWeaponConfirmed = false;
    bool witnessProjectileSent = false;
    bool witnessProjectileAccepted = false;
    uint32_t rejectedProjectileSequence = 0;
    uint32_t acceptedProjectileSequence = 0;
    uint32_t witnessProjectileSequence = 0;
    int32_t acceptedProjectileId = 0;
    int32_t witnessProjectileId = 0;
    bool deathSent = false;
    bool respawnSent = false;
    bool prematureRespawnReceived = false;
    bool staleDeadSent = false;
    bool duplicateRespawnReceived = false;
    bool corpseReceived = false;
    bool corpseReceivedBeforeRespawn = false;
    bool interestLeaveSent = false;
    bool interestLeaveReceived = false;
    bool generationCheckedRemovalReceived = false;
    bool interestReentrySent = false;
    bool interestReentryReceived = false;
    bool completionAcknowledged = false;
    unsigned __int64 bowStateSentAt = 0;
    unsigned __int64 lastBowCommandAt = 0;
    unsigned __int64 lastWitnessBowCommandAt = 0;
    unsigned __int64 deathSentAt = 0;
    unsigned __int64 respawnReceivedAt = 0;
    unsigned __int64 staleDeadSentAt = 0;
    uint32_t commandSequence = 1;
    uint32_t weaponSelectionSequence = 1;
    uint32_t projectileIntentSequence = 1;
    uint32_t lureIntentSequence = 1;
    Game::Client::LocalStructureActionStream structureActionStream;
    uint32_t sceneEntrySequence = 1;
    uint32_t unauthorizedSceneRequestSequence = 0;
    uint32_t interestReturnSequence = 0;
    int32_t authorizedScene = -1;
    uint8_t authoritativeSelectedWeapon = 0xFF;
    uint32_t latestAuthoritativeServerTick = 0;
    uint32_t bowAimStartedTick = 0;
    uint32_t witnessAimStartedTick = 0;
    uint32_t witnessWeaponRequestedAfterTick = 0;
    uint32_t hostEntityIndex = 0;
    uint32_t hostEntityGeneration = 0;
    bool unauthorizedSceneRequested = false;
    bool unauthorizedSceneRejected = false;
    bool pondTransitionRequestSent = false;
    bool pondTransitionGranted = false;
    bool combatTransitionRequestSent = false;
    bool combatTransitionGranted = false;
    bool scene73Requested = false;
    bool scene110Requested = false;
    bool scene101Requested = false;
    bool returnSceneRequested = false;
    bool interestOutsideRequested = false;
    bool interestOutboundGrantRequestSent = false;
    bool interestOutboundGranted = false;
    bool interestReturnRequested = false;
    bool interestReturnGrantRequestSent = false;
    bool interestReturnGranted = false;
    int structureActionsSent = 0;
    unsigned __int64 lastStructureActionAt = 0;
    unsigned __int64 lastLureIntentAt = 0;
    unsigned __int64 lastInterestReturnAt = 0;
    bool transportFaultsActive = false;

    const unsigned __int64 timeout = GetTickCount64() + 40000;
    while (GetTickCount64() < timeout) {
        network.Update();
        if (!transportFaultsActive && network.IsSecure()) {
            ConfigureNetworkTestPacketLoss(kDropEveryNthDisposableDatagram, true);
            transportFaultsActive = true;
        }
        Game::Client::LocalSceneAuthority sceneEntry{};
        while (network.PollSceneEntryState(sceneEntry)) {
            authorizedScene = sceneEntry.sceneId;
            unauthorizedSceneRejected = unauthorizedSceneRejected ||
                                        (sceneEntry.requestSequence == unauthorizedSceneRequestSequence &&
                                         !sceneEntry.accepted && sceneEntry.sceneId == 110);
        }
        if (!unauthorizedSceneRequested && authorizedScene == 110 && network.IsSecure() &&
            network.LocalPlayerId() > 0) {
            Game::Client::LocalSceneEntryRequest intent{};
            unauthorizedSceneRequestSequence = sceneEntrySequence++;
            intent.sequence = unauthorizedSceneRequestSequence;
            intent.sceneId = 200;
            unauthorizedSceneRequested = network.SendSceneEntryIntent(intent);
        }
        if (!pondTransitionRequestSent && unauthorizedSceneRejected && authorizedScene == 110) {
            pondTransitionRequestSent =
                network.SendChat("runtime-request-pond-transition");
        }
        if (!scene73Requested && pondTransitionGranted && authorizedScene == 110) {
            Game::Client::LocalSceneEntryRequest intent{};
            intent.sequence = sceneEntrySequence++;
            intent.sceneId = 0x49;
            scene73Requested = network.SendSceneEntryIntent(intent);
        }
        if (!fishingEquipTransitionSent && authorizedScene == 0x49) {
            TestPlayerState fishingEquipTransition = MakeState(109);
            fishingEquipTransition.sceneId = 0x49;
            // Presentation packets are deliberately forged far away. They may
            // carry animation, equipment, and fishing visuals, but must never
            // move the server simulation away from its registered scene spawn.
            fishingEquipTransition.x = 900000.0f;
            fishingEquipTransition.y = 800000.0f;
            fishingEquipTransition.z = 700000.0f;
            fishingEquipTransition.selectedWeapon = 4;
            // An invalid optional fishing visual sample must be stripped
            // without dropping the movement/animation packet around it. The
            // optional fishing fields cannot suppress the core pose.
            fishingEquipTransition.fishingState = 4;
            fishingEquipTransition.fishingLineScale = 0.0f;
            // Face into the pond so the server-owned cast lands among the
            // canonical fish rather than beyond the shoreline.
            Game::Simulation::PlayerCommand fishingCommand =
                MakeCommand(commandSequence++, 4, -32768, 0,
                            NETWORK_ACTION_PRIMARY);
            if (!fishingWeaponSelected) {
                fishingWeaponSelected =
                    SelectWeapon(network, weaponSelectionSequence++, 4);
            }
            fishingEquipTransitionSent = fishingWeaponSelected &&
                                         authoritativeSelectedWeapon == 4 &&
                                         network.SendPlayerCommand(fishingCommand) &&
                                         SendPresentation(network, fishingEquipTransition);
        }
        if (fishingEquipTransitionSent && !initialSent && authorizedScene == 0x49) {
            TestPlayerState initialState = MakeState(110);
            initialState.sceneId = 0x49;
            initialState.x = 666.0f;
            initialState.y = -87.0f;
            initialState.z = 354.0f;
            initialState.selectedWeapon = 4;
            initialState.fishingLineScale = 0.0005f;
            initialState.fishingLineGravity = 2.25f;
            initialState.fishingState = 4;
            initialState.fishingRodTipOffset[0] = 300.0f;
            initialState.fishingLureDrawOffset[0] = 17.25f;
            initialState.fishingLureSpin = 0.375f;
            initialState.fishingLureZOffset = -725.0f;
            initialState.fishingSinkingLureUnderwater = 1;
            initialSent = network.SendChat("runtime-client-chat") && SendPresentation(network, initialState) &&
                          network.SendVoiceFrame(MakeVoice(444));
        }
        if (initialSent && !adminCommandSent) {
            adminCommandSent = network.SendChat("/users");
        }
        if (teamBaselineReceived && !teamCommandSent) {
            teamCommandSent = network.SendChat("/team red");
        }
        if (initialSent && !lureStateReceived &&
            (lastLureIntentAt == 0 || GetTickCount64() - lastLureIntentAt >= 20)) {
            const bool sent = network.SendLureControlIntent(MakeLureIntent(lureIntentSequence));
            lureIntentSent = lureIntentSent || sent;
            if (sent) ++lureIntentSequence;
            lastLureIntentAt = GetTickCount64();
        }
        Game::Client::RemoteLureEntity lureState{};
        while (network.PollLureState(lureState)) {
            latestLurePhase = lureState.phase;
            latestLurePosition = { lureState.x, lureState.y, lureState.z };
            lureStateReceived = lureStateReceived ||
                                (lureState.ownerPlayerId > 0 && lureState.sceneId == 0x49);
            settledLureStateReceived = settledLureStateReceived ||
                (lureState.ownerPlayerId > 0 && lureState.sceneId == 0x49 &&
                 lureState.phase == static_cast<uint8_t>(
                     Game::Simulation::FishingLurePhase::Settled));
        }
        Game::Client::LocalProjectileIntentDecision projectileResult{};
        while (network.PollProjectileIntentResult(projectileResult)) {
            rejectedProjectileResult = rejectedProjectileResult ||
                (projectileResult.sequence == rejectedProjectileSequence &&
                 projectileResult.projectileId == 0 &&
                 projectileResult.kind == Game::Client::LocalProjectileIntentKind::FireArrow &&
                 !projectileResult.accepted);
            if (projectileResult.sequence == acceptedProjectileSequence &&
                projectileResult.projectileId > 0 &&
                projectileResult.kind == Game::Client::LocalProjectileIntentKind::FireArrow &&
                projectileResult.accepted) {
                acceptedProjectileId = projectileResult.projectileId;
                projectileAccepted = true;
            }
            if (projectileResult.sequence == witnessProjectileSequence &&
                projectileResult.projectileId > 0 &&
                projectileResult.kind == Game::Client::LocalProjectileIntentKind::FireArrow &&
                projectileResult.accepted) {
                witnessProjectileId = projectileResult.projectileId;
                witnessProjectileAccepted = true;
            }
        }
        Game::Client::RemoteProjectileReplicaState ownerProjectile{};
        while (network.PollProjectileState(ownerProjectile)) {
            if (ownerProjectile.logicalId.ownerPlayerId != network.LocalPlayerId()) continue;
            localArrowTerminalReceived = localArrowTerminalReceived ||
                ((ownerProjectile.logicalId.projectileId == acceptedProjectileId ||
                  ownerProjectile.logicalId.projectileId == witnessProjectileId) &&
                 ownerProjectile.logicalId.projectileKind == NETWORK_PROJECTILE_ARROW &&
                 (!ownerProjectile.active || ownerProjectile.Terminal()));
        }
        if (lureStateReceived && !staleLureControlSent) {
            staleLureControlSent = network.SendLureControlIntent(MakeLureIntent(1, 0));
        }
        if (lureStateReceived && staleLureControlSent && !lurePresentationSent) {
            TestPlayerState lurePresentation = MakeState(111);
            lurePresentation.sceneId = 0x49;
            lurePresentation.x = 666.0f;
            lurePresentation.y = -87.0f;
            lurePresentation.z = 354.0f;
            lurePresentation.selectedWeapon = 4;
            lurePresentation.fishingState = 4;
            lurePresentation.fishingLineScale = 0.0005f;
            lurePresentation.fishingLineGravity = 2.25f;
            lurePresentation.fishingLureDrawOffset[0] = 17.25f;
            lurePresentation.fishingLureSpin = 0.375f;
            lurePresentation.fishingLureZOffset = -725.0f;
            lurePresentation.fishingRodTipOffset[0] = 300.0f;
            lurePresentation.fishingSinkingLureUnderwater = 1;
            lurePresentationSent = SendPresentation(network, lurePresentation);
        }
        if (settledLureStateReceived && lurePresentationSent &&
            fishTelemetryAcknowledged && !fishHookSent) {
            fishHookSent = network.SendFishIntent(MakeFishIntent(111, NETWORK_FISH_INTENT_HOOK));
        }
        if (initialSent && !privateSent) {
            privateSent = network.SendPrivateChat(0, "runtime-client-private");
        }

        NetworkChatLine line;
        while (network.PollChat(line)) {
            chatReceived = chatReceived || line.text.find("runtime-host-chat") != std::string::npos;
            adminCommandAcknowledged = adminCommandAcknowledged ||
                                       line.text.find("system: users online:") != std::string::npos;
            teamCommandAcknowledged = teamCommandAcknowledged ||
                                      line.text.find("system: team set to red") != std::string::npos;
            privateReceived = privateReceived || line.text.find("runtime-host-private") != std::string::npos;
            projectileImpactAcknowledged = projectileImpactAcknowledged ||
                                           line.text.find("runtime-impact-complete") != std::string::npos;
            arrowDamageAcknowledged = arrowDamageAcknowledged ||
                                      line.text.find("runtime-arrow-damage") != std::string::npos;
            clientMeleeDamageAcknowledged = clientMeleeDamageAcknowledged ||
                                            line.text.find("runtime-client-melee-damage") != std::string::npos;
            witnessArrowReady = witnessArrowReady || line.text.find("runtime-witness-ready") != std::string::npos;
            fishHookAcknowledged = fishHookAcknowledged ||
                                   line.text.find("runtime-fish-hook") != std::string::npos;
            fishTelemetryAcknowledged = fishTelemetryAcknowledged ||
                                         line.text.find("runtime-fish-telemetry-rejected") != std::string::npos;
            fishCanonicalAcknowledged = fishCanonicalAcknowledged ||
                                        line.text.find("runtime-fish-canonical") != std::string::npos;
            pondTransitionGranted = pondTransitionGranted ||
                line.text.find("runtime-pond-transition-granted") != std::string::npos;
            combatTransitionGranted = combatTransitionGranted ||
                line.text.find("runtime-combat-transition-granted") != std::string::npos;
            interestOutboundGranted = interestOutboundGranted ||
                line.text.find("runtime-interest-outbound-granted") != std::string::npos;
            interestReturnGranted = interestReturnGranted ||
                line.text.find("runtime-interest-return-granted") != std::string::npos;
            completionAcknowledged = completionAcknowledged ||
                                     line.text.find("runtime-complete-ack") != std::string::npos;
        }
        Game::Simulation::PlayerSnapshot snapshot{};
        while (network.PollPlayerSnapshot(snapshot)) {
            if (snapshot.ownerPlayerId == network.LocalPlayerId()) {
                teamBaselineReceived = true;
                teamSnapshotReceived = teamSnapshotReceived ||
                    snapshot.team == Game::Simulation::TeamId::Red;
                latestAuthoritativeServerTick =
                    std::max(latestAuthoritativeServerTick, snapshot.serverTick);
                authoritativeSelectedWeapon = snapshot.selectedWeapon;
                if (snapshot.selectedWeapon == 3 &&
                    snapshot.actionState ==
                        Game::Simulation::PlayerActionState::Aiming &&
                    (snapshot.heldActions & NETWORK_ACTION_AIM) != 0) {
                    if (snapshot.sceneId == 110 && bowAimStartedTick == 0) {
                        bowAimStartedTick = snapshot.actionStartTick;
                    } else if (snapshot.sceneId == 0x65 &&
                               witnessAimStartedTick == 0) {
                        witnessAimStartedTick = snapshot.actionStartTick;
                    }
                }
                witnessWeaponConfirmed = witnessWeaponConfirmed ||
                    (witnessWeaponRequestedAfterTick != 0 && snapshot.sceneId == 0x65 &&
                     snapshot.selectedWeapon == 3 &&
                     static_cast<int32_t>(snapshot.serverTick -
                                          witnessWeaponRequestedAfterTick) > 0);
                witnessAimConfirmed = witnessAimConfirmed ||
                    (snapshot.sceneId == 0x65 && snapshot.selectedWeapon == 3 &&
                     (snapshot.heldActions & NETWORK_ACTION_AIM) != 0 &&
                     std::abs(snapshot.aimPitchRadians - 0.3926990817f) < 0.0001f);
            } else if (snapshot.ownerPlayerId == 0 && snapshot.entity.Valid()) {
                hostEntityIndex = snapshot.entity.index;
                hostEntityGeneration = snapshot.entity.generation;
                stateReceived = true;
                interestReentryReceived = interestReentryReceived ||
                                          (interestLeaveReceived && interestReturnRequested &&
                                           snapshot.sceneId == 110);
            }
        }
        Game::Client::ReplicatedObjectiveState objective{};
        while (network.PollObjectiveState(objective)) {
            if (objective.snapshot.objectiveKey == 7001 && !objective.active &&
                authorizedScene == 0x49) {
                objectiveInterestLeaveReceived = true;
            }
            if (objective.snapshot.objectiveKey == 7001 && objective.active) {
                objectiveBaselineReceived = true;
                objectiveInterestReentryReceived = objectiveInterestReentryReceived ||
                    (objectiveInterestLeaveReceived && authorizedScene == 110);
                objectiveCapturedReceived = objectiveCapturedReceived ||
                    objective.snapshot.owner == Game::Simulation::TeamId::Red;
            }
        }
        Game::Client::ReplicatedStrategicTopologyState topology{};
        while (network.PollStrategicTopology(topology)) {
            strategicTopologyReceived = strategicTopologyReceived ||
                (topology.sites.size() == 2 && topology.supplyRoutes.size() == 1 &&
                 topology.influenceAdjacencies.size() == 1 &&
                 topology.influenceAdjacencies.front().lowerRegionKey == 1 &&
                 topology.influenceAdjacencies.front().upperRegionKey == 2 &&
                 topology.supplyRoutes.front().sourceObjectiveKey == 7000 &&
                 topology.supplyRoutes.front().destinationObjectiveKey == 7001);
        }
        Game::Client::ReplicatedStructureState structureState{};
        while (network.PollStructureState(structureState)) {
            const auto& structureSnapshot = structureState.snapshot;
            if (structureSnapshot.structureKey == 8001 && structureState.active) {
                structureBaselineReceived = true;
                structurePartialReceived = structurePartialReceived ||
                                           (structureSnapshot.phase ==
                                                Game::Simulation::StructurePhase::Building &&
                                            structureSnapshot.buildProgress == 25);
                structureActiveReceived = structureActiveReceived ||
                                          (structureSnapshot.phase ==
                                               Game::Simulation::StructurePhase::Active &&
                                           structureSnapshot.team == Game::Simulation::TeamId::Red &&
                                           structureSnapshot.health == 500 &&
                                           structureSnapshot.buildProgress == 50);
            }
        }
        if (objectiveCapturedReceived && structureBaselineReceived && !structureActiveReceived &&
            structureActionsSent < 8 &&
            (lastStructureActionAt == 0 || GetTickCount64() - lastStructureActionAt >= 300)) {
            const auto action = structureActionStream.Issue(
                { 8001, Game::Client::LocalStructureActionKind::Build });
            if (action && network.SendStructureAction(*action)) {
                ++structureActionsSent;
                lastStructureActionAt = GetTickCount64();
            }
        }
        Game::Client::CorpsePresentationState corpse{};
        while (network.PollCorpseState(corpse)) {
            const bool admitted = corpse.active && corpse.entity.Valid() &&
                                  corpse.sourcePlayerEntity.Valid() &&
                                  corpse.sourceLifeEpoch != 0;
            corpseReceived = corpseReceived || admitted;
            corpseReceivedBeforeRespawn =
                corpseReceivedBeforeRespawn || (admitted && !respawnSent);
        }
        Game::Client::RemotePlayerPresentationState lifecycle{};
        while (network.PollPlayerLifecycle(lifecycle)) {
            generationCheckedRemovalReceived = generationCheckedRemovalReceived ||
                (!lifecycle.active && lifecycle.playerId == 0 && hostEntityGeneration != 0 &&
                 lifecycle.entity.index == hostEntityIndex &&
                 lifecycle.entity.generation == hostEntityGeneration);
            interestLeaveReceived = interestLeaveReceived ||
                                    (interestOutsideRequested && generationCheckedRemovalReceived);
            interestReentryReceived = interestReentryReceived ||
                                      (interestLeaveReceived && interestReturnRequested && lifecycle.active &&
                                       lifecycle.playerId == 0 && lifecycle.sceneId == 110);
        }
        NetworkVoicePacket voice;
        while (network.PollVoice(voice)) {
            voiceReceived = voice.playerId == 0 && voice.sequence == 1 && !voice.data.empty();
        }
        if (fishHookAcknowledged && !postHookStateSent) {
            TestPlayerState hookedState = MakeState(112);
            hookedState.sceneId = 0x49;
            hookedState.x = 666.0f;
            hookedState.y = -87.0f;
            hookedState.z = 354.0f;
            hookedState.selectedWeapon = 4;
            hookedState.fishingLineScale = 0.0005f;
            hookedState.fishingLineGravity = 2.25f;
            hookedState.fishingState = 4;
            // Cosmetic telemetry has no fields capable of forging fish
            // ownership, identity, position, species, or weight.
            postHookStateSent = SendPresentation(network, hookedState);
        }
        if (postHookStateSent && fishCanonicalAcknowledged && !fishReleaseSent) {
            fishReleaseSent = network.SendFishIntent(MakeFishIntent(111, NETWORK_FISH_INTENT_RELEASE));
        }
        if (fishReleaseSent && !combatTransitionRequestSent) {
            combatTransitionRequestSent =
                network.SendChat("runtime-request-combat-transition");
        }
        if (fishReleaseSent && combatTransitionGranted && !scene110Requested) {
            Game::Client::LocalSceneEntryRequest intent{};
            intent.sequence = sceneEntrySequence++;
            intent.sceneId = 110;
            scene110Requested = network.SendSceneEntryIntent(intent);
        }
        if (fishReleaseSent && authorizedScene == 110 && !bowStateSent) {
            TestPlayerState bowState = MakeState(113);
            bowState.x = 0.0f;
            bowState.y = 0.0f;
            bowState.z = 0.0f;
            bowState.selectedWeapon = 3;
            bowState.aimPitch = 0;
            bowState.aimYaw = 0;
            bowStateSent = SendPresentation(network, bowState);
            bowStateSentAt = GetTickCount64();
        }
        if (bowStateSent) {
            if (!rejectedProjectileSent) {
                Game::Client::LocalProjectileIntent rejectedArrow{};
                rejectedProjectileSequence = projectileIntentSequence++;
                rejectedArrow.sequence = rejectedProjectileSequence;
                rejectedProjectileSent = network.SendArrowFireIntent(rejectedArrow);
            }
            if (rejectedProjectileResult && !bowWeaponSelected) {
                bowWeaponSelected = SelectWeapon(network, weaponSelectionSequence++, 3);
            }
            if (!projectileSent && bowWeaponSelected &&
                authoritativeSelectedWeapon == 3 &&
                (lastBowCommandAt == 0 ||
                 GetTickCount64() - lastBowCommandAt >= 20)) {
                const bool sent = network.SendPlayerCommand(
                    MakeCommand(commandSequence++, 3, 0,
                                NETWORK_ACTION_AIM));
                bowCommandSent = bowCommandSent || sent;
                if (sent) lastBowCommandAt = GetTickCount64();
            }
        }
        if (bowCommandSent && !bowPresentationConfirmed && GetTickCount64() - bowStateSentAt >= 100) {
            TestPlayerState aimedState = MakeState(114);
            aimedState.x = 0.0f;
            aimedState.y = 0.0f;
            aimedState.z = 0.0f;
            aimedState.selectedWeapon = 3;
            bowPresentationConfirmed = SendPresentation(network, aimedState);
        }
        if (bowPresentationConfirmed && !projectileSent &&
            bowAimStartedTick != 0 &&
            latestAuthoritativeServerTick - bowAimStartedTick >=
                Game::Simulation::kBowMinimumDrawDurationTicks) {
            Game::Client::LocalProjectileIntent arrow{};
            acceptedProjectileSequence = projectileIntentSequence++;
            arrow.sequence = acceptedProjectileSequence;
            projectileSent = network.SendArrowFireIntent(arrow);
        }
        if (arrowDamageAcknowledged && !clientMeleeSent) {
            clientMeleeNearMissSent = true;
            if (!meleeWeaponSelected) {
                meleeWeaponSelected = SelectWeapon(network, weaponSelectionSequence++, 2);
            }
            clientMeleeSent = meleeWeaponSelected && authoritativeSelectedWeapon == 2 &&
                              network.SendPlayerCommand(
                                  MakeCommand(commandSequence++, 2, 0, 0,
                                              NETWORK_ACTION_PRIMARY));
        }
        Game::Simulation::CombatResultEvent damage{};
        while (network.PollCombatResult(damage)) {
            hostMeleeDamageReceived = hostMeleeDamageReceived ||
                                      (damage.sourcePlayerId == 0 && damage.targetPlayerId == network.LocalPlayerId() &&
                                       damage.sourceEntity.Valid() && damage.targetEntity.Valid() &&
                                       damage.sceneId == 110 &&
                                       damage.attackKind == Game::Simulation::CombatAttackKind::Melee &&
                                       damage.result == Game::Simulation::CombatResultKind::Damaged &&
                                       damage.hitRegion != Game::Simulation::PlayerHitRegion::None &&
                                       damage.damage == Game::Simulation::DamageForPlayerHitRegion(
                                                            16, damage.hitRegion));
        }
        Game::Simulation::PlayerRespawnEvent respawn{};
        while (network.PollPlayerRespawn(respawn)) {
            if (GetTickCount64() - deathSentAt < kExpectedRespawnMilliseconds) {
                prematureRespawnReceived = true;
            }
            if (respawn.playerId == network.LocalPlayerId()) {
                if (respawnSent) {
                    duplicateRespawnReceived = true;
                } else {
                    respawnSent = true;
                    respawnReceivedAt = GetTickCount64();
                    TestPlayerState respawnState = MakeState(119);
                    SendPresentation(network, respawnState);
                }
            }
        }
        if (respawnSent && !staleDeadSent && GetTickCount64() - respawnReceivedAt >= 150) {
            // Simulate a delayed unreliable snapshot arriving after the reliable
            // respawn command and the living state that acknowledges it.
            TestPlayerState staleDeadState = MakeState(120);
            staleDeadSent = SendPresentation(network, staleDeadState);
            if (staleDeadSent) {
                staleDeadSentAt = GetTickCount64();
            }
        }
        if (duplicateRespawnReceived) {
            Error("Runtime respawn test failed: stale death state caused a duplicate respawn");
            network.Disconnect();
            return 24;
        }
        if (hostMeleeDamageReceived && !hostMeleeDamageAcknowledged) {
            hostMeleeDamageAcknowledged = network.SendChat("runtime-host-melee-damage");
        }
        if (witnessArrowReady && !scene101Requested) {
            Game::Client::LocalSceneEntryRequest intent{};
            intent.sequence = sceneEntrySequence++;
            intent.sceneId = 0x65;
            scene101Requested = network.SendSceneEntryIntent(intent);
        }
        if (witnessArrowReady && authorizedScene == 0x65 && !witnessBowStateSent) {
            TestPlayerState witnessBow = MakeState(115);
            witnessBow.sceneId = 0x65;
            witnessBow.x = 0.0f;
            witnessBow.y = 0.0f;
            witnessBow.z = 0.0f;
            witnessBow.selectedWeapon = 3;
            witnessBow.aimPitch = 0x1000;
            witnessBow.aimYaw = 0x4000;
            witnessBowStateSent = SendPresentation(network, witnessBow);
            bowStateSentAt = GetTickCount64();
        }
        if (witnessBowStateSent && !witnessProjectileSent) {
            if (meleeWeaponSelected) {
                witnessWeaponRequestedAfterTick = latestAuthoritativeServerTick;
                const bool selectedBow =
                    SelectWeapon(network, weaponSelectionSequence++, 3);
                bowWeaponSelected = selectedBow;
                meleeWeaponSelected = !selectedBow;
            }
            if (bowWeaponSelected && witnessWeaponConfirmed &&
                (lastWitnessBowCommandAt == 0 ||
                 GetTickCount64() - lastWitnessBowCommandAt >= 20)) {
                Game::Simulation::PlayerCommand witnessCommand =
                    MakeCommand(commandSequence++, 3, 0x4000,
                                NETWORK_ACTION_AIM);
                witnessCommand.aimPitchRadians =
                    0x1000 * (3.14159265358979323846f / 32768.0f);
                const bool sent = network.SendPlayerCommand(witnessCommand);
                witnessBowCommandSent = witnessBowCommandSent || sent;
                if (sent) lastWitnessBowCommandAt = GetTickCount64();
            }
        }
        if (witnessBowCommandSent && !witnessBowPresentationConfirmed &&
            GetTickCount64() - bowStateSentAt >= 100) {
            TestPlayerState aimedWitness = MakeState(116);
            aimedWitness.sceneId = 0x65;
            aimedWitness.x = 0.0f;
            aimedWitness.y = 0.0f;
            aimedWitness.z = 0.0f;
            aimedWitness.selectedWeapon = 3;
            aimedWitness.aimPitch = 0x1000;
            aimedWitness.aimYaw = 0x4000;
            witnessBowPresentationConfirmed = SendPresentation(network, aimedWitness);
        }
        if (witnessBowPresentationConfirmed && witnessAimConfirmed &&
            !witnessProjectileSent && witnessAimStartedTick != 0 &&
            latestAuthoritativeServerTick - witnessAimStartedTick >=
                Game::Simulation::kBowMinimumDrawDurationTicks) {
            Game::Client::LocalProjectileIntent witnessArrow{};
            witnessProjectileSequence = projectileIntentSequence++;
            witnessArrow.sequence = witnessProjectileSequence;
            witnessProjectileSent = network.SendArrowFireIntent(witnessArrow);
        }
        if (projectileImpactAcknowledged && !returnSceneRequested) {
            Game::Client::LocalSceneEntryRequest intent{};
            intent.sequence = sceneEntrySequence++;
            intent.sceneId = 110;
            returnSceneRequested = network.SendSceneEntryIntent(intent);
        }

        const bool gameplayChecksComplete =
            unauthorizedSceneRequested && unauthorizedSceneRejected && fishingEquipTransitionSent &&
            initialSent && lureIntentSent && lureStateReceived && staleLureControlSent && lurePresentationSent &&
            fishTelemetryAcknowledged &&
            adminCommandSent && adminCommandAcknowledged &&
            teamCommandSent && teamCommandAcknowledged && teamSnapshotReceived &&
            objectiveBaselineReceived && objectiveCapturedReceived &&
            strategicTopologyReceived &&
            objectiveInterestLeaveReceived && objectiveInterestReentryReceived &&
            structureBaselineReceived &&
            structurePartialReceived && structureActiveReceived && privateSent &&
            chatReceived &&
            privateReceived && stateReceived && voiceReceived &&
            fishHookSent && fishReleaseSent && bowStateSent &&
            rejectedProjectileSent && rejectedProjectileResult &&
            projectileSent && projectileAccepted && localArrowTerminalReceived &&
            postHookStateSent && fishCanonicalAcknowledged &&
            arrowDamageAcknowledged && clientMeleeNearMissSent && clientMeleeSent && clientMeleeDamageAcknowledged &&
            hostMeleeDamageReceived && hostMeleeDamageAcknowledged &&
            witnessProjectileSent && witnessProjectileAccepted &&
            projectileImpactAcknowledged && returnSceneRequested && authorizedScene == 110;
        if (gameplayChecksComplete && !deathSent) {
            TestPlayerState finalPose = MakeState(118);
            deathSent = SendPresentation(network, finalPose) && network.SendChat("runtime-request-death");
            deathSentAt = GetTickCount64();
        }
        const bool readyForInterestTest = gameplayChecksComplete && respawnSent && staleDeadSent &&
                                          corpseReceivedBeforeRespawn &&
                                          !prematureRespawnReceived &&
                                          GetTickCount64() - staleDeadSentAt >=
                                              kExpectedRespawnMilliseconds + 500;
        if (readyForInterestTest && !interestOutboundGrantRequestSent) {
            interestOutboundGrantRequestSent =
                network.SendChat("runtime-request-interest-outbound");
        }
        if (readyForInterestTest && interestOutboundGranted && !interestOutsideRequested) {
            Game::Client::LocalSceneEntryRequest intent{};
            intent.sequence = sceneEntrySequence++;
            intent.sceneId = 101;
            interestOutsideRequested = network.SendSceneEntryIntent(intent);
        }
        if (readyForInterestTest && authorizedScene == 101 && !interestLeaveSent) {
            TestPlayerState outsideScene = MakeState(121);
            outsideScene.sceneId = 101;
            Game::Simulation::PlayerCommand outsideCommand = MakeCommand(commandSequence++, 1, 0);
            interestLeaveSent = SendPresentation(network, outsideScene) &&
                                network.SendPlayerCommand(outsideCommand);
        }
        if (interestLeaveReceived && interestReturnSequence == 0) {
            interestReturnSequence = sceneEntrySequence++;
        }
        if (interestReturnSequence != 0 && !interestReturnGrantRequestSent) {
            interestReturnGrantRequestSent =
                network.SendChat("runtime-request-interest-return");
        }
        if (interestReturnSequence != 0 && interestReturnGranted && authorizedScene != 110 &&
            (lastInterestReturnAt == 0 || GetTickCount64() - lastInterestReturnAt >= 250)) {
            Game::Client::LocalSceneEntryRequest intent{};
            intent.sequence = interestReturnSequence;
            intent.sceneId = 110;
            interestReturnRequested = network.SendSceneEntryIntent(intent) || interestReturnRequested;
            lastInterestReturnAt = GetTickCount64();
        }
        if (interestLeaveReceived && authorizedScene == 110 && !interestReentrySent) {
            TestPlayerState returnState = MakeState(122);
            Game::Simulation::PlayerCommand returnCommand = MakeCommand(commandSequence++, 1, 0);
            interestReentrySent = SendPresentation(network, returnState) &&
                                  network.SendPlayerCommand(returnCommand);
        }
        if (readyForInterestTest && interestReentryReceived && generationCheckedRemovalReceived) {
            // Keep pumping past one complete telemetry interval so the byte
            // counters are converted into the rates shown in the title bar.
            int32_t observedInboundRate = 0;
            int32_t observedOutboundRate = 0;
            for (int i = 0; i < 440; ++i) {
                if ((i % 10) == 0) {
                    network.SendChat("runtime-telemetry");
                }
                network.Update();
                observedInboundRate = std::max(observedInboundRate, network.InboundBytesPerSecond());
                observedOutboundRate = std::max(observedOutboundRate, network.OutboundBytesPerSecond());
                Sleep(5);
            }
            if (observedInboundRate <= 0 || observedOutboundRate <= 0) {
                Error("Runtime telemetry failed: in=%d B/s out=%d B/s", observedInboundRate,
                      observedOutboundRate);
                return 22;
            }
            for (int i = 0; i < 300 && !completionAcknowledged; ++i) {
                if ((i % 20) == 0) {
                    network.SendChat("runtime-client-complete");
                }
                network.Update();
                NetworkChatLine completionLine;
                while (network.PollChat(completionLine)) {
                    completionAcknowledged = completionAcknowledged ||
                        completionLine.text.find("runtime-complete-ack") != std::string::npos;
                }
                Sleep(5);
            }
            const NetworkTestFaultStats transportFaults = GetNetworkTestFaultStats();
            const uint64_t sessionGenerationBeforeDisconnect = network.SessionGeneration();
            network.Disconnect();
            const bool sessionGenerationAdvanced =
                network.SessionGeneration() != 0 &&
                network.SessionGeneration() != sessionGenerationBeforeDisconnect;
            const bool sessionStateReset =
                !network.IsActive() && !network.IsSecure() &&
                network.LocalPlayerId() == -1 && network.Players().empty() &&
                network.InboundBytesPerSecond() == 0 &&
                network.OutboundBytesPerSecond() == 0;
            Error("Runtime client transport faults: considered=%llu dropped=%llu reliable=%llu reliableDropped=%llu",
                  transportFaults.considered, transportFaults.dropped, transportFaults.reliableConsidered,
                  transportFaults.reliableDropped);
            return completionAcknowledged && sessionGenerationAdvanced &&
                           sessionStateReset &&
                           transportFaultsActive && transportFaults.dropped > 0 &&
                           transportFaults.reliableDropped == 1
                       ? 0
                       : 23;
        }
        Sleep(5);
    }

    const NetworkTestFaultStats timedOutTransportFaults = GetNetworkTestFaultStats();
    network.Disconnect();
    Error("Runtime client timeout transport faults: considered=%llu dropped=%llu reliable=%llu reliableDropped=%llu",
          timedOutTransportFaults.considered, timedOutTransportFaults.dropped,
          timedOutTransportFaults.reliableConsidered, timedOutTransportFaults.reliableDropped);
    std::fprintf(stderr,
                 "client-stage: scene=%d equip=%d initial=%d lure=%d lurePhase=%u lurePos=(%.1f,%.1f,%.1f) lurePose=%d hookSent=%d "
                 "hookAck=%d canonical=%d release=%d scene110=%d bow=%d arrow=%d returnScene=%d witnessSelect=%d "
                 "witnessCommand=%d witnessAim=%d witnessProjectile=%d authWeapon=%u authTick=%u requestTick=%u\n",
                 authorizedScene, fishingEquipTransitionSent, initialSent, lureStateReceived,
                 latestLurePhase, latestLurePosition.x, latestLurePosition.y,
                 latestLurePosition.z,
                 lurePresentationSent, fishHookSent, fishHookAcknowledged,
                 fishCanonicalAcknowledged, fishReleaseSent, scene110Requested, bowStateSent,
                 projectileSent, returnSceneRequested, witnessWeaponConfirmed,
                 witnessBowCommandSent, witnessAimConfirmed, witnessProjectileSent,
                 authoritativeSelectedWeapon, latestAuthoritativeServerTick,
                 witnessWeaponRequestedAfterTick);
    Error("Runtime client timeout: sceneRejected=%d fishingEquip=%d initial=%d adminSent=%d adminAck=%d teamBase=%d teamSent=%d teamAck=%d teamSnap=%d objectiveBase=%d objectiveCaptured=%d objectiveLeave=%d objectiveReentry=%d structureBase=%d structurePartial=%d structureActive=%d structureActions=%d privateSent=%d chat=%d private=%d state=%d voice=%d "
           "arrowDamage=%d clientMeleeNearMiss=%d clientMelee=%d clientMeleeAck=%d hostMeleeDamage=%d witnessArrow=%d impact=%d "
           "returnScene=%d death=%d respawn=%d corpse=%d stale=%d interestLeave=%d interestReentry=%d",
          unauthorizedSceneRejected, fishingEquipTransitionSent, initialSent, adminCommandSent,
          adminCommandAcknowledged,
          teamBaselineReceived, teamCommandSent, teamCommandAcknowledged, teamSnapshotReceived,
          objectiveBaselineReceived, objectiveCapturedReceived, objectiveInterestLeaveReceived,
          objectiveInterestReentryReceived, structureBaselineReceived,
          structurePartialReceived, structureActiveReceived, structureActionsSent, privateSent,
          chatReceived, privateReceived,
          stateReceived, voiceReceived,
          arrowDamageAcknowledged, clientMeleeNearMissSent, clientMeleeSent,
           clientMeleeDamageAcknowledged,
           hostMeleeDamageReceived, witnessProjectileSent, projectileImpactAcknowledged,
           returnSceneRequested, deathSent, respawnSent, corpseReceived, staleDeadSent,
          interestLeaveReceived, interestReentryReceived);
    return 21;
}

bool StartChild(const std::string& executable, const char* argument, PROCESS_INFORMATION& process) {
    std::string command = '"' + executable + "\" " + argument;
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    std::memset(&process, 0, sizeof(process));
    return CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                           &process) != FALSE;
}

int RunParent() {
    char executable[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, executable, sizeof(executable))) {
        return 30;
    }

    PROCESS_INFORMATION host{};
    PROCESS_INFORMATION client{};
    if (!StartChild(executable, "--host", host)) {
        return 31;
    }
    Sleep(200);
    if (!StartChild(executable, "--client", client)) {
        TerminateProcess(host.hProcess, 32);
        CloseHandle(host.hThread);
        CloseHandle(host.hProcess);
        return 32;
    }

    HANDLE processes[] = { host.hProcess, client.hProcess };
    const DWORD wait = WaitForMultipleObjects(2, processes, TRUE, 55000);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(host.hProcess, 33);
        TerminateProcess(client.hProcess, 33);
    }

    DWORD hostExit = 33;
    DWORD clientExit = 33;
    GetExitCodeProcess(host.hProcess, &hostExit);
    GetExitCodeProcess(client.hProcess, &clientExit);
    CloseHandle(host.hThread);
    CloseHandle(host.hProcess);
    CloseHandle(client.hThread);
    CloseHandle(client.hProcess);

    if (wait == WAIT_TIMEOUT || hostExit != 0 || clientExit != 0) {
        Error("Secure runtime self-test failed: wait=%lu host=%lu client=%lu", wait, hostExit, clientExit);
        std::fprintf(stderr, "runtime-parent: wait=%lu host=%lu client=%lu\n",
                     wait, hostExit, clientExit);
        return 34;
    }
    Error("Secure runtime self-test passed: encrypted state, chat, private E2E text, and voice");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--host") == 0) {
        return RunHost();
    }
    if (argc == 2 && std::strcmp(argv[1], "--client") == 0) {
        return RunClient();
    }
    ClearLog();
    return RunParent();
}
