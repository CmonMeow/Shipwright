#include "multiplayer/Win32NetworkPlatform.h"

#include "multiplayer/NetworkProtocol.h"
#include "multiplayer/NetworkProtocolIngress.h"
#include "multiplayer/CombatNetworkAdapter.h"
#include "multiplayer/ClientCombatPresentationPolicy.h"
#include "multiplayer/ClientPlayerActionPresentationPolicy.h"
#include "multiplayer/ClientProjectilePresentationPolicy.h"
#include "multiplayer/ClientReplicationInbox.h"
#include "multiplayer/ClientSessionIngress.h"
#include "multiplayer/ClientProtocolEndpoint.h"
#include "multiplayer/CommunicationInbox.h"
#include "multiplayer/CorpseNetworkAdapter.h"
#include "multiplayer/FishingNetworkAdapter.h"
#include "multiplayer/LocalNetworkIdentity.h"
#include "multiplayer/LocalClientAdmissionService.h"
#include "multiplayer/LocalTextCommunicationService.h"
#include "multiplayer/LocalVoiceSubmissionService.h"
#include "multiplayer/PlayerSimulationNetworkAdapter.h"
#include "multiplayer/ProjectileNetworkAdapter.h"
#include "multiplayer/ProtocolDispatcher.h"
#include "multiplayer/PrivateChatService.h"
#include "multiplayer/ModerationRegistry.h"
#include "multiplayer/SceneNetworkAdapter.h"
#include "multiplayer/ServerSessionManager.h"
#include "multiplayer/ServerAdministrationService.h"
#include "multiplayer/ServerCommunicationService.h"
#include "multiplayer/ServerCommandParser.h"
#include "multiplayer/ServerReplicationInterestPublisher.h"
#include "multiplayer/ServerReplicationEventPublisher.h"
#include "multiplayer/ServerGameplayCommandService.h"
#include "multiplayer/ServerGameplayPacketIngress.h"
#include "multiplayer/ServerPlayerSessionService.h"
#include "multiplayer/SecureTransportChannel.h"
#include "multiplayer/PlayerLifecycleNetworkAdapter.h"
#include "multiplayer/WorldPvpNetworkAdapter.h"
#include "../platform/client/LocalPlayerCommandStream.h"
#include "../platform/client/LocalStructureActionStream.h"
#include "../platform/client/LocalVoiceFrameStream.h"
#include "../platform/server/ServerAuthorityScheduler.h"

#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <set>
#include <string>

namespace {

NetworkIdentity AuthenticatedTestIdentity(const std::string& identity,
                                          const std::string& name,
                                          bool voiceClient = false) {
    NetworkIdentity result{};
    result.protocolVersion = APP_PROTOCOL_VERSION;
    result.voiceClient = voiceClient;
    result.authenticated = true;
    result.id = identity;
    result.name = name;
    result.publicKey.assign(crypto_sign_PUBLICKEYBYTES,
                            static_cast<char>(identity.size()));
    return result;
}

bool TestSessionEncryption() {
    cCryptoSession client;
    cCryptoSession server;
    std::string clientKey;
    std::string serverKey;

    if (!client.buildClientHello(clientKey) || !server.acceptClientHello(clientKey, serverKey) ||
        !client.acceptServerKey(serverKey) || !client.ready() || !server.ready()) {
        return false;
    }

    const std::string clientBinding = client.identityBinding();
    const std::string serverBinding = server.identityBinding();
    unsigned char signingPublic[crypto_sign_PUBLICKEYBYTES]{};
    unsigned char signingSecret[crypto_sign_SECRETKEYBYTES]{};
    std::string signature;
    if (clientBinding.empty() || clientBinding != serverBinding ||
        crypto_sign_keypair(signingPublic, signingSecret) != 0 ||
        !Game::Multiplayer::SignIdentityBinding(
            std::string(reinterpret_cast<char*>(signingSecret),
                        sizeof(signingSecret)),
            clientBinding, signature) ||
        !Game::Multiplayer::VerifyIdentityBinding(
            std::string(reinterpret_cast<char*>(signingPublic),
                        sizeof(signingPublic)),
            serverBinding, signature) ||
        Game::Multiplayer::VerifyIdentityBinding(
            std::string(reinterpret_cast<char*>(signingPublic),
                        sizeof(signingPublic)),
            serverBinding + "tampered", signature)) {
        sodium_memzero(signingSecret, sizeof(signingSecret));
        return false;
    }
    sodium_memzero(signingSecret, sizeof(signingSecret));

    const std::string clientPlain = "client encrypted payload";
    std::string clientCipher;
    std::string serverPlain;
    if (!client.encrypt(clientPlain, clientCipher) || !server.decrypt(clientCipher.data(),
                                                                      static_cast<__int32>(clientCipher.size()),
                                                                      serverPlain) ||
        serverPlain != clientPlain) {
        return false;
    }

    const std::string serverPlainSource = "server encrypted payload";
    std::string serverCipher;
    std::string clientPlainResult;
    if (!server.encrypt(serverPlainSource, serverCipher) ||
        !client.decrypt(serverCipher.data(), static_cast<__int32>(serverCipher.size()), clientPlainResult) ||
        clientPlainResult != serverPlainSource) {
        return false;
    }

    serverCipher.back() ^= 1;
    std::string tamperedPlain;
    return !client.decrypt(serverCipher.data(), static_cast<__int32>(serverCipher.size()), tamperedPlain);
}

bool TestServerSessionManager() {
    Game::Multiplayer::ServerSessionManager sessions;
    if (sessions.ConnectPeer(0) || !sessions.ConnectPeer(7) || sessions.ConnectPeer(7) ||
        !sessions.ConnectPeer(9) || sessions.PeerCount() != 2 ||
        sessions.Peers() != std::vector<int32_t>({ 7, 9 }) || sessions.AllPeersSecure()) {
        return false;
    }

    NetworkIdentity first = AuthenticatedTestIdentity("identity-7", "Seven");
    NetworkIdentity second = AuthenticatedTestIdentity("identity-9", "Nine", true);
    if (sessions.AdmitIdentity(8, first) || !sessions.AdmitIdentity(7, first) ||
        sessions.AdmitIdentity(7, first) || !sessions.AdmitIdentity(9, second) ||
        sessions.AdmittedPeers() != std::vector<int32_t>({ 7, 9 })) {
        return false;
    }

    cCryptoSession clients[2];
    const int32_t peers[2] = { 7, 9 };
    for (size_t i = 0; i < 2; ++i) {
        std::string clientKey;
        std::string serverKey;
        cCryptoSession* serverCrypto = sessions.CryptoFor(peers[i]);
        if (!serverCrypto || !clients[i].buildClientHello(clientKey) ||
            !serverCrypto->acceptClientHello(clientKey, serverKey) ||
            !clients[i].acceptServerKey(serverKey)) {
            return false;
        }
    }
    if (!sessions.AllPeersSecure() || !sessions.SetModerationReason(7, "Seven was kicked")) return false;

    const auto departure = sessions.DisconnectPeer(7);
    if (!departure || !departure->identity || departure->identity->id != "identity-7" ||
        departure->moderationReason != "Seven was kicked" || sessions.HasPeer(7) ||
        sessions.DisconnectPeer(7) || !sessions.AllPeersSecure()) {
        return false;
    }
    sessions.Reset();
    return sessions.PeerCount() == 0 && sessions.Peers().empty() && sessions.AdmittedPeers().empty() &&
           sessions.AllPeersSecure();
}

bool TestPrivateChatEncryption() {
    unsigned char recipientPublic[crypto_box_PUBLICKEYBYTES];
    unsigned char recipientSecret[crypto_box_SECRETKEYBYTES];
    if (crypto_box_keypair(recipientPublic, recipientSecret) != 0) {
        return false;
    }

    const std::string message = "end-to-end private text";
    std::string cipher(message.size() + crypto_box_SEALBYTES, '\0');
    if (crypto_box_seal(reinterpret_cast<unsigned char*>(cipher.data()),
                        reinterpret_cast<const unsigned char*>(message.data()), message.size(), recipientPublic) != 0) {
        return false;
    }

    std::string plain(message.size(), '\0');
    const bool opened = crypto_box_seal_open(reinterpret_cast<unsigned char*>(plain.data()),
                                              reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size(),
                                              recipientPublic, recipientSecret) == 0;
    sodium_memzero(recipientSecret, sizeof(recipientSecret));
    return opened && plain == message;
}

bool TestPacketSerialization() {
    NetworkPlayerCommandPacket command{};
    command.sequence = 100;
    command.actionSequence = 9;
    command.lifeEpoch = 3;
    command.clientTick = 700;
    command.moveX = -85;
    command.moveY = 63;
    command.heading = -12000;
    command.aimPitch = 4000;
    command.heldActions = NETWORK_ACTION_AIM;
    command.pressedActions = NETWORK_ACTION_PRIMARY;
    command.meleeAttackVariant = static_cast<uint8_t>(
        Game::Simulation::MeleeAttackVariant::LeftCombo);
    command.hasMeleeAttackVariant = 1;
    const std::string encodedCommand = BuildAppPacket(NAMTPlayerIntent, command);
    constexpr size_t encodedPlayerCommandBytes = 42;
    if (encodedCommand.size() != sizeof(NetAppMessageHeader) +
                                     encodedPlayerCommandBytes) {
        return false;
    }
    NetworkPlayerCommandPacket decodedCommand{};
    if (!ParseAppPacket(encodedCommand.data(), static_cast<__int32>(encodedCommand.size()),
                        NAMTPlayerIntent, decodedCommand) ||
        std::memcmp(&command, &decodedCommand, sizeof(command)) != 0) {
        return false;
    }

    NetworkWeaponSelectionIntentPacket weaponSelection{ 14, 3, 3 };
    const std::string encodedSelection =
        BuildAppPacket(NAMTWeaponSelectionIntent, weaponSelection);
    NetworkWeaponSelectionIntentPacket decodedSelection{};
    if (!ParseAppPacket(encodedSelection.data(),
                        static_cast<__int32>(encodedSelection.size()),
                        NAMTWeaponSelectionIntent, decodedSelection) ||
        std::memcmp(&weaponSelection, &decodedSelection, sizeof(weaponSelection)) != 0) {
        return false;
    }

    NetworkMessageRaw fishingRaw;
    NetworkFishingPresentationPacket fishingPresentation{};
    fishingPresentation.playerId = 7;
    fishingPresentation.entityIndex = 17;
    fishingPresentation.entityGeneration = 23;
    fishingPresentation.sceneId = 42;
    fishingPresentation.lifeEpoch = 3;
    fishingPresentation.sequence = 99;
    fishingPresentation.fishingState = 4;
    fishingPresentation.fishingLineSpooled = 137;
    fishingPresentation.fishingSinkingLureUnderwater = 1;
    fishingPresentation.fishingLureDrawOffset[2] = 9.5f;
    fishingPresentation.fishingLureSpin = 0.375f;
    fishingPresentation.fishingLureZOffset = -725.0f;
    fishingPresentation.fishingLureHookOffsets[0][0] = 19.25f;
    fishingPresentation.fishingLureHookOffsets[1][2] = -8.5f;
    fishingPresentation.fishingLureHookRot[0][0] = 0.75f;
    fishingPresentation.fishingLureHookRot[1][1] = -1.25f;
    fishingPresentation.fishingLineScale = 0.0015f;
    fishingPresentation.fishingLineGravity = 2.25f;
    fishingPresentation.fishingFishRot[0] = -3000;
    fishingPresentation.fishingFishRot[1] = 12000;
    fishingPresentation.fishingFishRot[2] = 900;
    EncodeFishingStateRaw(fishingRaw, fishingPresentation);
    NetworkMessageRaw encodedFishing(fishingRaw.data(), fishingRaw.size());
    NetworkFishingPresentationPacket decodedFishing{};
    if (fishingRaw.size() >= 164 || !DecodeFishingStateRaw(encodedFishing, decodedFishing) ||
        decodedFishing.playerId != fishingPresentation.playerId ||
        decodedFishing.entityIndex != fishingPresentation.entityIndex ||
        decodedFishing.entityGeneration != fishingPresentation.entityGeneration ||
        decodedFishing.sceneId != fishingPresentation.sceneId ||
        decodedFishing.sequence != fishingPresentation.sequence ||
        decodedFishing.fishingState != fishingPresentation.fishingState ||
        decodedFishing.fishingLureDrawOffset[2] != fishingPresentation.fishingLureDrawOffset[2] ||
        decodedFishing.fishingFishRot[1] != fishingPresentation.fishingFishRot[1]) {
        return false;
    }
    NetworkFishingPresentationIntentPacket fishingIntent{};
    fishingIntent.sequence = fishingPresentation.sequence;
    fishingIntent.lifeEpoch = 3;
    fishingIntent.fishingState = fishingPresentation.fishingState;
    fishingIntent.fishingLineSpooled = fishingPresentation.fishingLineSpooled;
    fishingIntent.fishingLineScale = fishingPresentation.fishingLineScale;
    fishingIntent.fishingLineGravity = fishingPresentation.fishingLineGravity;
    fishingIntent.fishingLureSpin = fishingPresentation.fishingLureSpin;
    fishingIntent.fishingLureDrawOffset[2] = fishingPresentation.fishingLureDrawOffset[2];
    NetworkMessageRaw fishingIntentRaw;
    EncodeFishingIntentRaw(fishingIntentRaw, fishingIntent);
    NetworkMessageRaw encodedFishingIntent(fishingIntentRaw.data(), fishingIntentRaw.size());
    NetworkFishingPresentationIntentPacket decodedFishingIntent{};
    if (fishingIntentRaw.size() + 16 != fishingRaw.size() ||
        !DecodeFishingIntentRaw(encodedFishingIntent, decodedFishingIntent) ||
        decodedFishingIntent.sequence != fishingIntent.sequence ||
        decodedFishingIntent.fishingLureDrawOffset[2] !=
            fishingIntent.fishingLureDrawOffset[2] ||
        !Game::Multiplayer::FishingNetworkAdapter::IsSane(decodedFishingIntent)) {
        return false;
    }

    const NetworkPlayerLifecyclePacket lifecycleSource{ 7, 17, 23, 3, 42, 1 };
    const std::string lifecycleEncoded = BuildAppPacket(NAMTPlayerLifecycle, lifecycleSource);
    NetworkPlayerLifecyclePacket lifecycleDecoded{};
    if (!ParseAppPacket(lifecycleEncoded.data(), static_cast<__int32>(lifecycleEncoded.size()),
                        NAMTPlayerLifecycle, lifecycleDecoded) ||
        std::memcmp(&lifecycleSource, &lifecycleDecoded, sizeof(lifecycleSource)) != 0) {
        return false;
    }

    NetworkCombatResultPacket combatSource{};
    combatSource.eventId = 99;
    combatSource.sourcePlayerId = 7;
    combatSource.targetPlayerId = 8;
    combatSource.sourceEntityIndex = 17;
    combatSource.sourceEntityGeneration = 23;
    combatSource.targetEntityIndex = 18;
    combatSource.targetEntityGeneration = 24;
    combatSource.sourceLifeEpoch = 3;
    combatSource.targetLifeEpoch = 3;
    combatSource.sceneId = 42;
    combatSource.attackKind = NETWORK_COMBAT_ARROW;
    combatSource.result = NETWORK_COMBAT_BLOCKED;
    combatSource.impactYaw = -12000;
    combatSource.impactX = 10.5f;
    combatSource.impactY = 31.0f;
    combatSource.impactZ = -4.25f;
    const std::string combatEncoded = BuildAppPacket(NAMTCombatResult, combatSource);
    NetworkCombatResultPacket combatDecoded{};
    if (combatEncoded.size() >= 64 ||
        !ParseAppPacket(combatEncoded.data(), static_cast<__int32>(combatEncoded.size()),
                        NAMTCombatResult, combatDecoded) ||
        std::memcmp(&combatSource, &combatDecoded, sizeof(combatSource)) != 0) {
        return false;
    }
    Game::Replication::EntityLifetimeRegistry activeCombatPlayers;
    const NetworkPlayerLifecyclePacket targetLifecycle{ 8, 18, 24, 3, 42, 1 };
    if (!Game::Multiplayer::PlayerLifecycleNetworkAdapter::Apply(
            lifecycleSource, activeCombatPlayers) ||
        !Game::Multiplayer::PlayerLifecycleNetworkAdapter::Apply(
            targetLifecycle, activeCombatPlayers)) {
        return false;
    }
    if (!Game::Multiplayer::CombatNetworkAdapter::IsSane(combatDecoded) ||
        !Game::Multiplayer::CombatNetworkAdapter::MatchesActiveLifetimes(
            combatDecoded, activeCombatPlayers) ||
        !Game::Multiplayer::CombatNetworkAdapter::MatchesActiveIncarnations(
            combatDecoded, std::map<int32_t, uint32_t>{ { 7, 3 }, { 8, 3 } })) {
        return false;
    }
    if (Game::Multiplayer::CombatNetworkAdapter::MatchesActiveIncarnations(
            combatDecoded, std::map<int32_t, uint32_t>{ { 7, 3 }, { 8, 4 } })) {
        return false;
    }
    combatDecoded.targetEntityGeneration = 25;
    if (Game::Multiplayer::CombatNetworkAdapter::MatchesActiveLifetimes(
            combatDecoded, activeCombatPlayers)) {
        return false;
    }
    combatDecoded.targetEntityGeneration = 24;
    combatDecoded.damage = 1;
    if (Game::Multiplayer::CombatNetworkAdapter::IsSane(combatDecoded)) return false;

    const NetworkSceneEntryIntentPacket sceneIntentSource{ 101, 1 };
    const std::string sceneIntentEncoded = BuildAppPacket(NAMTSceneEntryIntent, sceneIntentSource);
    NetworkSceneEntryIntentPacket sceneIntentDecoded{};
    if (sceneIntentEncoded.size() >= 32 ||
        !ParseAppPacket(sceneIntentEncoded.data(), static_cast<__int32>(sceneIntentEncoded.size()),
                        NAMTSceneEntryIntent, sceneIntentDecoded) ||
        std::memcmp(&sceneIntentSource, &sceneIntentDecoded, sizeof(sceneIntentSource)) != 0) {
        return false;
    }

    const NetworkSceneEntryStatePacket sceneStateSource{
        8, 19, 3, 101, 1, 73, 666.0f, -87.0f, 354.0f, -1234, 1
    };
    const std::string sceneStateEncoded = BuildAppPacket(NAMTSceneEntryState, sceneStateSource);
    NetworkSceneEntryStatePacket sceneStateDecoded{};
    if (sceneStateEncoded.size() >= 64 ||
        !ParseAppPacket(sceneStateEncoded.data(), static_cast<__int32>(sceneStateEncoded.size()),
                        NAMTSceneEntryState, sceneStateDecoded) ||
        std::memcmp(&sceneStateSource, &sceneStateDecoded, sizeof(sceneStateSource)) != 0) {
        return false;
    }
    Game::Simulation::PlayerSnapshot sceneSnapshot{};
    sceneSnapshot.entity = { 19, 3 };
    sceneSnapshot.ownerPlayerId = 8;
    sceneSnapshot.lifeEpoch = 1;
    sceneSnapshot.sceneId = 73;
    sceneSnapshot.position = { 666.0f, -87.0f, 354.0f };
    sceneSnapshot.headingRadians = -1234.0f * (3.14159265358979323846f / 32768.0f);
    const auto mappedSceneState = Game::Multiplayer::SceneNetworkAdapter::ToPacket(
        sceneSnapshot, 101, true);
    const auto mappedSceneAuthority =
        Game::Multiplayer::SceneNetworkAdapter::ToAuthority(sceneStateSource);
    const auto mappedSceneCommand =
        Game::Multiplayer::SceneNetworkAdapter::ToCommand(sceneIntentSource);
    if (!Game::Multiplayer::SceneNetworkAdapter::IsSane(sceneIntentSource) ||
        !Game::Multiplayer::SceneNetworkAdapter::IsSane(mappedSceneState) ||
        std::memcmp(&sceneStateSource, &mappedSceneState, sizeof(sceneStateSource)) != 0 ||
        mappedSceneAuthority.playerId != sceneStateSource.playerId ||
        mappedSceneAuthority.entity != Game::Simulation::EntityId{ 19, 3 } ||
        mappedSceneAuthority.requestSequence != 101 || mappedSceneAuthority.lifeEpoch != 1 ||
        mappedSceneAuthority.sceneId != 73 || mappedSceneAuthority.position.x != 666.0f ||
        mappedSceneAuthority.position.y != -87.0f || mappedSceneAuthority.position.z != 354.0f ||
        mappedSceneAuthority.heading != -1234 || !mappedSceneAuthority.accepted ||
        mappedSceneCommand.playerId != -1 || mappedSceneCommand.sequence != 101 ||
        mappedSceneCommand.lifeEpoch != 1) {
        return false;
    }

    NetworkProjectileStatePacket projectileSource{};
    projectileSource.playerId = 4;
    projectileSource.projectileId = 81;
    projectileSource.entityIndex = 12;
    projectileSource.entityGeneration = 7;
    projectileSource.sceneId = 42;
    projectileSource.sequence = 0x12345678;
    projectileSource.active = 1;
    projectileSource.projectileKind = NETWORK_PROJECTILE_ARROW;
    projectileSource.phase = NETWORK_ARROW_STUCK;
    projectileSource.projectileType = 2;
    projectileSource.x = -15.5f;
    projectileSource.y = 250.25f;
    projectileSource.z = 99.0f;
    projectileSource.rotationX = 0x4000;
    projectileSource.rotationY = -1234;
    projectileSource.velocityZ = 3000.0f;
    projectileSource.bodyPlayerId = 8;
    projectileSource.bodyLifeEpoch = 3;
    projectileSource.bodyRegion = 5;
    projectileSource.bodyOffsetX = 0.5f;
    projectileSource.bodyOffsetY = 0.25f;
    projectileSource.bodyOffsetZ = -0.75f;
    projectileSource.bodyDirectionZ = 1.0f;
    const std::string projectileEncoded = BuildAppPacket(NAMTProjectileState, projectileSource);
    NetworkProjectileStatePacket projectileDecoded{};
    if (!ParseAppPacket(projectileEncoded.data(), static_cast<__int32>(projectileEncoded.size()),
                        NAMTProjectileState, projectileDecoded) ||
        std::memcmp(&projectileSource, &projectileDecoded, sizeof(projectileSource)) != 0) {
        return false;
    }

    const NetworkProjectileLifecyclePacket projectileLifecycleSource{
        4, 81, 12, 7, 42, NETWORK_PROJECTILE_ARROW, 1
    };
    const std::string projectileLifecycleEncoded =
        BuildAppPacket(NAMTProjectileLifecycle, projectileLifecycleSource);
    NetworkProjectileLifecyclePacket projectileLifecycleDecoded{};
    if (!ParseAppPacket(projectileLifecycleEncoded.data(),
                        static_cast<__int32>(projectileLifecycleEncoded.size()),
                        NAMTProjectileLifecycle, projectileLifecycleDecoded) ||
        std::memcmp(&projectileLifecycleSource, &projectileLifecycleDecoded,
                    sizeof(projectileLifecycleSource)) != 0) {
        return false;
    }

    const NetworkArrowFireIntentPacket arrowIntentSource{ 77, 3, 456, 1234, -567 };
    const auto mappedArrowCommand =
        Game::Multiplayer::ProjectileNetworkAdapter::ToCommand(arrowIntentSource);
    const std::string arrowIntentEncoded = BuildAppPacket(NAMTArrowFireIntent, arrowIntentSource);
    NetworkArrowFireIntentPacket arrowIntentDecoded{};
    if (arrowIntentEncoded.size() >= 32 ||
        !ParseAppPacket(arrowIntentEncoded.data(), static_cast<__int32>(arrowIntentEncoded.size()),
                        NAMTArrowFireIntent, arrowIntentDecoded) ||
        std::memcmp(&arrowIntentSource, &arrowIntentDecoded, sizeof(arrowIntentSource)) != 0) {
        return false;
    }
    if (mappedArrowCommand.playerId != -1 || mappedArrowCommand.sequence != 77 ||
        mappedArrowCommand.lifeEpoch != 3 ||
        mappedArrowCommand.clientTick != 456 ||
        mappedArrowCommand.heading != 1234 ||
        mappedArrowCommand.aimPitch != -567) return false;
    std::string forgedArrowIntent = arrowIntentEncoded;
    forgedArrowIntent.push_back(static_cast<char>(8));
    if (ParseAppPacket(forgedArrowIntent.data(),
                       static_cast<__int32>(forgedArrowIntent.size()),
                       NAMTArrowFireIntent, arrowIntentDecoded)) {
        return false;
    }
    const NetworkProjectileIntentResultPacket projectileIntentResultSource{
        78, 3, 82, NETWORK_PROJECTILE_INTENT_ARROW_FIRE, 1
    };
    const std::string projectileIntentResultEncoded = BuildAppPacket(
        NAMTProjectileIntentResult, projectileIntentResultSource);
    NetworkProjectileIntentResultPacket projectileIntentResultDecoded{};
    if (!ParseAppPacket(projectileIntentResultEncoded.data(),
                        static_cast<__int32>(projectileIntentResultEncoded.size()),
                        NAMTProjectileIntentResult, projectileIntentResultDecoded) ||
        std::memcmp(&projectileIntentResultSource, &projectileIntentResultDecoded,
                    sizeof(projectileIntentResultSource)) != 0) {
        return false;
    }

    const NetworkFishIntentPacket fishIntentSource{
        124, 3, NETWORK_FISH_INTENT_HOOK
    };
    const auto mappedFishCommand =
        Game::Multiplayer::FishingNetworkAdapter::ToCommand(fishIntentSource);
    const std::string fishIntentEncoded = BuildAppPacket(NAMTFishIntent, fishIntentSource);
    NetworkFishIntentPacket fishIntentDecoded{};
    if (!ParseAppPacket(fishIntentEncoded.data(), static_cast<__int32>(fishIntentEncoded.size()),
                        NAMTFishIntent, fishIntentDecoded) ||
        std::memcmp(&fishIntentSource, &fishIntentDecoded, sizeof(fishIntentSource)) != 0) {
        return false;
    }
    if (mappedFishCommand.playerId != -1 || mappedFishCommand.sequence != 124 ||
        mappedFishCommand.lifeEpoch != 3 ||
        mappedFishCommand.action != Game::Simulation::FishActionKind::Hook) {
        return false;
    }
    const NetworkFishStatePacket fishStateSource{
        7, 3, 23, 4, 124, 73, 9001,
        666.0f, -45.0f, 354.0f,
        static_cast<uint8_t>(Game::Simulation::FishSpecies::HylianLoach),
        18.5f, 1
    };
    const std::string fishStateEncoded = BuildAppPacket(NAMTFishState, fishStateSource);
    NetworkFishStatePacket fishStateDecoded{};
    if (!ParseAppPacket(fishStateEncoded.data(), static_cast<__int32>(fishStateEncoded.size()),
                        NAMTFishState, fishStateDecoded) ||
        std::memcmp(&fishStateSource, &fishStateDecoded, sizeof(fishStateSource)) != 0) {
        return false;
    }

    const NetworkLureControlIntentPacket lureIntentSource{
        125, 3, NETWORK_LURE_DEPLOYED | NETWORK_LURE_REEL_HELD
    };
    const auto mappedLureCommand =
        Game::Multiplayer::FishingNetworkAdapter::ToCommand(lureIntentSource);
    const std::string lureIntentEncoded = BuildAppPacket(NAMTLureControlIntent, lureIntentSource);
    NetworkLureControlIntentPacket lureIntentDecoded{};
    if (!ParseAppPacket(lureIntentEncoded.data(), static_cast<__int32>(lureIntentEncoded.size()),
                        NAMTLureControlIntent, lureIntentDecoded) ||
        std::memcmp(&lureIntentSource, &lureIntentDecoded, sizeof(lureIntentSource)) != 0) {
        return false;
    }
    if (mappedLureCommand.playerId != -1 || mappedLureCommand.sequence != 125 ||
        mappedLureCommand.lifeEpoch != 3 || !mappedLureCommand.deployed ||
        !mappedLureCommand.reelHeld) return false;
    std::string forgedLureIntent = lureIntentEncoded;
    forgedLureIntent.push_back(static_cast<char>(2));
    if (ParseAppPacket(forgedLureIntent.data(),
                       static_cast<__int32>(forgedLureIntent.size()),
                       NAMTLureControlIntent, lureIntentDecoded)) {
        return false;
    }
    const NetworkLureStatePacket lureStateSource{
        7, 3, 24, 5, 125, 73, 666.0f, -45.0f, 354.0f,
        static_cast<unsigned char>(Game::Simulation::FishingLurePhase::Hooked), 2, 1
    };
    const std::string lureStateEncoded = BuildAppPacket(NAMTLureState, lureStateSource);
    NetworkLureStatePacket lureStateDecoded{};
    if (!ParseAppPacket(lureStateEncoded.data(), static_cast<__int32>(lureStateEncoded.size()),
                        NAMTLureState, lureStateDecoded) ||
        std::memcmp(&lureStateSource, &lureStateDecoded, sizeof(lureStateSource)) != 0) {
        return false;
    }

    NetworkPlayerRespawnPacket respawnSource{
        7, 19, 3, 4, 118, 99, 1.25f, -2.5f, 3.75f, 0x1234, 2
    };
    const std::string respawnEncoded = BuildAppPacket(NAMTPlayerRespawn, respawnSource);
    NetworkPlayerRespawnPacket respawnDecoded{};
    if (!ParseAppPacket(respawnEncoded.data(), static_cast<__int32>(respawnEncoded.size()), NAMTPlayerRespawn,
                        respawnDecoded) ||
        std::memcmp(&respawnSource, &respawnDecoded, sizeof(respawnSource)) != 0) {
        return false;
    }

    NetworkCorpseStatePacket corpseSource{};
    corpseSource.entityIndex = 19;
    corpseSource.entityGeneration = 3;
    corpseSource.sequence = 44;
    corpseSource.sourcePlayerId = 7;
    corpseSource.sourcePlayerEntityIndex = 12;
    corpseSource.sourcePlayerEntityGeneration = 3;
    corpseSource.sourceLifeEpoch = 4;
    corpseSource.sceneId = 118;
    corpseSource.roomId = 2;
    corpseSource.x = 10.5f;
    corpseSource.y = -20.25f;
    corpseSource.z = 30.75f;
    corpseSource.rotation[1] = -12345;
    corpseSource.selectedWeapon = 2;
    corpseSource.active = 1;
    const std::string corpseEncoded = BuildAppPacket(NAMTCorpseState, corpseSource);
    NetworkCorpseStatePacket corpseDecoded{};
    if (!ParseAppPacket(corpseEncoded.data(), static_cast<__int32>(corpseEncoded.size()), NAMTCorpseState,
                        corpseDecoded) ||
        std::memcmp(&corpseSource, &corpseDecoded, sizeof(corpseSource)) != 0) {
        return false;
    }

    return true;
}

bool TestProjectileNetworkAdapter() {
    namespace Adapter = Game::Multiplayer::ProjectileNetworkAdapter;

    Game::Simulation::ArrowSnapshot arrow{};
    arrow.entity = { 31, 4 };
    arrow.replicationId = 77;
    arrow.ownerPlayerId = 7;
    arrow.sceneId = 118;
    arrow.sequence = 9;
    arrow.active = true;
    arrow.phase = Game::Simulation::ArrowPhase::Stuck;
    arrow.projectileType = 2;
    arrow.position = { 10.0f, 20.0f, 30.0f };
    arrow.velocity = { 40.0f, 50.0f, 60.0f };
    arrow.rotationX = 100;
    arrow.rotationY = 200;
    arrow.rotationZ = 300;
    const NetworkProjectileStatePacket arrowPacket = Adapter::ToPacket(arrow);
    const auto arrowPresentation = Adapter::ToPresentationState(arrowPacket);
    if (!Adapter::IsSane(arrowPacket) ||
        arrowPacket.projectileKind != NETWORK_PROJECTILE_ARROW ||
        arrowPacket.phase != NETWORK_ARROW_STUCK || arrowPacket.entityIndex != 31 ||
        arrowPacket.entityGeneration != 4 || arrowPacket.sequence != 9 ||
        arrowPacket.velocityZ != 60.0f || arrowPresentation.entity != arrow.entity ||
        arrowPresentation.logicalId.ownerPlayerId != arrow.ownerPlayerId ||
        arrowPresentation.logicalId.projectileId != arrow.replicationId ||
        arrowPresentation.phase != Game::Client::RemoteProjectilePhase::ArrowStuck ||
        arrowPresentation.position.z != arrow.position.z) {
        return false;
    }

    const Game::Replication::ReplicatedOwnedEntity ownedArrow{
        { Game::Replication::OwnedEntityKind::Arrow, 7, 77 },
        { 31, 4 }, 118, { 10.0f, 20.0f, 30.0f }, false
    };
    NetworkProjectileLifecyclePacket lifecycle = Adapter::ToLifecyclePacket(ownedArrow, true);
    if (!Adapter::IsSane(lifecycle) || lifecycle.projectileKind != NETWORK_PROJECTILE_ARROW ||
        lifecycle.active != 1) {
        return false;
    }

    Game::Replication::ProjectileLifetimeRegistry lifetimes;
    auto result = Adapter::ApplyLifecycle(lifecycle, lifetimes);
    if (result.kind != Adapter::LifecycleApplyKind::Established ||
        !Adapter::MatchesActiveLifetime(arrowPacket, lifetimes)) {
        return false;
    }

    NetworkProjectileLifecyclePacket replacement = lifecycle;
    replacement.entityGeneration = 5;
    result = Adapter::ApplyLifecycle(replacement, lifetimes);
    if (result.kind != Adapter::LifecycleApplyKind::Replaced || !result.previousEntity ||
        result.previousEntity->generation != 4 ||
        Adapter::MatchesActiveLifetime(arrowPacket, lifetimes)) {
        return false;
    }
    NetworkProjectileStatePacket replacementState = arrowPacket;
    replacementState.entityGeneration = 5;
    if (!Adapter::MatchesActiveLifetime(replacementState, lifetimes)) return false;

    NetworkProjectileLifecyclePacket staleRetirement = lifecycle;
    staleRetirement.active = 0;
    if (Adapter::ApplyLifecycle(staleRetirement, lifetimes).Accepted() ||
        !Adapter::MatchesActiveLifetime(replacementState, lifetimes)) {
        return false;
    }
    replacement.active = 0;
    result = Adapter::ApplyLifecycle(replacement, lifetimes);
    if (result.kind != Adapter::LifecycleApplyKind::Retired ||
        Adapter::MatchesActiveLifetime(replacementState, lifetimes)) {
        return false;
    }
    const auto retired = Adapter::ToRetiredPresentationState(replacement);
    if (retired.active || retired.entity.generation != 5 ||
        retired.logicalId.projectileKind != NETWORK_PROJECTILE_ARROW) {
        return false;
    }

    NetworkArrowFireIntentPacket arrowIntent{ 30, 2, 99, 1234, -567 };
    if (!Adapter::IsSane(arrowIntent)) return false;
    arrowIntent.sequence = 0;
    NetworkProjectileIntentResultPacket intentResult{
        30, 2, 77, NETWORK_PROJECTILE_INTENT_ARROW_FIRE, 1
    };
    if (Adapter::IsSane(arrowIntent) || !Adapter::IsSane(intentResult)) return false;
    intentResult.projectileId = 0;
    intentResult.accepted = 0;
    if (!Adapter::IsSane(intentResult)) return false;
    intentResult.projectileId = 77;
    intentResult.accepted = 2;
    if (Adapter::IsSane(intentResult)) return false;

    lifecycle.playerId = -1;
    replacementState.playerId = -1;
    return !Adapter::IsSane(lifecycle) && !Adapter::IsSane(replacementState);
}

bool TestClientProjectilePresentationPolicy() {
    using Action = Game::Multiplayer::ClientProjectilePresentationAction;
    using Policy = Game::Multiplayer::ClientProjectilePresentationPolicy;

    constexpr int32_t localPlayerId = 7;
    Game::Client::RemoteProjectileReplicaState packet{};
    packet.logicalId.ownerPlayerId = localPlayerId;
    packet.logicalId.projectileKind = NETWORK_PROJECTILE_ARROW;
    packet.phase = Game::Client::RemoteProjectilePhase::ArrowFlying;
    packet.active = true;
    const auto evaluate = [&](bool presentationExists) {
        return Policy::Evaluate(packet, localPlayerId, presentationExists);
    };

    if (evaluate(false) != Action::Ignore ||
        evaluate(true) != Action::Retire) {
        return false;
    }

    packet.phase = Game::Client::RemoteProjectilePhase::ArrowStuck;
    if (evaluate(false) != Action::Upsert) return false;
    packet.active = false;
    if (evaluate(true) != Action::Retire ||
        evaluate(false) != Action::Ignore) {
        return false;
    }

    packet.logicalId.ownerPlayerId = 8;
    packet.active = true;
    packet.phase = Game::Client::RemoteProjectilePhase::ArrowFlying;
    if (evaluate(false) != Action::Upsert) return false;
    packet.active = false;
    if (evaluate(true) != Action::Retire) return false;

    // Exercise far more impacts than a play session's server-side stuck-arrow cap.
    // Every exact retirement must release its presentation instead of accumulating actors.
    packet.logicalId.projectileKind = NETWORK_PROJECTILE_ARROW;
    std::set<uint32_t> presentations;
    std::deque<uint32_t> stuckOrder;
    for (uint32_t projectileId = 1; projectileId <= 1000; ++projectileId) {
        packet.logicalId.ownerPlayerId = localPlayerId;
        packet.logicalId.projectileId = projectileId;
        packet.entity = { projectileId, 1 };
        packet.active = true;
        packet.phase = Game::Client::RemoteProjectilePhase::ArrowStuck;
        if (evaluate(false) != Action::Upsert) return false;
        presentations.insert(projectileId);
        stuckOrder.push_back(projectileId);

        if (stuckOrder.size() > 99) {
            const uint32_t retiredId = stuckOrder.front();
            stuckOrder.pop_front();
            packet.logicalId.projectileId = retiredId;
            packet.entity = { retiredId, 1 };
            packet.active = false;
            if (evaluate(presentations.contains(retiredId)) != Action::Retire) {
                return false;
            }
            presentations.erase(retiredId);
        }
        if (presentations.size() > 99) return false;
    }
    while (!stuckOrder.empty()) {
        const uint32_t retiredId = stuckOrder.front();
        stuckOrder.pop_front();
        packet.logicalId.projectileId = retiredId;
        packet.entity = { retiredId, 1 };
        packet.active = false;
        if (evaluate(true) != Action::Retire) return false;
        presentations.erase(retiredId);
    }
    return presentations.empty();
}

bool TestClientCombatPresentationPolicy() {
    using Action = Game::Multiplayer::ClientCombatPresentationAction;
    using Policy = Game::Multiplayer::ClientCombatPresentationPolicy;

    constexpr int32_t localPlayerId = 7;
    constexpr int32_t currentSceneId = 118;
    Game::Simulation::CombatResultEvent packet{};
    packet.sceneId = currentSceneId;
    packet.targetPlayerId = localPlayerId;
    packet.result = Game::Simulation::CombatResultKind::Blocked;

    if (Policy::Evaluate(packet, localPlayerId, currentSceneId) != Action::BlockedImpact) {
        return false;
    }
    packet.targetPlayerId = 8;
    if (Policy::Evaluate(packet, localPlayerId, currentSceneId) != Action::BlockedImpact) {
        return false;
    }

    packet.result = Game::Simulation::CombatResultKind::Damaged;
    if (Policy::Evaluate(packet, localPlayerId, currentSceneId) != Action::ObservedDamageImpact) {
        return false;
    }
    packet.targetPlayerId = localPlayerId;
    if (Policy::Evaluate(packet, localPlayerId, currentSceneId) != Action::LocalHitReaction) {
        return false;
    }
    packet.sceneId = currentSceneId + 1;
    return Policy::Evaluate(packet, localPlayerId, currentSceneId) == Action::Ignore;
}

bool TestClientPlayerActionPresentationPolicy() {
    using Base = Game::Multiplayer::ClientPlayerBaseAnimation;
    using Equipment = Game::Multiplayer::ClientEquipmentPresentation;
    using Policy = Game::Multiplayer::ClientPlayerActionPresentationPolicy;
    using Upper = Game::Multiplayer::ClientPlayerUpperAnimation;

    Game::Simulation::PlayerSnapshot snapshot{};
    snapshot.health = 48;
    snapshot.selectedWeapon = 1;
    snapshot.actionState = Game::Simulation::PlayerActionState::Blocking;
    auto presentation = Policy::Evaluate(snapshot);
    if (presentation.equipment != Equipment::MasterSwordAndShield ||
        !presentation.blocking || presentation.bowReady || presentation.dead ||
        presentation.baseAnimation != Base::BlockingSword ||
        presentation.upperAnimation != Upper::None) {
        return false;
    }

    snapshot.selectedWeapon = 0;
    presentation = Policy::Evaluate(snapshot);
    if (presentation.equipment != Equipment::None ||
        !presentation.blocking ||
        presentation.baseAnimation != Base::BlockingFree ||
        presentation.upperAnimation != Upper::None) {
        return false;
    }

    snapshot.selectedWeapon = 1;
    snapshot.velocity.z = 20.0f;
    presentation = Policy::Evaluate(snapshot);
    if (presentation.baseAnimation != Base::RunForward ||
        presentation.upperAnimation != Upper::Blocking) {
        return false;
    }
    snapshot.velocity = {};

    snapshot.selectedWeapon = 3;
    snapshot.actionState = Game::Simulation::PlayerActionState::Aiming;
    presentation = Policy::Evaluate(snapshot);
    if (presentation.equipment != Equipment::Bow || presentation.blocking ||
        !presentation.bowReady || presentation.dead ||
        presentation.upperAnimation != Upper::BowAiming) {
        return false;
    }
    snapshot.actionState = Game::Simulation::PlayerActionState::Idle;
    if (Policy::Evaluate(snapshot).bowReady) return false;

    snapshot.selectedWeapon = 1;
    snapshot.velocity.z = 4.0f;
    if (Policy::Evaluate(snapshot).baseAnimation != Base::RunForward) return false;
    snapshot.velocity.z = -2.0f;
    if (Policy::Evaluate(snapshot).baseAnimation != Base::WalkBackward) return false;
    snapshot.velocity.z = 0.0f;
    snapshot.velocity.x = 2.0f;
    if (Policy::Evaluate(snapshot).baseAnimation != Base::StrafeLeft) return false;
    snapshot.velocity.x = 0.0f;
    snapshot.actionState = Game::Simulation::PlayerActionState::PrimaryActive;
    snapshot.meleeAttackVariant = Game::Simulation::MeleeAttackVariant::ForwardSlash;
    if (Policy::Evaluate(snapshot).baseAnimation != Base::MeleeForwardSlash) return false;
    snapshot.meleeAttackVariant = Game::Simulation::MeleeAttackVariant::RightSlash;
    if (Policy::Evaluate(snapshot).baseAnimation != Base::MeleeRightSlash) return false;
    snapshot.meleeAttackVariant = Game::Simulation::MeleeAttackVariant::LeftCombo;
    snapshot.meleeAttackId = 77;
    if (Policy::Evaluate(snapshot).baseAnimation != Base::MeleeLeftCombo) return false;

    snapshot.actionState = Game::Simulation::PlayerActionState::Evading;
    snapshot.velocity = { 0.0f, 0.0f, -120.0f };
    if (Policy::Evaluate(snapshot).baseAnimation != Base::EvadeBackward) return false;
    snapshot.velocity = { 170.0f, 0.0f, 0.0f };
    if (Policy::Evaluate(snapshot).baseAnimation != Base::EvadeLeft) return false;
    snapshot.velocity = { -170.0f, 0.0f, 0.0f };
    if (Policy::Evaluate(snapshot).baseAnimation != Base::EvadeRight) return false;

    snapshot.selectedWeapon = 4;
    snapshot.actionState = Game::Simulation::PlayerActionState::Idle;
    snapshot.velocity = {};
    presentation = Policy::Evaluate(snapshot);
    if (presentation.equipment != Equipment::FishingPole ||
        presentation.baseAnimation != Base::Fishing ||
        presentation.upperAnimation != Upper::Fishing) return false;
    snapshot.health = 0;
    presentation = Policy::Evaluate(snapshot);
    return presentation.dead && !presentation.blocking && !presentation.bowReady &&
           presentation.baseAnimation == Base::Dead &&
           presentation.upperAnimation == Upper::None;
}

bool TestFishingNetworkAdapter() {
    namespace Adapter = Game::Multiplayer::FishingNetworkAdapter;

    Game::Simulation::FishSnapshot fish{};
    fish.entity = { 41, 7 };
    fish.identity = {
        118, Game::Simulation::MakeFishSpawnKey(118, 3, 666, -45, 354)
    };
    fish.ownerPlayerId = 9;
    fish.ownerLifeEpoch = 2;
    fish.position = { 660.0f, -40.0f, 350.0f };
    fish.species = Game::Simulation::FishSpecies::HylianLoach;
    fish.length = 18.5f;
    NetworkFishStatePacket fishState = Adapter::ToPacket(fish, 100, true);
    const Game::Client::RemoteFishEntity remoteFish = Adapter::ToRemoteEntity(fishState);
    if (!Adapter::IsSane(fishState) || fishState.entityGeneration != 7 ||
        fishState.spawnKey != fish.identity.spawnKey ||
        fishState.species != static_cast<uint8_t>(Game::Simulation::FishSpecies::HylianLoach) ||
        fishState.length != 18.5f ||
        fishState.active != 1 || remoteFish.ownerPlayerId != 9 ||
        remoteFish.entity != Game::Simulation::EntityId{ 41, 7 } ||
        remoteFish.identity != Game::Client::RemoteFishIdentity{ 118, fish.identity.spawnKey } ||
        remoteFish.x != 660.0f ||
        remoteFish.species != Game::Simulation::FishSpecies::HylianLoach ||
        !remoteFish.active) {
        return false;
    }

    Game::Replication::EntityLifetimeRegistry fishLifetimes;
    auto result = Adapter::ApplyLifetime(fishState, fishLifetimes);
    if (result.kind != Adapter::LifetimeApplyKind::Established ||
        !fishLifetimes.Matches(9, { 41, 7 })) {
        return false;
    }
    NetworkFishStatePacket replacement = fishState;
    replacement.entityGeneration = 8;
    replacement.sequence = 101;
    result = Adapter::ApplyLifetime(replacement, fishLifetimes);
    if (result.kind != Adapter::LifetimeApplyKind::Replaced || !result.previousEntity ||
        result.previousEntity->generation != 7 ||
        !fishLifetimes.Matches(9, { 41, 8 })) {
        return false;
    }
    NetworkFishStatePacket staleRetirement = fishState;
    staleRetirement.active = 0;
    staleRetirement.sequence = 102;
    if (Adapter::ApplyLifetime(staleRetirement, fishLifetimes).Accepted() ||
        !fishLifetimes.Matches(9, { 41, 8 })) {
        return false;
    }
    replacement.active = 0;
    replacement.sequence = 103;
    if (Adapter::ApplyLifetime(replacement, fishLifetimes).kind !=
            Adapter::LifetimeApplyKind::Retired ||
        fishLifetimes.Size() != 0) {
        return false;
    }
    NetworkFishStatePacket missingSpawn = fishState;
    missingSpawn.spawnKey = 0;
    NetworkFishStatePacket unknownSpecies = fishState;
    unknownSpecies.species = 2;
    if (Adapter::IsSane(missingSpawn) || Adapter::IsSane(unknownSpecies)) {
        return false;
    }

    Game::Simulation::FishingLureSnapshot lure{};
    lure.entity = { 52, 11 };
    lure.ownerPlayerId = 9;
    lure.ownerLifeEpoch = 2;
    lure.sceneId = 118;
    lure.position = { 10.0f, 20.0f, 30.0f };
    lure.phase = Game::Simulation::FishingLurePhase::Hooked;
    lure.lureType = 2;
    NetworkLureStatePacket lureState = Adapter::ToPacket(lure, 200, true);
    const Game::Client::RemoteLureEntity remoteLure = Adapter::ToRemoteEntity(lureState);
    Game::Replication::EntityLifetimeRegistry lureLifetimes;
    if (!Adapter::IsSane(lureState) ||
        lureState.phase != static_cast<unsigned char>(Game::Simulation::FishingLurePhase::Hooked) ||
        lureState.lureType != 2 || remoteLure.ownerPlayerId != 9 ||
        remoteLure.entity != Game::Simulation::EntityId{ 52, 11 } ||
        remoteLure.sceneId != 118 || remoteLure.phase != lureState.phase ||
        remoteLure.lureType != 2 || !remoteLure.active ||
        Adapter::ApplyLifetime(lureState, lureLifetimes).kind !=
            Adapter::LifetimeApplyKind::Established ||
        !lureLifetimes.Matches(9, { 52, 11 })) {
        return false;
    }

    NetworkFishIntentPacket fishIntent{
        10, 2, NETWORK_FISH_INTENT_HOOK
    };
    NetworkLureControlIntentPacket lureIntent{
        11, 2, NETWORK_LURE_DEPLOYED | NETWORK_LURE_REEL_HELD
    };
    NetworkFishingPresentationPacket presentation{};
    presentation.playerId = 9;
    presentation.entityIndex = 53;
    presentation.entityGeneration = 4;
    presentation.lifeEpoch = 2;
    presentation.sequence = 12;
    presentation.sceneId = 118;
    if (!Adapter::IsSane(fishIntent) || !Adapter::IsSane(lureIntent) ||
        !Adapter::IsSane(presentation)) {
        return false;
    }
    presentation.fishingRodBendY = 2.5f;
    presentation.fishingFishLimbRot[3] = 456;
    const auto presentationState = Adapter::ToState(presentation);
    const auto presentationRoundTrip = Adapter::ToPacket(presentationState);
    NetworkFishingPresentationIntentPacket presentationIntent =
        Adapter::ToIntentPacket(presentationState);
    presentationIntent.lifeEpoch = 2;
    const auto unboundPresentation = Adapter::ToIntent(presentationIntent);
    if (presentationState.entity != Game::Simulation::EntityId{ 53, 4 } ||
        presentationState.rodBendY != 2.5f ||
        presentationState.fishLimbRotation[3] != 456 ||
        presentationRoundTrip.entityGeneration != 4 ||
        presentationRoundTrip.fishingRodBendY != 2.5f ||
        presentationRoundTrip.fishingFishLimbRot[3] != 456 ||
        !Adapter::IsSane(presentationIntent) ||
        presentationIntent.fishingRodBendY != 2.5f ||
        presentationIntent.fishingFishLimbRot[3] != 456 ||
        unboundPresentation.lifeEpoch != 2 ||
        unboundPresentation.presentation.playerId != -1 ||
        unboundPresentation.presentation.entity.Valid() ||
        unboundPresentation.presentation.sceneId != -1 ||
        unboundPresentation.presentation.rodBendY != 2.5f) {
        return false;
    }
    NetworkFishingPresentationIntentPacket unsafePresentation = presentationIntent;
    unsafePresentation.fishingRodTwist = 1000.0f;
    if (Adapter::IsSane(unsafePresentation)) return false;
    unsafePresentation = presentationIntent;
    unsafePresentation.fishingState = 3;
    unsafePresentation.fishingLineScale = 0.0015f;
    unsafePresentation.fishingLureDrawOffset[2] = 100.0f;
    unsafePresentation.fishingLureHookOffsets[0][2] = 6000.0f;
    if (Adapter::IsSane(unsafePresentation)) return false;
    fishIntent.action = 0xFF;
    lureIntent.controlFlags = 0x80;
    lureState.ownerPlayerId = -1;
    return !Adapter::IsSane(fishIntent) && !Adapter::IsSane(lureIntent) &&
           !Adapter::IsSane(lureState);
}

bool TestWorldPvpNetworkAdapter() {
    namespace Adapter = Game::Multiplayer::WorldPvpNetworkAdapter;

    Game::Simulation::ObjectiveSnapshot objective{};
    objective.entity = { 61, 3 };
    objective.objectiveKey = 4;
    objective.sceneId = 118;
    objective.position = { 100.0f, 20.0f, -50.0f };
    objective.captureRadius = 300.0f;
    objective.owner = Game::Simulation::TeamId::Green;
    objective.capturingTeam = Game::Simulation::TeamId::Red;
    objective.captureProgress = 45.0f;
    objective.contested = true;
    NetworkObjectiveStatePacket objectivePacket = Adapter::ToPacket(objective, 70);
    const Game::Client::ReplicatedObjectiveState objectiveState =
        Adapter::ToClientState(objectivePacket);
    if (!Adapter::IsSane(objectivePacket) || objectivePacket.objectiveKey != 4 ||
        objectivePacket.ownerTeam != NETWORK_TEAM_GREEN || objectivePacket.active != 1 ||
        objectivePacket.sequence != 70 || !objectiveState.active ||
        objectiveState.snapshot.entity != objective.entity ||
        objectiveState.snapshot.owner != Game::Simulation::TeamId::Green ||
        objectiveState.snapshot.captureProgress != 45.0f) {
        return false;
    }
    NetworkObjectiveStatePacket retiredObjectivePacket = objectivePacket;
    retiredObjectivePacket.active = 0;
    retiredObjectivePacket.sequence = 71;
    if (!Adapter::IsSane(retiredObjectivePacket) ||
        Adapter::ToClientState(retiredObjectivePacket).active) {
        return false;
    }
    objectivePacket.captureProgress = 101.0f;
    if (Adapter::IsSane(objectivePacket)) return false;

    Game::Simulation::StructureSnapshot structure{};
    structure.entity = { 62, 5 };
    structure.structureKey = 8;
    structure.objectiveKey = 4;
    structure.sceneId = 118;
    structure.position = { 125.0f, 20.0f, -50.0f };
    structure.team = Game::Simulation::TeamId::Blue;
    structure.phase = Game::Simulation::StructurePhase::Active;
    structure.health = 450;
    structure.maximumHealth = 500;
    structure.buildProgress = 100;
    structure.requiredBuild = 100;
    NetworkStructureStatePacket structurePacket = Adapter::ToPacket(structure, 71);
    const Game::Client::ReplicatedStructureState structureState =
        Adapter::ToClientState(structurePacket);
    if (!Adapter::IsSane(structurePacket) || structurePacket.structureKey != 8 ||
        structurePacket.phase !=
            static_cast<uint8_t>(Game::Simulation::StructurePhase::Active) ||
        !structureState.active || structureState.snapshot.entity != structure.entity ||
        structureState.snapshot.phase != Game::Simulation::StructurePhase::Active ||
        structureState.snapshot.health != 450) {
        return false;
    }
    NetworkStructureStatePacket retiredStructurePacket = structurePacket;
    retiredStructurePacket.active = 0;
    retiredStructurePacket.sequence = 72;
    if (!Adapter::IsSane(retiredStructurePacket) ||
        Adapter::ToClientState(retiredStructurePacket).active) {
        return false;
    }
    structurePacket.health = 0;
    if (Adapter::IsSane(structurePacket)) return false;

    NetworkStructureActionPacket action{
        300, 2, 8, NETWORK_STRUCTURE_ACTION_REPAIR
    };
    const auto structureCommand = Adapter::ToCommand(action);
    if (!Adapter::IsSane(action) || structureCommand.playerId != -1 ||
        structureCommand.sequence != 300 || structureCommand.lifeEpoch != 2 ||
        structureCommand.structureKey != 8 ||
        structureCommand.kind != Game::Simulation::StructureActionKind::Repair) {
        return false;
    }
    action.action = 0xFF;
    if (Adapter::IsSane(action)) return false;

    Game::Client::LocalStructureActionStream actionStream(UINT32_MAX);
    const auto issuedRepair = actionStream.Issue(
        { 8, Game::Client::LocalStructureActionKind::Repair });
    const auto wrappedBuild = actionStream.Issue(
        { 8, Game::Client::LocalStructureActionKind::Build });
    if (!issuedRepair || issuedRepair->sequence != UINT32_MAX ||
        issuedRepair->request.structureKey != 8 ||
        issuedRepair->request.kind != Game::Client::LocalStructureActionKind::Repair ||
        !wrappedBuild || wrappedBuild->sequence != 1 ||
        actionStream.Issue({ -1, Game::Client::LocalStructureActionKind::Build })) {
        return false;
    }
    actionStream.BeginLife();
    const auto newLifeBuild = actionStream.Issue(
        { 9, Game::Client::LocalStructureActionKind::Build });
    if (!newLifeBuild || newLifeBuild->sequence != 1 ||
        newLifeBuild->request.structureKey != 9) {
        return false;
    }

    const std::vector<Game::Simulation::StrategicSiteDefinition> sites{
        { 4, Game::Simulation::StrategicSiteKind::Keep, 10 },
        { 5, Game::Simulation::StrategicSiteKind::Camp, 11 },
        { 6, Game::Simulation::StrategicSiteKind::Tower, 12 },
    };
    const std::vector<Game::Simulation::SupplyRouteDefinition> routes{
        { 20, 5, 4 }, { 21, 5, 6 },
    };
    const std::vector<Game::Simulation::InfluenceRegionAdjacencyDefinition>
        adjacencies{ { 30, 10, 11 }, { 31, 11, 12 } };
    const NetworkStrategicTopologyPacket topologyPacket =
        Adapter::ToPacket(sites, routes, adjacencies, 9);
    const auto topologyState = Adapter::ToClientState(topologyPacket);
    if (!Adapter::IsSane(topologyPacket) || topologyPacket.revision != 9 ||
        topologyPacket.sites.size() != 3 || topologyPacket.supplyRoutes.size() != 2 ||
        topologyPacket.influenceAdjacencies.size() != 2 ||
        topologyState.sites != sites || topologyState.supplyRoutes != routes ||
        topologyState.influenceAdjacencies != adjacencies) {
        return false;
    }
    const std::string topologyMessage =
        BuildAppPacket(NAMTStrategicTopology, topologyPacket);
    NetworkStrategicTopologyPacket decodedTopology{};
    if (!ParseAppPacket(topologyMessage.data(),
                        static_cast<int32_t>(topologyMessage.size()),
                        NAMTStrategicTopology, decodedTopology) ||
        decodedTopology.revision != topologyPacket.revision ||
        decodedTopology.sites.size() != sites.size() ||
        decodedTopology.supplyRoutes.size() != routes.size() ||
        decodedTopology.influenceAdjacencies.size() != adjacencies.size()) {
        return false;
    }
    Game::Client::ClientWorldState clientWorld;
    if (!clientWorld.ApplyStrategicTopology(topologyState) ||
        clientWorld.StrategicTopologyRevision() != 9 ||
        clientWorld.StrategicTopology().Sites() != sites ||
        clientWorld.StrategicTopology().InfluenceAdjacencies() != adjacencies ||
        clientWorld.ApplyStrategicTopology(topologyState)) {
        return false;
    }
    NetworkStrategicTopologyPacket malformedTopology = topologyPacket;
    malformedTopology.supplyRoutes.front().sourceObjectiveKey = 4;
    if (Adapter::IsSane(malformedTopology)) return false;
    malformedTopology = topologyPacket;
    malformedTopology.influenceAdjacencies.front().lowerRegionKey = 11;
    malformedTopology.influenceAdjacencies.front().upperRegionKey = 10;
    return !Adapter::IsSane(malformedTopology);
}

bool TestLocalVoiceFrameStream() {
    Game::Client::LocalVoiceFrameStream stream(UINT32_MAX);
    const auto last = stream.Issue({ 1, 2, 3 });
    const auto wrapped = stream.Issue({ 4 });
    if (!last || last->sequence != UINT32_MAX || last->opusData.size() != 3 ||
        !wrapped || wrapped->sequence != 1 || wrapped->opusData.size() != 1 ||
        stream.Issue({}) ||
        stream.Issue(std::vector<uint8_t>(
            Game::Client::kMaximumEncodedVoiceFrameBytes + 1, 0x7f))) {
        return false;
    }
    stream.Reset();
    const auto reset = stream.Issue({ 5, 6 });
    return reset && reset->sequence == 1 && reset->opusData.size() == 2;
}

bool TestLocalVoiceSubmissionService() {
    using Game::Multiplayer::LocalVoiceSubmissionRole;
    std::vector<NetworkVoiceIntentPacket> clientPackets;
    std::vector<int32_t> hostRecipients;
    std::vector<NetworkVoicePacket> hostPackets;
    LocalVoiceSubmissionRole role = LocalVoiceSubmissionRole::Inactive;
    bool failPeerFour = false;

    Game::Multiplayer::LocalVoiceSubmissionService service({
        [&]() { return role; },
        [&](const NetworkVoiceIntentPacket& packet) {
            clientPackets.push_back(packet);
            return true;
        },
        []() { return std::vector<int32_t>{ 0, 3, 4, -1 }; },
        [&](int32_t observer, const std::string& payload) {
            NetworkVoicePacket packet{};
            if (!ParseVoicePacket(payload.data(),
                                  static_cast<__int32>(payload.size()),
                                  packet)) {
                return false;
            }
            hostRecipients.push_back(observer);
            hostPackets.push_back(std::move(packet));
            return observer != 4 || !failPeerFour;
        },
    });

    // Inactive capture is rejected before assigning a sequence.
    if (service.Submit({ 0x01 }) || !clientPackets.empty() ||
        !hostRecipients.empty()) {
        return false;
    }

    role = LocalVoiceSubmissionRole::Client;
    if (!service.Submit({ 0x11, 0x12 }) || clientPackets.size() != 1 ||
        clientPackets[0].sequence != 1 ||
        clientPackets[0].codec != VOICE_CODEC_OPUS ||
        clientPackets[0].sampleRate != VOICE_SAMPLE_RATE ||
        clientPackets[0].frameSamples != VOICE_SAMPLES_PER_PACKET ||
        clientPackets[0].data != std::vector<uint8_t>({ 0x11, 0x12 })) {
        return false;
    }

    role = LocalVoiceSubmissionRole::Host;
    if (!service.Submit({ 0x21 }) ||
        hostRecipients != std::vector<int32_t>({ 3, 4 }) ||
        hostPackets.size() != 2) {
        return false;
    }
    for (const NetworkVoicePacket& packet : hostPackets) {
        if (packet.playerId != 0 || packet.sequence != 2 ||
            packet.codec != VOICE_CODEC_OPUS ||
            packet.sampleRate != VOICE_SAMPLE_RATE ||
            packet.frameSamples != VOICE_SAMPLES_PER_PACKET ||
            packet.data != std::vector<uint8_t>({ 0x21 })) {
            return false;
        }
    }

    failPeerFour = true;
    if (service.Submit({ 0x31 })) return false;
    service.Reset();
    role = LocalVoiceSubmissionRole::Client;
    if (!service.Submit({ 0x41 }) || clientPackets.size() != 2 ||
        clientPackets.back().sequence != 1) {
        return false;
    }
    return true;
}

bool TestLocalClientAdmissionService() {
    cCryptoSession clientCrypto;
    cCryptoSession serverCrypto;
    Game::Multiplayer::PrivateChatService privateChat;
    if (!privateChat.Initialize()) return false;

    bool active = false;
    bool secureDeliverySucceeds = true;
    int32_t plainCount = 0;
    int32_t secureCount = 0;
    std::string clientHello;
    NetworkIdentity submittedIdentity{};
    int32_t chatKeyPlayer = -1;
    std::string chatKeyName;
    std::string chatPublicKey;
    NetMsgFlags secureFlags = NMFNone;

    Game::Multiplayer::LocalClientAdmissionService service(
        clientCrypto, privateChat,
        {
            [&]() { return active; },
            [&](NetAppMessageType type, const NetworkMessageRaw& raw) {
                ++plainCount;
                if (type != NAMTKeyHello || !raw.data() || raw.size() <= 0) {
                    return false;
                }
                clientHello.assign(raw.data(), static_cast<size_t>(raw.size()));
                return true;
            },
            [&](NetAppMessageType type, const NetworkMessageRaw& raw,
                NetMsgFlags flags) {
                ++secureCount;
                secureFlags = flags;
                if (type == NAMTConnect) {
                    const std::string message = BuildAppRawMessage(type, raw);
                    return ParseIdentityRaw(
                               message.data(),
                               static_cast<int32_t>(message.size()),
                               submittedIdentity) &&
                           secureDeliverySucceeds;
                }
                if (type == NAMTChatKey) {
                    NetworkMessageRaw reader(raw.data(), raw.size());
                    return reader.getInt32(chatKeyPlayer) &&
                           reader.getString(chatKeyName, 48) &&
                           reader.getString(chatPublicKey,
                                            crypto_box_PUBLICKEYBYTES) &&
                           reader.fullyRead() && secureDeliverySucceeds;
                }
                return false;
            },
        });

    if (service.BeginCryptoHandshake() || service.SubmitIdentity() ||
        service.SubmitPrivateChatKey() || service.IdentitySent() ||
        plainCount != 0 || secureCount != 0) {
        return false;
    }

    active = true;
    if (!service.BeginCryptoHandshake() || plainCount != 1 ||
        clientHello.size() != crypto_kx_PUBLICKEYBYTES) {
        return false;
    }
    std::string serverReply;
    if (!serverCrypto.acceptClientHello(clientHello, serverReply) ||
        !clientCrypto.acceptServerKey(serverReply) ||
        !service.SubmitIdentity() || !service.IdentitySent() ||
        secureCount != 1 || submittedIdentity.protocolVersion != APP_PROTOCOL_VERSION ||
        submittedIdentity.publicKey.size() != crypto_sign_PUBLICKEYBYTES ||
        submittedIdentity.name != Game::Multiplayer::LocalUserName() ||
        !Game::Multiplayer::VerifyIdentityBinding(
            submittedIdentity.publicKey, clientCrypto.identityBinding(),
            submittedIdentity.signature) ||
        (secureFlags & NMFGuaranteed) == 0 ||
        (secureFlags & NMFHighPriority) == 0) {
        return false;
    }
    if (!service.SubmitPrivateChatKey() || secureCount != 2 ||
        chatKeyPlayer != 0 || chatKeyName != Game::Multiplayer::LocalUserName() ||
        chatPublicKey != privateChat.PublicKey()) {
        return false;
    }

    service.Reset();
    secureDeliverySucceeds = false;
    if (service.IdentitySent() || service.SubmitIdentity() ||
        service.IdentitySent() || secureCount != 3) {
        return false;
    }
    active = false;
    return !service.SubmitPrivateChatKey();
}

bool TestLifecycleAndCorpseAdapters() {
    namespace Corpse = Game::Multiplayer::CorpseNetworkAdapter;

    Game::Simulation::PlayerSnapshot player{};
    player.entity = { 80, 12 };
    player.ownerPlayerId = 9;
    player.sceneId = 118;
    player.position = { 10.0f, 20.0f, 30.0f };
    player.headingRadians = 0.5f;

    const Game::Replication::ReplicatedPlayer replicatedPlayer{
        9, player.entity, player.lifeEpoch, player.sceneId, player.position
    };
    const NetworkPlayerLifecyclePacket lifecycle =
        Game::Multiplayer::PlayerLifecycleNetworkAdapter::ToPacket(replicatedPlayer, true);
    const Game::Client::RemotePlayerPresentationState lifecycleState =
        Game::Multiplayer::PlayerLifecycleNetworkAdapter::ToPresentationState(lifecycle);
    if (!Game::Multiplayer::PlayerLifecycleNetworkAdapter::IsSane(lifecycle) ||
        lifecycle.playerId != 9 || lifecycle.entityGeneration != 12 || lifecycle.active != 1 ||
        lifecycleState.entity != player.entity || lifecycleState.playerId != player.ownerPlayerId ||
        lifecycleState.sceneId != player.sceneId || !lifecycleState.active) {
        return false;
    }

    Game::Simulation::CorpseSnapshot corpseSnapshot{};
    corpseSnapshot.entity = { 81, 2 };
    corpseSnapshot.pose.sourcePlayerId = player.ownerPlayerId;
    corpseSnapshot.pose.sourcePlayerEntity = player.entity;
    corpseSnapshot.pose.sourceLifeEpoch = player.lifeEpoch;
    corpseSnapshot.pose.sceneId = player.sceneId;
    corpseSnapshot.pose.roomId = -1;
    corpseSnapshot.pose.position = player.position;
    corpseSnapshot.pose.selectedWeapon = 3;
    NetworkCorpseStatePacket corpsePacket = Corpse::ToPacket(corpseSnapshot, 72);
    const Game::Client::CorpsePresentationState corpsePresentation =
        Corpse::ToPresentationState(corpsePacket);
    if (!Corpse::IsSane(corpsePacket) || corpsePacket.entityGeneration != 2 ||
        corpsePacket.selectedWeapon != 3 || corpsePacket.active != 1 ||
        corpsePacket.sequence != 72 ||
        corpsePresentation.entity != Game::Simulation::EntityId{ 81, 2 } ||
        corpsePresentation.sourcePlayerId != player.ownerPlayerId ||
        corpsePresentation.sourcePlayerEntity != player.entity ||
        corpsePresentation.sourceLifeEpoch != player.lifeEpoch ||
        corpsePresentation.sceneId != player.sceneId ||
        corpsePresentation.selectedWeapon != 3 || !corpsePresentation.active) {
        return false;
    }

    corpsePacket.roomId = 256;
    return !Corpse::IsSane(corpsePacket);
}

bool TestPlayerSimulationNetworkAdapter() {
    namespace Adapter = Game::Multiplayer::PlayerSimulationNetworkAdapter;

    NetworkPlayerCommandPacket command{};
    command.sequence = 600;
    command.actionSequence = 40;
    command.lifeEpoch = 3;
    command.clientTick = 777;
    command.moveX = -85;
    command.moveY = 42;
    command.heading = 0x2000;
    command.aimPitch = -0x1000;
    command.heldActions = NETWORK_ACTION_AIM;
    command.pressedActions = NETWORK_ACTION_PRIMARY;
    command.meleeAttackVariant = static_cast<uint8_t>(
        Game::Simulation::MeleeAttackVariant::LeftCombo);
    command.hasMeleeAttackVariant = 1;
    command.x = 12.0f;
    command.y = 34.0f;
    command.z = 56.0f;
    command.locomotionMode = static_cast<uint8_t>(
        Game::Simulation::PlayerLocomotionMode::Climbing);
    command.hasPose = 1;
    if (!Adapter::IsSane(command)) return false;
    const Game::Simulation::PlayerCommand simulationCommand = Adapter::ToCommand(command);
    if (!Adapter::IsSane(simulationCommand) ||
        simulationCommand.ownerPlayerId != -1 || simulationCommand.sequence != 600 ||
        simulationCommand.lifeEpoch != 3 || simulationCommand.clientTick != 777 ||
        simulationCommand.sceneId != -1 ||
        simulationCommand.moveX != -1.0f ||
        simulationCommand.pressedActions != command.pressedActions ||
        !simulationCommand.hasReportedMeleeAttackVariant ||
        simulationCommand.reportedMeleeAttackVariant !=
            Game::Simulation::MeleeAttackVariant::LeftCombo ||
        !simulationCommand.hasReportedPose ||
        simulationCommand.reportedPosition.y != 34.0f ||
        simulationCommand.reportedLocomotionMode !=
            Game::Simulation::PlayerLocomotionMode::Climbing) {
        return false;
    }
    const NetworkPlayerCommandPacket roundTrip = Adapter::ToPacket(simulationCommand);
    if (roundTrip.sequence != command.sequence ||
        roundTrip.clientTick != command.clientTick ||
        roundTrip.heldActions != command.heldActions ||
        roundTrip.meleeAttackVariant != command.meleeAttackVariant ||
        roundTrip.hasMeleeAttackVariant != 1 ||
        roundTrip.z != command.z ||
        roundTrip.locomotionMode != command.locomotionMode ||
        roundTrip.hasPose != 1) {
        return false;
    }
    NetworkPlayerCommandPacket invalidAttackIdentity = command;
    invalidAttackIdentity.meleeAttackVariant = 0xFF;
    if (Adapter::IsSane(invalidAttackIdentity)) return false;
    invalidAttackIdentity = command;
    invalidAttackIdentity.pressedActions = NETWORK_ACTION_EVADE;
    if (Adapter::IsSane(invalidAttackIdentity)) return false;
    const NetworkWeaponSelectionIntentPacket weaponIntent{ 44, 3, 4 };
    const auto weaponCommand = Adapter::ToCommand(weaponIntent);
    if (!Adapter::IsSane(weaponIntent) || weaponCommand.playerId != -1 ||
        weaponCommand.sequence != 44 || weaponCommand.lifeEpoch != 3 ||
        weaponCommand.selectedWeapon != 4) return false;
    Game::Simulation::PlayerCommand invalidSemanticCommand = simulationCommand;
    invalidSemanticCommand.headingRadians = std::numeric_limits<float>::quiet_NaN();
    if (Adapter::IsSane(invalidSemanticCommand)) return false;

    Game::Simulation::PlayerSnapshot snapshot{};
    snapshot.entity = { 90, 4 };
    snapshot.ownerPlayerId = 9;
    snapshot.sceneId = 118;
    snapshot.serverTick = 800;
    snapshot.lastProcessedCommand = 600;
    snapshot.lifeEpoch = 3;
    snapshot.position = { 10.0f, 20.0f, 30.0f };
    snapshot.velocity = { 1.0f, 2.0f, 3.0f };
    snapshot.headingRadians = simulationCommand.headingRadians;
    snapshot.aimPitchRadians = simulationCommand.aimPitchRadians;
    snapshot.heldActions = NETWORK_ACTION_AIM;
    snapshot.selectedWeapon = 3;
    snapshot.locomotionMode = Game::Simulation::PlayerLocomotionMode::Swimming;
    snapshot.actionState = Game::Simulation::PlayerActionState::Aiming;
    snapshot.meleeAttackVariant = Game::Simulation::MeleeAttackVariant::LeftCombo;
    snapshot.meleeAttackId = 77;
    snapshot.actionStartTick = 790;
    snapshot.health = 24;
    snapshot.team = Game::Simulation::TeamId::Red;
    snapshot.locomotionPhaseRadians = 1.25f;
    NetworkPlayerSnapshotPacket packet = Adapter::ToPacket(snapshot);
    const auto decodedSnapshot = Adapter::ToSnapshot(packet);
    if (!Adapter::IsSane(packet) || packet.entityGeneration != 4 ||
        packet.lastProcessedCommand != 600 || packet.lifeEpoch != 3 ||
        packet.heading != command.heading || packet.aimPitch != command.aimPitch ||
        packet.team != NETWORK_TEAM_RED ||
        packet.locomotionMode != static_cast<unsigned char>(
            Game::Simulation::PlayerLocomotionMode::Swimming) ||
        packet.locomotionPhaseRadians != 1.25f ||
        packet.meleeAttackVariant != static_cast<unsigned char>(
            Game::Simulation::MeleeAttackVariant::LeftCombo) ||
        packet.meleeAttackId != 77 ||
        decodedSnapshot.locomotionMode !=
            Game::Simulation::PlayerLocomotionMode::Swimming ||
        decodedSnapshot.locomotionPhaseRadians != 1.25f ||
        decodedSnapshot.meleeAttackVariant !=
            Game::Simulation::MeleeAttackVariant::LeftCombo ||
        decodedSnapshot.meleeAttackId != 77) {
        return false;
    }

    packet.actionState = NETWORK_PLAYER_ACTION_SPIN_ATTACKING;
    if (!Adapter::IsSane(packet)) return false;
    packet.actionState = NETWORK_PLAYER_ACTION_SPIN_ATTACKING + 1;
    if (Adapter::IsSane(packet)) return false;
    packet.actionState = NETWORK_PLAYER_ACTION_EVADING;

    packet.locomotionMode = static_cast<unsigned char>(
        Game::Simulation::PlayerLocomotionMode::Climbing) + 1;
    if (Adapter::IsSane(packet)) return false;
    packet.locomotionMode = static_cast<unsigned char>(
        Game::Simulation::PlayerLocomotionMode::Swimming);
    packet.locomotionPhaseRadians = std::numeric_limits<float>::quiet_NaN();
    if (Adapter::IsSane(packet)) return false;
    packet.locomotionPhaseRadians = 6.28318530717958647692f;
    if (Adapter::IsSane(packet)) return false;
    packet.locomotionPhaseRadians = 1.25f;

    Game::Simulation::PlayerSnapshot idleSwimmer = snapshot;
    idleSwimmer.velocity = {};
    const auto idleSwimPresentation =
        Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(idleSwimmer);
    idleSwimmer.velocity.z = 20.0f;
    idleSwimmer.headingRadians = 0.0f;
    const auto forwardSwimPresentation =
        Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(idleSwimmer);
    if (idleSwimPresentation.baseAnimation !=
            Game::Multiplayer::ClientPlayerBaseAnimation::SwimIdle ||
        forwardSwimPresentation.baseAnimation !=
            Game::Multiplayer::ClientPlayerBaseAnimation::SwimForward) {
        return false;
    }
    Game::Simulation::PlayerSnapshot idleEquipment = snapshot;
    idleEquipment.locomotionMode =
        Game::Simulation::PlayerLocomotionMode::Grounded;
    idleEquipment.actionState = Game::Simulation::PlayerActionState::Idle;
    idleEquipment.velocity = {};
    idleEquipment.heldActions = 0;
    idleEquipment.selectedWeapon = 0;
    const auto emptyIdle =
        Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(
            idleEquipment);
    idleEquipment.selectedWeapon = 1;
    const auto swordIdle =
        Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(
            idleEquipment);
    idleEquipment.selectedWeapon = 2;
    const auto biggoronIdle =
        Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(
            idleEquipment);
    idleEquipment.selectedWeapon = 3;
    const auto bowIdle =
        Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(
            idleEquipment);
    if (emptyIdle.equipment !=
            Game::Multiplayer::ClientEquipmentPresentation::None ||
        emptyIdle.baseAnimation !=
            Game::Multiplayer::ClientPlayerBaseAnimation::IdleFree ||
        swordIdle.baseAnimation !=
            Game::Multiplayer::ClientPlayerBaseAnimation::IdleSword ||
        biggoronIdle.baseAnimation !=
            Game::Multiplayer::ClientPlayerBaseAnimation::IdleBiggoron ||
        bowIdle.equipment !=
            Game::Multiplayer::ClientEquipmentPresentation::Bow ||
        bowIdle.baseAnimation !=
            Game::Multiplayer::ClientPlayerBaseAnimation::IdleFree ||
        bowIdle.upperAnimation !=
            Game::Multiplayer::ClientPlayerUpperAnimation::None) {
        return false;
    }
    Game::Simulation::PlayerSnapshot airborne = snapshot;
    airborne.locomotionMode =
        Game::Simulation::PlayerLocomotionMode::Airborne;
    airborne.velocity = { 0.0f, -30.0f, 0.0f };
    if (Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(airborne)
            .baseAnimation !=
        Game::Multiplayer::ClientPlayerBaseAnimation::Falling) {
        return false;
    }
    airborne.actionState = Game::Simulation::PlayerActionState::JumpSlashing;
    if (Game::Multiplayer::ClientPlayerActionPresentationPolicy::Evaluate(airborne)
            .baseAnimation !=
        Game::Multiplayer::ClientPlayerBaseAnimation::JumpSlash) {
        return false;
    }

    command.pressedActions = 0;
    if (Adapter::IsSane(command)) return false;
    packet.actionStartTick = packet.serverTick + 1;
    return !Adapter::IsSane(packet);
}

bool TestLocalPlayerCommandStream() {
    using Game::Client::LocalPlayerCommandSubmission;
    using Game::Client::LocalPlayerCommandStream;
    using Game::Client::LocalPlayerInputSample;

    LocalPlayerCommandStream stream;
    if (stream.WeaponSelectionConfirmed(3)) return false;
    const auto offeredWeapon = stream.PrepareWeaponSelection(3);
    const auto repeatedWeapon = stream.PrepareWeaponSelection(3);
    if (!offeredWeapon || offeredWeapon->sequence != 1 ||
        offeredWeapon->selectedWeapon != 3 ||
        !repeatedWeapon || repeatedWeapon->sequence != offeredWeapon->sequence ||
        repeatedWeapon->selectedWeapon != offeredWeapon->selectedWeapon) {
        return false;
    }
    stream.ResolveWeaponSelection(offeredWeapon->sequence + 1, true);
    if (!stream.PrepareWeaponSelection(3)) return false;
    stream.ResolveWeaponSelection(offeredWeapon->sequence, true);
    if (stream.PrepareWeaponSelection(3) || stream.WeaponSelectionConfirmed(3)) return false;
    stream.ObserveAuthoritativeWeapon(3, 10);
    if (!stream.WeaponSelectionConfirmed(3) || stream.WeaponSelectionConfirmed(2)) return false;
    LocalPlayerInputSample sample{};
    sample.clientTick = 100;
    sample.lifeEpoch = 1;
    sample.sceneId = 118;
    sample.moveX = 2.0f;
    sample.moveY = -2.0f;
    sample.headingRadians = 0.5f;
    sample.aimPitchRadians = -0.25f;
    sample.heldActions = Game::Simulation::PLAYER_ACTION_AIM;
    sample.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    sample.selectedWeapon = 3;

    const auto first = stream.Build(sample);
    if (!first || first->sequence != 1 || first->actionSequence != 1 ||
        first->lifeEpoch != 1 || first->clientTick != 100 ||
        first->moveX != 1.0f || first->moveY != -1.0f ||
        first->heldActions != Game::Simulation::PLAYER_ACTION_AIM ||
        first->pressedActions != Game::Simulation::PLAYER_ACTION_PRIMARY) {
        return false;
    }
    if (stream.Build(sample)) return false;

    // A reordered native sample must not receive a fresh command/action
    // sequence and execute old input as if it were current.
    sample.clientTick = 99;
    if (stream.Build(sample)) return false;

    sample.lifeEpoch = 0;
    sample.clientTick = 101;
    if (stream.Build(sample)) return false;
    sample.lifeEpoch = 1;

    sample.pressedActions = 0;
    const auto heldOnly = stream.Build(sample);
    if (!heldOnly || heldOnly->sequence != 2 || heldOnly->actionSequence != 0 ||
        heldOnly->heldActions != Game::Simulation::PLAYER_ACTION_AIM) {
        return false;
    }

    // The same numeric frame in a newly authorized scene is a distinct sample.
    sample.sceneId = 119;
    const auto newScene = stream.Build(sample);
    if (!newScene || newScene->sequence != 3 || newScene->sceneId != 119) return false;

    sample.clientTick = 102;
    sample.selectedWeapon = 5;
    if (stream.Build(sample)) return false;
    sample.selectedWeapon = 1;
    const auto corrected = stream.Build(sample);
    if (!corrected || corrected->sequence != 4) return false;

    stream.BeginLife();
    if (stream.WeaponSelectionConfirmed(3) ||
        !stream.PrepareWeaponSelection(3) ||
        stream.PrepareWeaponSelection(3)->sequence != 1) {
        return false;
    }
    sample.lifeEpoch = 2;
    sample.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    const auto newLife = stream.Build(sample);
    if (!newLife || newLife->sequence != 1 || newLife->actionSequence != 1 ||
        newLife->lifeEpoch != 2) {
        return false;
    }

    stream.Reset();
    const auto reset = stream.Build(sample);
    if (!reset || reset->sequence != 1 || reset->actionSequence != 1) return false;

    LocalPlayerCommandStream wrapping(UINT32_MAX, UINT32_MAX);
    sample.clientTick = 200;
    sample.pressedActions = Game::Simulation::PLAYER_ACTION_PRIMARY;
    const auto last = wrapping.Build(sample);
    sample.clientTick = 201;
    const auto wrapped = wrapping.Build(sample);
    if (!last || !wrapped || last->sequence != UINT32_MAX ||
        last->actionSequence != UINT32_MAX || wrapped->sequence != 1 ||
        wrapped->actionSequence != 1) {
        return false;
    }
    LocalPlayerCommandStream tickWrapping;
    sample.clientTick = UINT32_MAX;
    const auto lastTick = tickWrapping.Build(sample);
    sample.clientTick = 1;
    const auto wrappedTick = tickWrapping.Build(sample);
    if (!lastTick || !wrappedTick || wrappedTick->sequence != 2) return false;

    NetworkPlayerCommandPacket packet =
        Game::Multiplayer::PlayerSimulationNetworkAdapter::ToPacket(*first);
    packet.lifeEpoch = 2;
    if (!Game::Multiplayer::PlayerSimulationNetworkAdapter::IsSane(packet) ||
        packet.moveX != 85 || packet.moveY != -85) {
        return false;
    }
    const auto roundTrip =
        Game::Multiplayer::PlayerSimulationNetworkAdapter::ToCommand(packet);
    LocalPlayerCommandStream lifetimeStream;
    LocalPlayerInputSample lifetimeSample{};
    lifetimeSample.clientTick = 7;
    lifetimeSample.lifeEpoch = 1;
    lifetimeSample.sceneId = 118;
    const auto oldLifeCommand = lifetimeStream.Build(lifetimeSample);
    lifetimeSample.lifeEpoch = 2;
    const auto newLifeSameFrameCommand = lifetimeStream.Build(lifetimeSample);
    if (!oldLifeCommand || !newLifeSameFrameCommand ||
        newLifeSameFrameCommand->sequence != oldLifeCommand->sequence + 1 ||
        newLifeSameFrameCommand->lifeEpoch != 2 ||
        roundTrip.ownerPlayerId != -1 || roundTrip.sequence != first->sequence ||
        roundTrip.sceneId != -1 ||
        roundTrip.actionSequence != first->actionSequence ||
        roundTrip.heldActions != first->heldActions ||
        roundTrip.pressedActions != first->pressedActions) {
        return false;
    }

    // A rejected transport sample consumes its one-shot command identity but
    // must not donate elapsed time to the next successful prediction sample.
    LocalPlayerCommandStream submissionStream;
    Game::Simulation::ClientPrediction submissionPrediction;
    LocalPlayerInputSample submissionSample{};
    submissionSample.clientTick = 1;
    submissionSample.lifeEpoch = 1;
    submissionSample.sceneId = 118;
    submissionSample.moveY = 1.0f;
    Game::Simulation::PlayerSnapshot submissionBaseline{};
    submissionBaseline.lifeEpoch = 1;
    submissionBaseline.sceneId = 118;
    if (!submissionPrediction.SeedAuthoritative(submissionBaseline)) return false;
    uint32_t acceptedSequence = 0;
    bool acceptTransport = true;
    const auto sender = [&](const Game::Simulation::PlayerCommand& command) {
        if (!acceptTransport) return false;
        acceptedSequence = command.sequence;
        return true;
    };
    if (submissionStream.Submit(
            submissionSample, 1.0f / 30.0f, sender,
            submissionPrediction) != LocalPlayerCommandSubmission::Submitted) {
        return false;
    }
    submissionSample.clientTick = 2;
    acceptTransport = false;
    if (submissionStream.Submit(
            submissionSample, 0.2f, sender,
            submissionPrediction) != LocalPlayerCommandSubmission::TransportRejected ||
        submissionPrediction.PendingCommandCount() != 1) {
        return false;
    }
    submissionSample.clientTick = 3;
    acceptTransport = true;
    if (submissionStream.Submit(
            submissionSample, 1.0f / 30.0f, sender,
            submissionPrediction) != LocalPlayerCommandSubmission::Submitted ||
        acceptedSequence != 3 || submissionPrediction.PendingCommandCount() != 2) {
        return false;
    }
    Game::Simulation::PlayerSnapshot submissionAuthority{};
    submissionAuthority.sceneId = 118;
    submissionAuthority.serverTick = 10;
    submissionAuthority.actionStartTick = 10;
    submissionAuthority.lastProcessedCommand = acceptedSequence;
    submissionAuthority.lifeEpoch = 1;
    submissionAuthority.position = { 0.0f, 0.0f, 12.0f };
    return submissionPrediction.Reconcile(
               submissionAuthority, { 0.0f, 0.0f, 12.0f }) &&
           std::abs(submissionPrediction.PendingCorrection().z) < 0.001f;
}

bool TestClientReplicationInbox() {
    Game::Multiplayer::ClientReplicationInbox inbox;
    const NetworkPlayerLifecyclePacket lifetime{ 7, 10, 1, 1, 118, 1 };
    if (!inbox.AcceptPlayerLifecycle(lifetime)) return false;

    Game::Simulation::PlayerSnapshot player{};
    player.entity = { 10, 1 };
    player.ownerPlayerId = 7;
    player.sceneId = 118;
    player.serverTick = 20;
    player.health = 48;
    const NetworkPlayerSnapshotPacket snapshot =
        Game::Multiplayer::PlayerSimulationNetworkAdapter::ToPacket(player);
    const auto decodedSnapshot =
        Game::Multiplayer::PlayerSimulationNetworkAdapter::ToSnapshot(snapshot);
    if (decodedSnapshot.entity != player.entity ||
        decodedSnapshot.ownerPlayerId != player.ownerPlayerId ||
        decodedSnapshot.sceneId != player.sceneId ||
        decodedSnapshot.serverTick != player.serverTick ||
        decodedSnapshot.health != player.health ||
        decodedSnapshot.position.x != player.position.x ||
        decodedSnapshot.position.y != player.position.y ||
        decodedSnapshot.position.z != player.position.z) {
        return false;
    }
    if (!inbox.AcceptPlayerSnapshot(snapshot) || inbox.AcceptPlayerSnapshot(snapshot) ||
        inbox.PlayerSnapshotCount() != 1) {
        return false;
    }
    NetworkPlayerSnapshotPacket newerSnapshot = snapshot;
    newerSnapshot.serverTick = 21;
    if (!inbox.AcceptPlayerSnapshot(newerSnapshot) || inbox.PlayerSnapshotCount() != 1) {
        return false;
    }
    Game::Simulation::PlayerSnapshot polledSnapshot{};
    if (!inbox.Poll(polledSnapshot) || polledSnapshot.ownerPlayerId != 7 ||
        polledSnapshot.entity != Game::Simulation::EntityId{ 10, 1 } ||
        polledSnapshot.sceneId != 118 || polledSnapshot.serverTick != 21 ||
        inbox.Poll(polledSnapshot)) {
        return false;
    }

    const NetworkPlayerRespawnPacket wrongGenerationRespawn{
        7, 10, 2, 2, 118, 22, 100.0f, 20.0f, -40.0f, 0x2000, 3
    };
    const NetworkPlayerRespawnPacket respawn{
        7, 10, 1, 2, 118, 22, 100.0f, 20.0f, -40.0f, 0x2000, 3
    };

    // These reliable old-life events can remain queued while native gameplay
    // is frozen for death. The accepted respawn must retire them before the
    // client reuses per-life sequence numbers.
    const NetworkProjectileIntentResultPacket oldLifeIntent{
        30, 1, 76, NETWORK_PROJECTILE_INTENT_ARROW_FIRE, 1
    };
    NetworkCombatResultPacket oldLifeCombat{};
    oldLifeCombat.eventId = 1;
    oldLifeCombat.sourcePlayerId = -1;
    oldLifeCombat.targetPlayerId = 7;
    oldLifeCombat.targetEntityIndex = 10;
    oldLifeCombat.targetEntityGeneration = 1;
    oldLifeCombat.targetLifeEpoch = 1;
    oldLifeCombat.sceneId = 118;
    oldLifeCombat.attackKind = NETWORK_COMBAT_ENVIRONMENT;
    oldLifeCombat.result = NETWORK_COMBAT_DAMAGED;
    oldLifeCombat.damage = 1;
    if (!inbox.AcceptProjectileIntentResult(oldLifeIntent, 1) ||
        !inbox.AcceptCombatResult(oldLifeCombat) ||
        inbox.ProjectileIntentResultCount() != 1 || inbox.CombatResultCount() != 1) {
        return false;
    }
    if (inbox.AcceptPlayerRespawn(wrongGenerationRespawn) ||
        !inbox.AcceptPlayerRespawn(respawn) || inbox.AcceptPlayerRespawn(respawn) ||
        inbox.ProjectileIntentResultCount() != 0 || inbox.CombatResultCount() != 0) {
        return false;
    }
    NetworkPlayerSnapshotPacket staleLife = snapshot;
    staleLife.serverTick = 22;
    if (inbox.AcceptPlayerSnapshot(staleLife)) return false;
    NetworkPlayerSnapshotPacket currentLife = staleLife;
    currentLife.lifeEpoch = 2;
    if (!inbox.AcceptPlayerSnapshot(currentLife) || !inbox.Poll(polledSnapshot) ||
        polledSnapshot.lifeEpoch != 2) {
        return false;
    }
    oldLifeCombat.eventId = 2;
    if (inbox.AcceptCombatResult(oldLifeCombat)) return false;
    Game::Simulation::PlayerRespawnEvent polledRespawn{};
    if (!inbox.Poll(polledRespawn) || polledRespawn.lifeEpoch != 2 ||
        polledRespawn.playerId != 7 ||
        polledRespawn.entity != Game::Simulation::EntityId{ 10, 1 } ||
        polledRespawn.sceneId != 118 || polledRespawn.serverTick != 22 ||
        polledRespawn.position.x != 100.0f || polledRespawn.position.y != 20.0f ||
        polledRespawn.position.z != -40.0f || polledRespawn.selectedWeapon != 3 ||
        inbox.Poll(polledRespawn)) {
        return false;
    }

    // Disposable state may arrive before the reliable life boundary. Both are
    // admitted against the same generation, but the respawn retains its own
    // complete placement baseline and advances the epoch floor so an old-life
    // snapshot arriving afterward cannot resurrect dead presentation state.
    Game::Multiplayer::ClientReplicationInbox reorderedRespawnInbox;
    const NetworkPlayerLifecyclePacket reorderedLifetime{ 9, 50, 2, 2, 118, 1 };
    if (!reorderedRespawnInbox.AcceptPlayerLifecycle(reorderedLifetime)) {
        return false;
    }
    NetworkPlayerSnapshotPacket reorderedSnapshot = snapshot;
    reorderedSnapshot.playerId = 9;
    reorderedSnapshot.entityIndex = 50;
    reorderedSnapshot.entityGeneration = 2;
    reorderedSnapshot.serverTick = 110;
    reorderedSnapshot.lifeEpoch = 2;
    reorderedSnapshot.x = 900.0f;
    const NetworkPlayerRespawnPacket reorderedRespawn{
        9, 50, 2, 2, 118, 108, 700.0f, 30.0f, -80.0f, -0x1800, 4
    };
    NetworkPlayerSnapshotPacket wrongSceneSnapshot = reorderedSnapshot;
    wrongSceneSnapshot.sceneId = 119;
    NetworkPlayerRespawnPacket wrongSceneRespawn = reorderedRespawn;
    wrongSceneRespawn.sceneId = 119;
    if (reorderedRespawnInbox.AcceptPlayerSnapshot(wrongSceneSnapshot) ||
        reorderedRespawnInbox.AcceptPlayerRespawn(wrongSceneRespawn) ||
        !reorderedRespawnInbox.AcceptPlayerSnapshot(reorderedSnapshot) ||
        !reorderedRespawnInbox.AcceptPlayerRespawn(reorderedRespawn)) {
        return false;
    }
    NetworkPlayerSnapshotPacket reorderedOldLife = reorderedSnapshot;
    reorderedOldLife.serverTick = 111;
    reorderedOldLife.lifeEpoch = 1;
    if (reorderedRespawnInbox.AcceptPlayerSnapshot(reorderedOldLife) ||
        !reorderedRespawnInbox.Poll(polledSnapshot) ||
        polledSnapshot.lifeEpoch != 2 || polledSnapshot.position.x != 900.0f ||
        !reorderedRespawnInbox.Poll(polledRespawn) ||
        polledRespawn.lifeEpoch != 2 || polledRespawn.serverTick != 108 ||
        polledRespawn.position.x != 700.0f || polledRespawn.position.y != 30.0f ||
        polledRespawn.position.z != -80.0f || polledRespawn.selectedWeapon != 4) {
        return false;
    }
    reorderedSnapshot.serverTick = 112;
    if (!reorderedRespawnInbox.AcceptPlayerSnapshot(reorderedSnapshot) ||
        reorderedRespawnInbox.PlayerSnapshotCount() != 1) {
        return false;
    }
    const NetworkPlayerLifecyclePacket movedLifetime{ 9, 50, 2, 2, 119, 1 };
    if (!reorderedRespawnInbox.AcceptPlayerLifecycle(movedLifetime) ||
        reorderedRespawnInbox.PlayerSnapshotCount() != 0) {
        return false;
    }
    reorderedSnapshot.serverTick = 113;
    if (reorderedRespawnInbox.AcceptPlayerSnapshot(reorderedSnapshot)) {
        return false;
    }
    reorderedSnapshot.sceneId = 119;
    if (!reorderedRespawnInbox.AcceptPlayerSnapshot(reorderedSnapshot)) {
        return false;
    }

    NetworkProjectileIntentResultPacket acceptedIntent{
        31, 2, 77, NETWORK_PROJECTILE_INTENT_ARROW_FIRE, 1
    };
    if (inbox.AcceptProjectileIntentResult(acceptedIntent, 1) ||
        !inbox.AcceptProjectileIntentResult(acceptedIntent, 2) ||
        inbox.ProjectileIntentResultCount() != 1) {
        return false;
    }
    Game::Client::LocalProjectileIntentDecision polledIntent{};
    if (!inbox.Poll(polledIntent) || polledIntent.sequence != 31 ||
        !polledIntent.accepted ||
        polledIntent.kind != Game::Client::LocalProjectileIntentKind::FireArrow ||
        inbox.Poll(polledIntent)) {
        return false;
    }

    // Owner motion remains native-predicted, but terminal server outcomes must
    // reconcile only the exact authoritative slot/generation established by
    // reliable lifecycle replication.
    const NetworkProjectileLifecyclePacket localArrowLifetime{
        7, 77, 30, 4, 118, NETWORK_PROJECTILE_ARROW, 1
    };
    if (!inbox.AcceptProjectileLifecycle(localArrowLifetime)) return false;
    NetworkProjectileStatePacket localArrow{};
    localArrow.playerId = 7;
    localArrow.projectileId = 77;
    localArrow.entityIndex = 30;
    localArrow.entityGeneration = 4;
    localArrow.sceneId = 118;
    localArrow.sequence = 1;
    localArrow.active = 1;
    localArrow.projectileKind = NETWORK_PROJECTILE_ARROW;
    localArrow.phase = NETWORK_ARROW_FLYING;
    localArrow.projectileType = 2;
    if (inbox.AcceptProjectileState(localArrow, 7)) return false;
    localArrow.phase = NETWORK_ARROW_STUCK;
    localArrow.sequence = 2;
    if (!inbox.AcceptProjectileState(localArrow, 7)) return false;
    Game::Client::RemoteProjectileReplicaState polledProjectile{};
    if (!inbox.Poll(polledProjectile) ||
        polledProjectile.phase != Game::Client::RemoteProjectilePhase::ArrowStuck ||
        polledProjectile.entity != Game::Simulation::EntityId{ 30, 4 } ||
        polledProjectile.logicalId.ownerPlayerId != 7) {
        return false;
    }
    NetworkProjectileStatePacket wrongArrowLifetime = localArrow;
    wrongArrowLifetime.entityGeneration = 5;
    wrongArrowLifetime.sequence = 3;
    if (inbox.AcceptProjectileState(wrongArrowLifetime, 7)) return false;
    localArrow.active = 0;
    localArrow.phase = NETWORK_ARROW_BLOCKED;
    localArrow.sequence = 3;
    if (!inbox.AcceptProjectileState(localArrow, 7) ||
        !inbox.Poll(polledProjectile) || polledProjectile.active) {
        return false;
    }

    // A reliable generation replacement is the exact presentation boundary:
    // already-admitted state for the old generation must not remain pollable.
    localArrow.active = 1;
    localArrow.phase = NETWORK_ARROW_STUCK;
    localArrow.sequence = 4;
    if (!inbox.AcceptProjectileState(localArrow, 7) ||
        inbox.ProjectileStateCount() != 1) {
        return false;
    }
    NetworkProjectileLifecyclePacket replacementArrowLifetime = localArrowLifetime;
    replacementArrowLifetime.entityGeneration = 5;
    if (!inbox.AcceptProjectileLifecycle(replacementArrowLifetime) ||
        inbox.ProjectileStateCount() != 0 ||
        inbox.AcceptProjectileState(localArrow, 7)) {
        return false;
    }
    NetworkProjectileStatePacket replacementArrow = localArrow;
    replacementArrow.entityGeneration = 5;
    replacementArrow.sequence = 1;
    if (!inbox.AcceptProjectileState(replacementArrow, 7) ||
        !inbox.Poll(polledProjectile) ||
        polledProjectile.entity != Game::Simulation::EntityId{ 30, 5 } ||
        polledProjectile.sequence != 1) {
        return false;
    }
    replacementArrowLifetime.active = 0;
    if (!inbox.AcceptProjectileLifecycle(replacementArrowLifetime) ||
        !inbox.Poll(polledProjectile) || polledProjectile.active ||
        polledProjectile.entity != Game::Simulation::EntityId{ 30, 5 } ||
        polledProjectile.logicalId.projectileId != 77) {
        return false;
    }

    NetworkFishingPresentationPacket presentation{};
    presentation.playerId = 7;
    presentation.entityIndex = 10;
    presentation.entityGeneration = 1;
    presentation.sceneId = 118;
    presentation.lifeEpoch = 2;
    presentation.sequence = 1;
    NetworkFishingPresentationPacket previousLifePresentation = presentation;
    previousLifePresentation.lifeEpoch = 1;
    if (inbox.AcceptFishingPresentation(previousLifePresentation) ||
        !inbox.AcceptFishingPresentation(presentation) ||
        inbox.AcceptFishingPresentation(presentation)) return false;
    presentation.sequence = 2;
    if (!inbox.AcceptFishingPresentation(presentation) ||
        inbox.FishingPresentationCount() != 1) {
        return false;
    }
    Game::Replication::FishingPresentationState polledFishing{};
    if (!inbox.Poll(polledFishing) || polledFishing.playerId != 7 ||
        polledFishing.entity != Game::Simulation::EntityId{ 10, 1 } ||
        polledFishing.sceneId != 118 || polledFishing.sequence != 2) {
        return false;
    }
    presentation.sequence = 3;
    if (!inbox.AcceptFishingPresentation(presentation) ||
        inbox.FishingPresentationCount() != 1) {
        return false;
    }
    NetworkFishingPresentationPacket malformedPresentation = presentation;
    malformedPresentation.sequence = 4;
    malformedPresentation.fishingLureDrawOffset[0] =
        std::numeric_limits<float>::quiet_NaN();
    if (inbox.AcceptFishingPresentation(malformedPresentation) ||
        inbox.FishingPresentationCount() != 1) {
        return false;
    }
    NetworkPlayerSnapshotPacket pendingBeforeReplacement = currentLife;
    pendingBeforeReplacement.serverTick = 23;
    if (!inbox.AcceptPlayerSnapshot(pendingBeforeReplacement) ||
        inbox.PlayerSnapshotCount() != 1) {
        return false;
    }
    const NetworkPlayerLifecyclePacket replacement{ 7, 10, 2, 2, 118, 1 };
    if (!inbox.AcceptPlayerLifecycle(replacement) ||
        inbox.PlayerSnapshotCount() != 0 ||
        inbox.FishingPresentationCount() != 0 ||
        inbox.AcceptFishingPresentation(presentation)) {
        return false;
    }
    NetworkPlayerLifecyclePacket previousLifeLifecycle = replacement;
    previousLifeLifecycle.lifeEpoch = 1;
    if (inbox.AcceptPlayerLifecycle(previousLifeLifecycle)) return false;
    previousLifeLifecycle.active = 0;
    if (inbox.AcceptPlayerLifecycle(previousLifeLifecycle)) return false;
    const NetworkPlayerLifecyclePacket combatTargetLifetime{ 8, 11, 1, 1, 118, 1 };
    if (!inbox.AcceptPlayerLifecycle(combatTargetLifetime)) return false;

    Game::Simulation::CombatResultEvent combatEvent{};
    combatEvent.eventId = 50;
    combatEvent.sourcePlayerId = 7;
    combatEvent.targetPlayerId = 8;
    combatEvent.sourceEntity = { 10, 2 };
    combatEvent.targetEntity = { 11, 1 };
    combatEvent.sourceLifeEpoch = 2;
    combatEvent.targetLifeEpoch = 1;
    combatEvent.sceneId = 118;
    combatEvent.attackKind = Game::Simulation::CombatAttackKind::Arrow;
    combatEvent.result = Game::Simulation::CombatResultKind::Blocked;
    combatEvent.impactPosition = { 1.0f, 2.0f, 3.0f };
    NetworkCombatResultPacket combat = Game::Multiplayer::CombatNetworkAdapter::ToPacket(combatEvent);
    const Game::Simulation::CombatResultEvent decodedCombat =
        Game::Multiplayer::CombatNetworkAdapter::ToEvent(combat);
    if (decodedCombat.eventId != combatEvent.eventId ||
        decodedCombat.sourceEntity != combatEvent.sourceEntity ||
        decodedCombat.targetEntity != combatEvent.targetEntity ||
        decodedCombat.attackKind != combatEvent.attackKind ||
        decodedCombat.result != combatEvent.result ||
        decodedCombat.meleeAttackId != 0 ||
        decodedCombat.hitRegion != Game::Simulation::PlayerHitRegion::None ||
        decodedCombat.impactPosition.z != combatEvent.impactPosition.z) {
        return false;
    }
    Game::Simulation::CombatResultEvent damagedCombatEvent = combatEvent;
    damagedCombatEvent.result = Game::Simulation::CombatResultKind::Damaged;
    damagedCombatEvent.damage = 8;
    damagedCombatEvent.hitRegion = Game::Simulation::PlayerHitRegion::Torso;
    const NetworkCombatResultPacket damagedCombat =
        Game::Multiplayer::CombatNetworkAdapter::ToPacket(damagedCombatEvent);
    const Game::Simulation::CombatResultEvent decodedDamagedCombat =
        Game::Multiplayer::CombatNetworkAdapter::ToEvent(damagedCombat);
    if (!Game::Multiplayer::CombatNetworkAdapter::IsSane(damagedCombat) ||
        decodedDamagedCombat.hitRegion != Game::Simulation::PlayerHitRegion::Torso) {
        return false;
    }
    Game::Simulation::CombatResultEvent meleeCombatEvent = damagedCombatEvent;
    meleeCombatEvent.attackKind = Game::Simulation::CombatAttackKind::Melee;
    meleeCombatEvent.meleeAttackId = 37;
    const NetworkCombatResultPacket meleeCombat =
        Game::Multiplayer::CombatNetworkAdapter::ToPacket(meleeCombatEvent);
    const Game::Simulation::CombatResultEvent decodedMeleeCombat =
        Game::Multiplayer::CombatNetworkAdapter::ToEvent(meleeCombat);
    if (!Game::Multiplayer::CombatNetworkAdapter::IsSane(meleeCombat) ||
        decodedMeleeCombat.meleeAttackId != 37) {
        return false;
    }
    NetworkCombatResultPacket invalidMeleeIdentity = meleeCombat;
    invalidMeleeIdentity.meleeAttackId = 0;
    if (Game::Multiplayer::CombatNetworkAdapter::IsSane(invalidMeleeIdentity)) return false;
    invalidMeleeIdentity = damagedCombat;
    invalidMeleeIdentity.meleeAttackId = 37;
    if (Game::Multiplayer::CombatNetworkAdapter::IsSane(invalidMeleeIdentity)) return false;
    NetworkCombatResultPacket invalidHitRegion = damagedCombat;
    invalidHitRegion.hitRegion = Game::Simulation::kPlayerHitRegionCount;
    if (Game::Multiplayer::CombatNetworkAdapter::IsSane(invalidHitRegion)) return false;
    invalidHitRegion = damagedCombat;
    invalidHitRegion.hitRegion = static_cast<unsigned char>(Game::Simulation::PlayerHitRegion::None);
    if (Game::Multiplayer::CombatNetworkAdapter::IsSane(invalidHitRegion)) return false;
    invalidHitRegion = combat;
    invalidHitRegion.hitRegion = static_cast<unsigned char>(Game::Simulation::PlayerHitRegion::Head);
    if (Game::Multiplayer::CombatNetworkAdapter::IsSane(invalidHitRegion)) return false;
    if (!inbox.AcceptCombatResult(combat) || inbox.AcceptCombatResult(combat)) return false;
    combat.eventId = 49;
    if (inbox.AcceptCombatResult(combat)) return false;
    combat.eventId = 51;
    if (!inbox.AcceptCombatResult(combat)) return false;
    Game::Simulation::CombatResultEvent polledCombat{};
    if (!inbox.Poll(polledCombat) || polledCombat.eventId != 50 ||
        !inbox.Poll(polledCombat) || polledCombat.eventId != 51 ||
        inbox.Poll(polledCombat)) {
        return false;
    }

    Game::Simulation::FishSnapshot fish{};
    fish.entity = { 20, 1 };
    fish.ownerPlayerId = 7;
    fish.ownerLifeEpoch = 2;
    fish.identity = {
        118, Game::Simulation::MakeFishSpawnKey(118, 0, 1, 2, 3)
    };
    fish.position = { 1.0f, 2.0f, 3.0f };
    NetworkFishStatePacket fishState =
        Game::Multiplayer::FishingNetworkAdapter::ToPacket(fish, 30, true);
    NetworkFishStatePacket invalidFishOwner = fishState;
    invalidFishOwner.ownerPlayerId = -1;
    NetworkFishStatePacket invalidFishSequence = fishState;
    invalidFishSequence.sequence = 0;
    NetworkFishStatePacket wrongSceneFish = fishState;
    wrongSceneFish.sceneId = 119;
    NetworkFishStatePacket previousLifeFish = fishState;
    previousLifeFish.ownerLifeEpoch = 1;
    if (Game::Multiplayer::FishingNetworkAdapter::IsSane(invalidFishOwner) ||
        Game::Multiplayer::FishingNetworkAdapter::IsSane(invalidFishSequence) ||
        inbox.AcceptFishState(wrongSceneFish) || inbox.AcceptFishState(previousLifeFish)) {
        return false;
    }
    if (!inbox.AcceptFishState(fishState) || inbox.AcceptFishState(fishState)) return false;
    NetworkFishStatePacket staleFishState = fishState;
    staleFishState.sequence = 29;
    if (inbox.AcceptFishState(staleFishState)) return false;
    NetworkFishStatePacket replacementFishState = fishState;
    replacementFishState.entityGeneration = 2;
    replacementFishState.sequence = 31;
    if (!inbox.AcceptFishState(replacementFishState)) return false;
    NetworkFishStatePacket wrongFishRetirement = fishState;
    wrongFishRetirement.sequence = 32;
    wrongFishRetirement.active = 0;
    if (inbox.AcceptFishState(wrongFishRetirement)) return false;
    NetworkFishStatePacket validFishRetirement = replacementFishState;
    validFishRetirement.sequence = 32;
    validFishRetirement.active = 0;
    if (!inbox.AcceptFishState(validFishRetirement)) return false;
    Game::Client::RemoteFishEntity polledFishState{};
    if (!inbox.Poll(polledFishState) || polledFishState.active ||
        polledFishState.entity != Game::Simulation::EntityId{ 20, 2 } ||
        polledFishState.identity != Game::Client::RemoteFishIdentity{
            118, fish.identity.spawnKey } ||
        inbox.Poll(polledFishState)) {
        return false;
    }

    fishState.entityGeneration = 3;
    fishState.sequence = 33;
    if (!inbox.AcceptFishState(fishState)) return false;
    NetworkLureStatePacket lureState{};
    lureState.ownerPlayerId = 7;
    lureState.ownerLifeEpoch = 2;
    lureState.entityIndex = 21;
    lureState.entityGeneration = 1;
    lureState.sequence = 1;
    lureState.sceneId = 118;
    lureState.phase = 1;
    lureState.lureType = 2;
    lureState.active = 1;
    NetworkLureStatePacket wrongSceneLure = lureState;
    wrongSceneLure.sceneId = 119;
    NetworkLureStatePacket previousLifeLure = lureState;
    previousLifeLure.ownerLifeEpoch = 1;
    if (inbox.AcceptLureState(wrongSceneLure) ||
        inbox.AcceptLureState(previousLifeLure)) return false;
    if (!inbox.AcceptLureState(lureState)) return false;

    // Player retirement is the aggregate client lifetime boundary. It must
    // purge already-admitted owner entities and their pending presentation so
    // an old active packet cannot recreate fishing state after the player left.
    const NetworkPlayerLifecyclePacket playerLeave{ 7, 10, 2, 2, 118, 0 };
    if (!inbox.AcceptPlayerLifecycle(playerLeave) || inbox.FishStateCount() != 0 ||
        inbox.LureStateCount() != 0 || inbox.AcceptFishState(fishState) ||
        inbox.AcceptLureState(lureState)) {
        return false;
    }
    NetworkFishStatePacket departedFishRetirement = fishState;
    departedFishRetirement.sequence = 34;
    departedFishRetirement.active = 0;
    NetworkLureStatePacket departedLureRetirement = lureState;
    departedLureRetirement.sequence = 2;
    departedLureRetirement.active = 0;
    if (!inbox.AcceptFishState(departedFishRetirement) ||
        !inbox.AcceptLureState(departedLureRetirement)) {
        return false;
    }
    const NetworkPlayerLifecyclePacket playerReturn{ 7, 10, 3, 2, 118, 1 };
    fishState.entityGeneration = 4;
    fishState.sequence = 35;
    lureState.entityGeneration = 2;
    lureState.sequence = 3;
    if (!inbox.AcceptPlayerLifecycle(playerReturn) || !inbox.AcceptFishState(fishState) ||
        !inbox.AcceptLureState(lureState)) {
        return false;
    }

    // Owner visibility leaving after a reliable release must not erase the
    // terminal state that removes the already-presented fish or lure.
    Game::Multiplayer::ClientReplicationInbox terminalInbox;
    if (!terminalInbox.AcceptPlayerLifecycle({ 9, 90, 1, 2, 118, 1 })) return false;
    NetworkFishStatePacket terminalFish = fishState;
    terminalFish.ownerPlayerId = 9;
    terminalFish.entityIndex = 91;
    terminalFish.entityGeneration = 1;
    terminalFish.sequence = 1;
    terminalFish.active = 1;
    NetworkLureStatePacket terminalLure = lureState;
    terminalLure.ownerPlayerId = 9;
    terminalLure.entityIndex = 92;
    terminalLure.entityGeneration = 1;
    terminalLure.sequence = 1;
    terminalLure.active = 1;
    if (!terminalInbox.AcceptFishState(terminalFish) ||
        !terminalInbox.AcceptLureState(terminalLure)) {
        return false;
    }
    terminalFish.sequence = 2;
    terminalFish.active = 0;
    terminalLure.sequence = 2;
    terminalLure.active = 0;
    if (!terminalInbox.AcceptFishState(terminalFish) ||
        !terminalInbox.AcceptLureState(terminalLure) ||
        !terminalInbox.AcceptPlayerLifecycle({ 9, 90, 1, 2, 118, 0 }) ||
        terminalInbox.FishStateCount() != 1 || terminalInbox.LureStateCount() != 1) {
        return false;
    }

    // A reliable same-generation scene transition is an aggregate owner
    // presentation boundary. Pending active old-scene state must disappear,
    // delayed old-scene activity must be rejected, and a new exact entity in
    // the admitted scene may establish normally.
    Game::Multiplayer::ClientReplicationInbox fishingSceneInbox;
    if (!fishingSceneInbox.AcceptPlayerLifecycle({ 12, 120, 1, 1, 118, 1 })) {
        return false;
    }
    NetworkFishStatePacket sceneFish = terminalFish;
    sceneFish.ownerPlayerId = 12;
    sceneFish.ownerLifeEpoch = 1;
    sceneFish.entityIndex = 121;
    sceneFish.entityGeneration = 1;
    sceneFish.sequence = 1;
    sceneFish.sceneId = 118;
    sceneFish.spawnKey = 121;
    sceneFish.active = 1;
    NetworkLureStatePacket sceneLure = terminalLure;
    sceneLure.ownerPlayerId = 12;
    sceneLure.ownerLifeEpoch = 1;
    sceneLure.entityIndex = 122;
    sceneLure.entityGeneration = 1;
    sceneLure.sequence = 1;
    sceneLure.sceneId = 118;
    sceneLure.active = 1;
    if (!fishingSceneInbox.AcceptFishState(sceneFish) ||
        !fishingSceneInbox.AcceptLureState(sceneLure) ||
        !fishingSceneInbox.AcceptPlayerLifecycle({ 12, 120, 1, 1, 119, 1 }) ||
        fishingSceneInbox.FishStateCount() != 0 ||
        fishingSceneInbox.LureStateCount() != 0) {
        return false;
    }
    ++sceneFish.sequence;
    ++sceneLure.sequence;
    if (fishingSceneInbox.AcceptFishState(sceneFish) ||
        fishingSceneInbox.AcceptLureState(sceneLure)) {
        return false;
    }
    ++sceneFish.entityGeneration;
    sceneFish.sceneId = 119;
    sceneFish.spawnKey = 123;
    ++sceneLure.entityGeneration;
    sceneLure.sceneId = 119;
    if (!fishingSceneInbox.AcceptFishState(sceneFish) ||
        !fishingSceneInbox.AcceptLureState(sceneLure)) {
        return false;
    }
    Game::Client::RemoteFishEntity retainedFish{};
    Game::Client::RemoteLureEntity retainedLure{};
    if (!terminalInbox.Poll(retainedFish) || retainedFish.active ||
        retainedFish.entity != Game::Simulation::EntityId{ 91, 1 } ||
        !terminalInbox.Poll(retainedLure) || retainedLure.active ||
        retainedLure.entity != Game::Simulation::EntityId{ 92, 1 }) {
        return false;
    }

    Game::Multiplayer::ClientReplicationInbox sceneInbox;
    NetworkSceneEntryStatePacket sceneReply{};
    sceneReply.playerId = 7;
    sceneReply.entityIndex = 70;
    sceneReply.entityGeneration = 4;
    sceneReply.requestSequence = 11;
    sceneReply.lifeEpoch = 2;
    sceneReply.sceneId = 118;
    sceneReply.x = 10.0f;
    sceneReply.y = 20.0f;
    sceneReply.z = 30.0f;
    sceneReply.heading = 123;
    sceneReply.accepted = 0;
    if (sceneInbox.AcceptSceneEntryState(sceneReply, 7, 1) ||
        sceneInbox.AcceptSceneEntryState(sceneReply, 8, 2)) {
        return false;
    }
    sceneReply.lifeEpoch = 1;
    if (!sceneInbox.AcceptSceneEntryState(sceneReply, 7, 1)) return false;
    Game::Client::LocalSceneAuthority rejectedScene{};
    if (!sceneInbox.Poll(rejectedScene) || rejectedScene.accepted ||
        rejectedScene.playerId != 7 ||
        rejectedScene.entity != Game::Simulation::EntityId{ 70, 4 } ||
        rejectedScene.requestSequence != 11 || rejectedScene.lifeEpoch != 1 ||
        rejectedScene.sceneId != 118 || rejectedScene.position.x != 10.0f ||
        rejectedScene.position.y != 20.0f || rejectedScene.position.z != 30.0f ||
        rejectedScene.heading != 123) {
        return false;
    }
    sceneReply.requestSequence = 0;
    sceneReply.lifeEpoch = 3;
    sceneReply.accepted = 1;
    if (!sceneInbox.AcceptPlayerLifecycle({ 7, 70, 4, 1, 118, 1 }) ||
        !sceneInbox.AcceptSceneEntryState(sceneReply, 7, 1)) return false;
    Game::Client::LocalSceneAuthority bootstrapScene{};
    if (!sceneInbox.Poll(bootstrapScene) || !bootstrapScene.accepted ||
        bootstrapScene.requestSequence != 0 || bootstrapScene.lifeEpoch != 3) {
        return false;
    }

    const auto objectivePacket = [](int32_t key, float progress, uint32_t sequence = 1) {
        NetworkObjectiveStatePacket packet{};
        packet.entityIndex = static_cast<uint32_t>(key + 1);
        packet.entityGeneration = 1;
        packet.sequence = sequence;
        packet.objectiveKey = key;
        packet.sceneId = 118;
        packet.captureRadius = 300.0f;
        packet.captureProgress = progress;
        packet.active = 1;
        return packet;
    };
    const auto structurePacket = [](uint32_t health, uint32_t sequence) {
        NetworkStructureStatePacket packet{};
        packet.entityIndex = 13;
        packet.entityGeneration = 1;
        packet.sequence = sequence;
        packet.structureKey = 12;
        packet.objectiveKey = 0;
        packet.sceneId = 118;
        packet.health = health;
        packet.maximumHealth = 100;
        packet.buildProgress = 100;
        packet.requiredBuild = 100;
        packet.active = 1;
        packet.team = NETWORK_TEAM_RED;
        packet.phase = static_cast<uint8_t>(Game::Simulation::StructurePhase::Active);
        return packet;
    };

    for (int32_t index = 0; index < 300; ++index) {
        if (!inbox.AcceptObjectiveState(objectivePacket(index, 0.0f))) return false;
    }
    if (inbox.ObjectiveStateCount() != 300) return false;
    Game::Client::ReplicatedObjectiveState objective{};
    if (!inbox.Poll(objective) || objective.snapshot.objectiveKey != 0 ||
        !objective.active) return false;

    NetworkObjectiveStatePacket revisedObjective = objectivePacket(1000, 10.0f, 1);
    if (!inbox.AcceptObjectiveState(revisedObjective)) return false;
    revisedObjective.captureProgress = 90.0f;
    revisedObjective.sequence = 2;
    if (!inbox.AcceptObjectiveState(revisedObjective)) return false;
    NetworkObjectiveStatePacket staleObjective = revisedObjective;
    staleObjective.sequence = 1;
    staleObjective.captureProgress = 5.0f;
    if (inbox.AcceptObjectiveState(staleObjective)) return false;
    if (inbox.ObjectiveStateCount() != 300) return false;

    namespace WorldAdapter = Game::Multiplayer::WorldPvpNetworkAdapter;
    const std::vector<Game::Simulation::StrategicSiteDefinition> topologySites{
        { 1000, Game::Simulation::StrategicSiteKind::Keep, 1 },
        { 1001, Game::Simulation::StrategicSiteKind::Camp, 2 },
    };
    const std::vector<Game::Simulation::SupplyRouteDefinition> topologyRoutes{
        { 2000, 1001, 1000 },
    };
    const std::vector<Game::Simulation::InfluenceRegionAdjacencyDefinition>
        topologyAdjacencies{ { 3000, 1, 2 } };
    const NetworkStrategicTopologyPacket topology =
        WorldAdapter::ToPacket(topologySites, topologyRoutes,
                               topologyAdjacencies, 10);
    if (!inbox.AcceptStrategicTopology(topology) ||
        inbox.AcceptStrategicTopology(topology) ||
        inbox.StrategicTopologyStateCount() != 1) {
        return false;
    }
    NetworkStrategicTopologyPacket malformedTopology = topology;
    malformedTopology.revision = 11;
    malformedTopology.supplyRoutes.front().sourceObjectiveKey = 1000;
    if (inbox.AcceptStrategicTopology(malformedTopology)) return false;
    malformedTopology = topology;
    malformedTopology.revision = 11;
    malformedTopology.influenceAdjacencies.front().upperRegionKey = 99;
    if (inbox.AcceptStrategicTopology(malformedTopology)) return false;
    const NetworkStrategicTopologyPacket removedTopology =
        WorldAdapter::ToPacket(
             std::vector<Game::Simulation::StrategicSiteDefinition>{},
             std::vector<Game::Simulation::SupplyRouteDefinition>{}, 11);
    if (!inbox.AcceptStrategicTopology(removedTopology) ||
        inbox.StrategicTopologyStateCount() != 1) {
        return false;
    }
    Game::Client::ReplicatedStrategicTopologyState semanticTopology{};
    if (!inbox.Poll(semanticTopology) || semanticTopology.revision != 11 ||
        !semanticTopology.sites.empty() || !semanticTopology.supplyRoutes.empty() ||
        !semanticTopology.influenceAdjacencies.empty()) {
        return false;
    }

    NetworkStructureStatePacket structure = structurePacket(100, 1);
    if (!inbox.AcceptStructureState(structure)) return false;
    structure = structurePacket(50, 2);
    if (!inbox.AcceptStructureState(structure)) return false;
    NetworkStructureStatePacket staleStructure = structurePacket(75, 1);
    if (inbox.AcceptStructureState(staleStructure)) return false;
    Game::Client::ReplicatedStructureState semanticStructure{};
    if (inbox.StructureStateCount() != 1 || !inbox.Poll(semanticStructure) ||
        semanticStructure.snapshot.health != 50 || !semanticStructure.active) {
        return false;
    }

    for (int32_t playerId = 1000; playerId < 1300; ++playerId) {
        const uint32_t entityIndex = static_cast<uint32_t>(playerId + 1);
        if (!inbox.AcceptPlayerLifecycle({ playerId, entityIndex, 1, 1, 118, 1 })) {
            return false;
        }
        NetworkPlayerSnapshotPacket queued{};
        queued.playerId = playerId;
        queued.entityIndex = entityIndex;
        queued.entityGeneration = 1;
        queued.sceneId = 118;
        queued.serverTick = 1;
        queued.lifeEpoch = 1;
        queued.health = 48;
        if (!inbox.AcceptPlayerSnapshot(queued)) return false;
    }
    if (inbox.PlayerSnapshotCount() != 300) return false;

    // A large interest reconciliation must retain every logical entity, while
    // revisions of one entity replace only that entity's pending current state.
    // Ordered reliable events must retain every accepted event as well.
    inbox.Reset();
    for (int32_t index = 0; index < 300; ++index) {
        if (!inbox.AcceptPlayerLifecycle(
                { index, static_cast<uint32_t>(index + 1), 1,
                  static_cast<uint32_t>(index + 1), 118, 1 })) {
            return false;
        }
    }

    for (int32_t index = 0; index < 300; ++index) {

        NetworkFishStatePacket queuedFish{};
        queuedFish.ownerPlayerId = index;
        queuedFish.ownerLifeEpoch = static_cast<uint32_t>(index + 1);
        queuedFish.entityIndex = static_cast<uint32_t>(index + 1);
        queuedFish.entityGeneration = 1;
        queuedFish.sequence = 1;
        queuedFish.sceneId = 118;
        queuedFish.spawnKey = static_cast<uint32_t>(index + 1);
        queuedFish.length = 10.0f;
        queuedFish.active = 1;
        if (!inbox.AcceptFishState(queuedFish)) return false;

        NetworkLureStatePacket queuedLure{};
        queuedLure.ownerPlayerId = index;
        queuedLure.ownerLifeEpoch = static_cast<uint32_t>(index + 1);
        queuedLure.entityIndex = static_cast<uint32_t>(index + 1);
        queuedLure.entityGeneration = 1;
        queuedLure.sequence = 1;
        queuedLure.sceneId = 118;
        queuedLure.lureType = 2;
        queuedLure.active = 1;
        if (!inbox.AcceptLureState(queuedLure)) return false;

        NetworkProjectileStatePacket queuedProjectile{};
        queuedProjectile.playerId = index;
        queuedProjectile.projectileId = index + 1;
        queuedProjectile.entityIndex = static_cast<uint32_t>(index + 1000);
        queuedProjectile.entityGeneration = 1;
        queuedProjectile.sceneId = 118;
        queuedProjectile.sequence = 1;
        queuedProjectile.projectileKind = NETWORK_PROJECTILE_ARROW;
        queuedProjectile.active = 1;
        if (!inbox.AcceptProjectileLifecycle(
                { queuedProjectile.playerId, queuedProjectile.projectileId,
                  queuedProjectile.entityIndex, queuedProjectile.entityGeneration,
                  queuedProjectile.sceneId, NETWORK_PROJECTILE_ARROW, 1 }) ||
            !inbox.AcceptProjectileState(queuedProjectile, -1)) {
            return false;
        }

        NetworkCorpseStatePacket queuedCorpse{};
        queuedCorpse.entityIndex = static_cast<uint32_t>(index);
        queuedCorpse.entityGeneration = 1;
        queuedCorpse.sequence = 1;
        queuedCorpse.sourcePlayerId = index;
        queuedCorpse.sourcePlayerEntityIndex = static_cast<uint32_t>(index + 1);
        queuedCorpse.sourcePlayerEntityGeneration = 1;
        queuedCorpse.sourceLifeEpoch = 1;
        queuedCorpse.sceneId = 118;
        queuedCorpse.roomId = 0;
        queuedCorpse.active = 1;
        if (!inbox.AcceptCorpseState(queuedCorpse)) return false;

        NetworkSceneEntryStatePacket queuedScene{};
        queuedScene.playerId = index;
        queuedScene.entityIndex = static_cast<uint32_t>(index + 1);
        queuedScene.entityGeneration = 1;
        queuedScene.requestSequence = static_cast<uint32_t>(index + 1);
        queuedScene.lifeEpoch = 1;
        queuedScene.sceneId = 118;
        queuedScene.accepted = 1;
        if (!inbox.AcceptSceneEntryState(queuedScene, index, 1)) return false;

        if (!inbox.AcceptPlayerRespawn(
                { index, static_cast<uint32_t>(index + 1), 1,
                  static_cast<uint32_t>(index + 1), 118,
                  static_cast<uint32_t>(index + 1),
                  static_cast<float>(index), 0.0f, 0.0f, 0, 1 })) {
            return false;
        }

        NetworkCombatResultPacket queuedCombat{};
        queuedCombat.eventId = static_cast<uint32_t>(index + 1);
        queuedCombat.sourcePlayerId = -1;
        queuedCombat.targetPlayerId = index;
        queuedCombat.targetEntityIndex = static_cast<uint32_t>(index + 1);
        queuedCombat.targetEntityGeneration = 1;
        queuedCombat.targetLifeEpoch = static_cast<uint32_t>(index + 1);
        queuedCombat.sceneId = 118;
        queuedCombat.attackKind = NETWORK_COMBAT_ENVIRONMENT;
        queuedCombat.result = NETWORK_COMBAT_DAMAGED;
        queuedCombat.damage = 1;
        if (!inbox.AcceptCombatResult(queuedCombat)) return false;
    }
    if (inbox.PlayerLifecycleCount() != 300 || inbox.FishStateCount() != 300 ||
        inbox.LureStateCount() != 300 || inbox.ProjectileStateCount() != 300 ||
        inbox.CorpseStateCount() != 300 || inbox.SceneEntryStateCount() != 300 ||
        inbox.PlayerRespawnCount() != 300 || inbox.CombatResultCount() != 300) {
        return false;
    }

    if (!inbox.AcceptPlayerLifecycle({ 0, 1, 2, 1, 118, 1 })) return false;
    NetworkFishStatePacket revisedFish{};
    revisedFish.ownerPlayerId = 0;
    revisedFish.ownerLifeEpoch = 1;
    revisedFish.entityIndex = 1;
    revisedFish.entityGeneration = 1;
    revisedFish.sequence = 2;
    revisedFish.sceneId = 118;
    revisedFish.spawnKey = 1;
    revisedFish.length = 10.0f;
    revisedFish.active = 0;
    if (!inbox.AcceptFishState(revisedFish)) return false;
    NetworkLureStatePacket revisedLure{};
    revisedLure.ownerPlayerId = 0;
    revisedLure.ownerLifeEpoch = 1;
    revisedLure.entityIndex = 1;
    revisedLure.entityGeneration = 1;
    revisedLure.sequence = 2;
    revisedLure.sceneId = 118;
    revisedLure.lureType = 2;
    revisedLure.active = 0;
    if (!inbox.AcceptLureState(revisedLure)) return false;
    NetworkProjectileStatePacket revisedProjectile{};
    revisedProjectile.playerId = 0;
    revisedProjectile.projectileId = 1;
    revisedProjectile.entityIndex = 1000;
    revisedProjectile.entityGeneration = 1;
    revisedProjectile.sceneId = 118;
    revisedProjectile.sequence = 2;
    revisedProjectile.projectileKind = NETWORK_PROJECTILE_ARROW;
    revisedProjectile.active = 0;
    if (!inbox.AcceptProjectileState(revisedProjectile, -1)) return false;
    NetworkProjectileStatePacket distinctProjectile = revisedProjectile;
    distinctProjectile.projectileId = 301;
    distinctProjectile.entityIndex = 2000;
    distinctProjectile.sequence = 1;
    if (!inbox.AcceptProjectileLifecycle(
            { distinctProjectile.playerId, distinctProjectile.projectileId,
              distinctProjectile.entityIndex, distinctProjectile.entityGeneration,
              distinctProjectile.sceneId, NETWORK_PROJECTILE_ARROW, 1 }) ||
        !inbox.AcceptProjectileState(distinctProjectile, -1)) {
        return false;
    }
    NetworkCorpseStatePacket retiredCorpse{};
    retiredCorpse.entityIndex = 0;
    retiredCorpse.entityGeneration = 1;
    retiredCorpse.sequence = 2;
    retiredCorpse.sourcePlayerId = 0;
    retiredCorpse.sourcePlayerEntityIndex = 1;
    retiredCorpse.sourcePlayerEntityGeneration = 1;
    retiredCorpse.sourceLifeEpoch = 1;
    retiredCorpse.sceneId = 118;
    retiredCorpse.roomId = 0;
    retiredCorpse.active = 0;
    if (!inbox.AcceptCorpseState(retiredCorpse)) return false;
    NetworkCorpseStatePacket replacementCorpse = retiredCorpse;
    replacementCorpse.entityGeneration = 2;
    replacementCorpse.sequence = 3;
    replacementCorpse.active = 1;
    if (!inbox.AcceptCorpseState(replacementCorpse)) return false;
    NetworkCorpseStatePacket staleCorpse = retiredCorpse;
    staleCorpse.active = 1;
    if (inbox.AcceptCorpseState(staleCorpse)) return false;

    if (inbox.PlayerLifecycleCount() != 300 || inbox.FishStateCount() != 300 ||
        inbox.LureStateCount() != 300 || inbox.ProjectileStateCount() != 301 ||
        inbox.CorpseStateCount() != 300) {
        return false;
    }
    Game::Client::RemotePlayerPresentationState queuedLifecycle{};
    Game::Client::RemoteFishEntity queuedFish{};
    Game::Client::RemoteLureEntity queuedLure{};
    Game::Client::RemoteProjectileReplicaState queuedProjectile{};
    Game::Client::CorpsePresentationState queuedCorpse{};
    if (!inbox.Poll(queuedLifecycle) || queuedLifecycle.entity.generation != 2) {
        return false;
    }
    uint32_t fishStatesPolled = 0;
    bool revisedFishPolled = false;
    while (inbox.Poll(queuedFish)) {
        ++fishStatesPolled;
        if (queuedFish.ownerPlayerId == 0) {
            revisedFishPolled = !queuedFish.active &&
                queuedFish.entity == Game::Simulation::EntityId{ 1, 1 };
        } else if (!queuedFish.active || !queuedFish.entity.Valid()) {
            return false;
        }
    }
    uint32_t lureStatesPolled = 0;
    bool revisedLurePolled = false;
    while (inbox.Poll(queuedLure)) {
        ++lureStatesPolled;
        if (queuedLure.ownerPlayerId == 0) {
            revisedLurePolled = !queuedLure.active &&
                queuedLure.entity == Game::Simulation::EntityId{ 1, 1 };
        } else if (!queuedLure.active || !queuedLure.entity.Valid()) {
            return false;
        }
    }
    if (fishStatesPolled != 300 || !revisedFishPolled ||
        lureStatesPolled != 300 || !revisedLurePolled ||
        !inbox.Poll(queuedProjectile) || queuedProjectile.sequence != 2 ||
        queuedProjectile.logicalId.projectileKind != NETWORK_PROJECTILE_ARROW ||
        !inbox.Poll(queuedCorpse) || !queuedCorpse.active ||
        queuedCorpse.entity != Game::Simulation::EntityId{ 0, 2 }) {
        return false;
    }
    Game::Client::LocalSceneAuthority queuedScene{};
    Game::Simulation::PlayerRespawnEvent queuedRespawn{};
    for (uint32_t expected = 1; expected <= 300; ++expected) {
        if (!inbox.Poll(queuedScene) || queuedScene.requestSequence != expected ||
            !inbox.Poll(queuedRespawn) || queuedRespawn.lifeEpoch != expected ||
            !inbox.Poll(polledCombat) || polledCombat.eventId != expected) {
            return false;
        }
    }
    if (inbox.Poll(queuedScene) || inbox.Poll(queuedRespawn) || inbox.Poll(polledCombat)) {
        return false;
    }
    inbox.Reset();
    return inbox.PlayerSnapshotCount() == 0 && inbox.ObjectiveStateCount() == 0 &&
           inbox.StrategicTopologyStateCount() == 0 &&
           inbox.StructureStateCount() == 0 &&
           inbox.PlayerLifecycleCount() == 0 && inbox.FishStateCount() == 0 &&
           inbox.LureStateCount() == 0 && inbox.ProjectileStateCount() == 0 &&
           inbox.ProjectileIntentResultCount() == 0 &&
           inbox.CorpseStateCount() == 0 && inbox.SceneEntryStateCount() == 0 &&
           inbox.PlayerRespawnCount() == 0 && inbox.CombatResultCount() == 0;
}

class ClientDispatchCapture final : public Game::Multiplayer::ClientProtocolSink {
  public:
    void OnClientPlayerSnapshot(const NetworkPlayerSnapshotPacket& packet) override {
        ++snapshotCount;
        snapshot = packet;
    }

    void OnClientChat(const std::string& value) override {
        ++chatCount;
        chat = value;
    }

    void OnClientProjectileIntentResult(
        const NetworkProjectileIntentResultPacket& value) override {
        ++projectileIntentResultCount;
        projectileIntentResult = value;
    }

    void OnClientStrategicTopology(
        const NetworkStrategicTopologyPacket& value) override {
        ++strategicTopologyCount;
        strategicTopology = value;
    }

    int32_t snapshotCount = 0;
    int32_t chatCount = 0;
    int32_t projectileIntentResultCount = 0;
    int32_t strategicTopologyCount = 0;
    NetworkPlayerSnapshotPacket snapshot{};
    NetworkProjectileIntentResultPacket projectileIntentResult{};
    NetworkStrategicTopologyPacket strategicTopology{};
    std::string chat;
};

class ServerDispatchCapture final : public Game::Multiplayer::ServerProtocolSink {
  public:
    void OnServerKeyHello(int32_t player, const std::string& value) override {
        ++keyCount;
        sender = player;
        key = value;
    }

    void OnServerIdentity(int32_t player, const NetworkIdentity& value) override {
        ++identityCount;
        sender = player;
        identity = value;
    }

    void OnServerPlayerCommand(int32_t player, NetworkPlayerCommandPacket value) override {
        ++commandCount;
        sender = player;
        command = value;
    }


    void OnServerWeaponSelection(int32_t player,
                                 NetworkWeaponSelectionIntentPacket value) override {
        ++weaponSelectionCount;
        sender = player;
        weaponSelection = value;
    }

    void OnServerFishingPresentation(
        int32_t player, NetworkFishingPresentationIntentPacket value) override {
        ++fishingCount;
        sender = player;
        fishing = value;
    }

    void OnServerVoice(int32_t player, NetworkVoiceIntentPacket value) override {
        ++voiceCount;
        sender = player;
        voice = std::move(value);
    }

    int32_t keyCount = 0;
    int32_t identityCount = 0;
    int32_t commandCount = 0;
    int32_t weaponSelectionCount = 0;
    int32_t fishingCount = 0;
    int32_t voiceCount = 0;
    int32_t sender = -1;
    std::string key;
    NetworkIdentity identity{};
    NetworkPlayerCommandPacket command{};
    NetworkWeaponSelectionIntentPacket weaponSelection{};
    NetworkFishingPresentationIntentPacket fishing{};
    NetworkVoiceIntentPacket voice{};
};

bool TestProtocolDispatcher() {
    using Game::Multiplayer::ProtocolDispatchResult;
    using Game::Multiplayer::ProtocolDispatcher;

    ClientDispatchCapture client;
    NetworkPlayerSnapshotPacket snapshot{};
    snapshot.playerId = 7;
    snapshot.entityIndex = 20;
    snapshot.entityGeneration = 3;
    snapshot.sceneId = 118;
    snapshot.serverTick = 44;
    snapshot.lifeEpoch = 2;
    snapshot.health = 48;
    snapshot.locomotionMode = static_cast<unsigned char>(
        Game::Simulation::PlayerLocomotionMode::Swimming);
    const std::string snapshotMessage = BuildAppPacket(NAMTPlayerSnapshot, snapshot);
    if (ProtocolDispatcher::DispatchClient(snapshotMessage.data(),
                                           static_cast<int32_t>(snapshotMessage.size()), client) !=
            ProtocolDispatchResult::Dispatched ||
        client.snapshotCount != 1 || client.snapshot.serverTick != 44 ||
        client.snapshot.locomotionMode != static_cast<unsigned char>(
            Game::Simulation::PlayerLocomotionMode::Swimming)) {
        return false;
    }

    const NetworkStrategicTopologyPacket topology{
        5,
         { { 1, 10, static_cast<uint8_t>(Game::Simulation::StrategicSiteKind::Camp) },
           { 2, 11, static_cast<uint8_t>(Game::Simulation::StrategicSiteKind::Keep) } },
         { { 3, 1, 2 } },
         { { 4, 10, 11 } }
    };
    const std::string topologyMessage =
        BuildAppPacket(NAMTStrategicTopology, topology);
    if (ProtocolDispatcher::DispatchClient(
            topologyMessage.data(), static_cast<int32_t>(topologyMessage.size()),
            client) != ProtocolDispatchResult::Dispatched ||
        client.strategicTopologyCount != 1 ||
        client.strategicTopology.revision != 5 ||
        client.strategicTopology.sites.size() != 2 ||
        client.strategicTopology.influenceAdjacencies.size() != 1 ||
        ProtocolDispatcher::DispatchClient(
            topologyMessage.data(), static_cast<int32_t>(topologyMessage.size() - 1),
            client) != ProtocolDispatchResult::Malformed ||
        client.strategicTopologyCount != 1) {
        return false;
    }

    if (ProtocolDispatcher::DispatchClient(snapshotMessage.data(),
                                           static_cast<int32_t>(snapshotMessage.size() - 1), client) !=
            ProtocolDispatchResult::Malformed ||
        client.snapshotCount != 1) {
        return false;
    }

    const NetworkProjectileIntentResultPacket projectileIntentResult{
        18, 2, 44, NETWORK_PROJECTILE_INTENT_ARROW_FIRE, 0
    };
    const std::string projectileIntentResultMessage = BuildAppPacket(
        NAMTProjectileIntentResult, projectileIntentResult);
    if (ProtocolDispatcher::DispatchClient(
            projectileIntentResultMessage.data(),
            static_cast<int32_t>(projectileIntentResultMessage.size()), client) !=
            ProtocolDispatchResult::Dispatched ||
        client.projectileIntentResultCount != 1 ||
        client.projectileIntentResult.sequence != 18 ||
        client.projectileIntentResult.accepted != 0) {
        return false;
    }

    NetworkMessageRaw chatRaw;
    chatRaw.putString("dispatcher chat", CHAT_MAX_LINE_CHARS);
    const std::string chatMessage = BuildAppRawMessage(NAMTChat, chatRaw);
    if (ProtocolDispatcher::DispatchClient(chatMessage.data(),
                                           static_cast<int32_t>(chatMessage.size()), client) !=
            ProtocolDispatchResult::Dispatched ||
        client.chatCount != 1 || client.chat != "dispatcher chat") {
        return false;
    }

    // Removed compatibility slots are not valid-but-unsupported messages.
    // They are invalid protocol bytes in both directions, while an active
    // message sent in the wrong direction remains explicitly unsupported.
    constexpr uint8_t retiredMessageIds[] = { 2, 4, 7, 9, 17, 18, 19, 20, 21, 22 };
    for (const uint8_t retiredId : retiredMessageIds) {
        const NetAppMessageHeader retired{
            static_cast<NetAppMessageType>(retiredId)
        };
        if (ValidAppMessageType(retired.type) ||
            ProtocolDispatcher::DispatchClient(
                reinterpret_cast<const char*>(&retired), sizeof(retired), client) !=
                ProtocolDispatchResult::Malformed) {
            return false;
        }
    }
    const NetAppMessageHeader wrongDirection{ NAMTPlayerIntent };
    if (ProtocolDispatcher::DispatchClient(
            reinterpret_cast<const char*>(&wrongDirection), sizeof(wrongDirection), client) !=
        ProtocolDispatchResult::Unsupported) return false;
    const unsigned char invalidType = 0xFF;
    if (ProtocolDispatcher::DispatchClient(reinterpret_cast<const char*>(&invalidType), 1, client) !=
        ProtocolDispatchResult::Malformed) {
        return false;
    }

    ServerDispatchCapture server;
    for (const uint8_t retiredId : retiredMessageIds) {
        const NetAppMessageHeader retired{
            static_cast<NetAppMessageType>(retiredId)
        };
        if (ProtocolDispatcher::DispatchServer(
                7, reinterpret_cast<const char*>(&retired), sizeof(retired), false, server) !=
            ProtocolDispatchResult::Malformed) {
            return false;
        }
    }
    NetworkPlayerCommandPacket command{};
    command.sequence = 8;
    command.lifeEpoch = 1;
    const std::string commandMessage = BuildAppPacket(NAMTPlayerIntent, command);
    if (ProtocolDispatcher::DispatchServer(7, commandMessage.data(),
                                           static_cast<int32_t>(commandMessage.size()), true, server) !=
            ProtocolDispatchResult::Malformed ||
        server.commandCount != 0) {
        return false;
    }
    NetworkWeaponSelectionIntentPacket weaponSelection{ 12, 4, 2 };
    const std::string weaponSelectionMessage =
        BuildAppPacket(NAMTWeaponSelectionIntent, weaponSelection);
    if (ProtocolDispatcher::DispatchServer(
            7, weaponSelectionMessage.data(),
            static_cast<int32_t>(weaponSelectionMessage.size()), true, server) !=
            ProtocolDispatchResult::Malformed ||
        server.weaponSelectionCount != 0) {
        return false;
    }

    NetworkFishingPresentationIntentPacket fishingIntent{};
    fishingIntent.sequence = 9;
    fishingIntent.lifeEpoch = 2;
    NetworkMessageRaw fishingIntentRaw;
    EncodeFishingIntentRaw(fishingIntentRaw, fishingIntent);
    const std::string fishingIntentMessage =
        BuildAppRawMessage(NAMTFishingState, fishingIntentRaw);
    if (ProtocolDispatcher::DispatchServer(
            7, fishingIntentMessage.data(),
            static_cast<int32_t>(fishingIntentMessage.size()), false, server) !=
            ProtocolDispatchResult::Dispatched ||
        server.fishingCount != 1 || server.sender != 7 ||
        server.fishing.sequence != 9) {
        return false;
    }
    NetworkFishingPresentationPacket replicatedFishing{};
    replicatedFishing.playerId = 7;
    replicatedFishing.entityIndex = 20;
    replicatedFishing.entityGeneration = 3;
    replicatedFishing.sceneId = 118;
    replicatedFishing.sequence = 9;
    NetworkMessageRaw replicatedFishingRaw;
    EncodeFishingStateRaw(replicatedFishingRaw, replicatedFishing);
    const std::string replicatedFishingMessage =
        BuildAppRawMessage(NAMTFishingState, replicatedFishingRaw);
    if (ProtocolDispatcher::DispatchServer(
            7, replicatedFishingMessage.data(),
            static_cast<int32_t>(replicatedFishingMessage.size()), false, server) !=
            ProtocolDispatchResult::Malformed || server.fishingCount != 1) {
        return false;
    }

    NetworkVoiceIntentPacket voiceIntent{};
    voiceIntent.sequence = 10;
    voiceIntent.codec = VOICE_CODEC_OPUS;
    voiceIntent.sampleRate = VOICE_SAMPLE_RATE;
    voiceIntent.frameSamples = VOICE_SAMPLES_PER_PACKET;
    voiceIntent.data = { 1, 2, 3 };
    const std::string voiceIntentMessage = BuildVoiceIntentPayload(voiceIntent);
    if (ProtocolDispatcher::DispatchServer(
            7, voiceIntentMessage.data(), static_cast<int32_t>(voiceIntentMessage.size()),
            false, server) != ProtocolDispatchResult::Dispatched ||
        server.voiceCount != 1 || server.sender != 7 || server.voice.sequence != 10) {
        return false;
    }
    NetworkVoicePacket replicatedVoice{};
    replicatedVoice.playerId = 7;
    replicatedVoice.sequence = 10;
    replicatedVoice.codec = VOICE_CODEC_OPUS;
    replicatedVoice.sampleRate = VOICE_SAMPLE_RATE;
    replicatedVoice.frameSamples = VOICE_SAMPLES_PER_PACKET;
    replicatedVoice.data = { 1, 2, 3 };
    const std::string replicatedVoiceMessage = BuildVoicePayload(replicatedVoice);
    if (ProtocolDispatcher::DispatchServer(
            7, replicatedVoiceMessage.data(),
            static_cast<int32_t>(replicatedVoiceMessage.size()), false, server) !=
            ProtocolDispatchResult::Malformed || server.voiceCount != 1) {
        return false;
    }

    NetworkMessageRaw identityRaw;
    identityRaw.putInt32(APP_PROTOCOL_VERSION);
    const std::string dispatcherPublicKey(crypto_sign_PUBLICKEYBYTES, 'P');
    const std::string dispatcherSignature(crypto_sign_BYTES, 'S');
    identityRaw.putString(dispatcherPublicKey, crypto_sign_PUBLICKEYBYTES);
    identityRaw.putString("Dispatcher", 48);
    identityRaw.putUInt8(0);
    identityRaw.putString(dispatcherSignature, crypto_sign_BYTES);
    const std::string identityMessage = BuildAppRawMessage(NAMTConnect, identityRaw);
    if (ProtocolDispatcher::DispatchServer(7, identityMessage.data(),
                                           static_cast<int32_t>(identityMessage.size()), true, server) !=
            ProtocolDispatchResult::Dispatched ||
        server.identityCount != 1 || server.sender != 7 ||
        server.identity.publicKey != dispatcherPublicKey ||
        server.identity.signature != dispatcherSignature ||
        server.identity.authenticated || !server.identity.id.empty()) {
        return false;
    }

    if (ProtocolDispatcher::DispatchServer(7, commandMessage.data(),
                                           static_cast<int32_t>(commandMessage.size()), false, server) !=
            ProtocolDispatchResult::Dispatched ||
        server.commandCount != 1 || server.sender != 7 || server.command.sequence != 8) {
        return false;
    }
    if (ProtocolDispatcher::DispatchServer(
            7, weaponSelectionMessage.data(),
            static_cast<int32_t>(weaponSelectionMessage.size()), false, server) !=
            ProtocolDispatchResult::Dispatched ||
        server.weaponSelectionCount != 1 || server.sender != 7 ||
        server.weaponSelection.sequence != 12 ||
        server.weaponSelection.selectedWeapon != 2) {
        return false;
    }
    if (ProtocolDispatcher::DispatchServer(7, snapshotMessage.data(),
                                           static_cast<int32_t>(snapshotMessage.size()), false, server) !=
        ProtocolDispatchResult::Unsupported) {
        return false;
    }

    NetworkMessageRaw keyRaw;
    const std::string keyBytes(crypto_kx_PUBLICKEYBYTES, 'K');
    keyRaw.put(keyBytes.data(), static_cast<int32_t>(keyBytes.size()));
    const std::string keyMessage = BuildAppRawMessage(NAMTKeyHello, keyRaw);
    return ProtocolDispatcher::DispatchServer(9, keyMessage.data(),
                                              static_cast<int32_t>(keyMessage.size()), true, server) ==
               ProtocolDispatchResult::Dispatched &&
           server.keyCount == 1 && server.sender == 9 && server.key == keyBytes;
}

bool TestNetworkProtocolIngress() {
    using Game::Multiplayer::NetworkProtocolIngress;
    using Game::Multiplayer::ProtocolDispatchResult;

    cCryptoSession clientCrypto;
    Game::Multiplayer::ServerSessionManager sessions;
    Game::Replication::ServerReplicationCoordinator replication;
    Game::Multiplayer::SecureTransportChannel channel(clientCrypto, sessions,
                                                  replication);
    ClientDispatchCapture client;
    ServerDispatchCapture server;
    int32_t kickedPlayer = -1;
    std::string kickReason;
    NetworkProtocolIngress ingress(
        channel, sessions, client, server,
        { [&](int32_t player, const std::string& reason) {
            kickedPlayer = player;
            kickReason = reason;
        } });

    NetworkMessageRaw chatRaw;
    chatRaw.putString("ingress chat", CHAT_MAX_LINE_CHARS);
    std::string chat = BuildAppRawMessage(NAMTChat, chatRaw);
    if (ingress.ReceiveClient(chat.data(), static_cast<int32_t>(chat.size())) !=
            ProtocolDispatchResult::Dispatched ||
        client.chatCount != 1 || client.chat != "ingress chat" ||
        channel.InboundBytes() != chat.size()) {
        return false;
    }

    NetworkMessageRaw keyRaw;
    const std::string keyBytes(crypto_kx_PUBLICKEYBYTES, 'I');
    keyRaw.put(keyBytes.data(), static_cast<int32_t>(keyBytes.size()));
    std::string keyHello = BuildAppRawMessage(NAMTKeyHello, keyRaw);
    if (ingress.ReceiveServer(
            9, keyHello.data(), static_cast<int32_t>(keyHello.size())) !=
            ProtocolDispatchResult::Dispatched ||
        server.keyCount != 1 || server.sender != 9 || server.key != keyBytes ||
        kickedPlayer != -1 ||
        channel.InboundBytes() != chat.size() + keyHello.size()) {
        return false;
    }

    NetworkMessageRaw empty;
    std::string malformedHello = BuildAppRawMessage(NAMTKeyHello, empty);
    if (ingress.ReceiveServer(
            9, malformedHello.data(),
            static_cast<int32_t>(malformedHello.size())) !=
            ProtocolDispatchResult::Malformed ||
        kickedPlayer != 9 ||
        kickReason != "invalid or incompatible identity" ||
        server.keyCount != 1 ||
        channel.InboundBytes() !=
            chat.size() + keyHello.size() + malformedHello.size()) {
        return false;
    }

    const char invalidType = static_cast<char>(0xFF);
    return ingress.ReceiveClient(const_cast<char*>(&invalidType), 1) ==
               ProtocolDispatchResult::Malformed &&
           client.chatCount == 1 &&
           channel.InboundBytes() ==
               chat.size() + keyHello.size() + malformedHello.size() + 1;
}

bool TestLocalTextCommunicationService() {
    using Game::Multiplayer::LocalTextCommunicationRole;

    Game::Multiplayer::PrivateChatService alice;
    Game::Multiplayer::PrivateChatService bob;
    Game::Multiplayer::CommunicationInbox inbox;
    if (!alice.Initialize() || !bob.Initialize() ||
        !alice.SetPeer(2, "Bob", bob.PublicKey())) {
        return false;
    }

    LocalTextCommunicationRole role = LocalTextCommunicationRole::Inactive;
    bool clientDeliverySucceeds = true;
    int32_t clientSendCount = 0;
    NetAppMessageType sentType = NAMTChat;
    NetMsgFlags sentFlags = NMFNone;
    int32_t sentTarget = -1;
    std::string sentText;
    std::string sentCipher;
    int32_t hostChatCount = 0;
    int32_t hostPrivateCount = 0;
    std::string hostText;
    int32_t hostTarget = -1;

    Game::Multiplayer::LocalTextCommunicationService service(
        alice, inbox,
        {
            [&role]() { return role; },
            [&](NetAppMessageType type, const NetworkMessageRaw& raw,
                NetMsgFlags flags) {
                ++clientSendCount;
                sentType = type;
                sentFlags = flags;
                NetworkMessageRaw reader(raw.data(), raw.size());
                if (type == NAMTChat) {
                    return reader.getString(sentText, CHAT_MAX_MESSAGE_CHARS) &&
                           reader.fullyRead() && clientDeliverySucceeds;
                }
                if (type == NAMTPrivateChat) {
                    return reader.getInt32(sentTarget) &&
                           reader.getString(sentCipher, 255) &&
                           reader.fullyRead() && clientDeliverySucceeds;
                }
                return false;
            },
            [&](const std::string& text) {
                ++hostChatCount;
                hostText = text;
                return true;
            },
            [&](int32_t target, const std::string& text) {
                ++hostPrivateCount;
                hostTarget = target;
                hostText = text;
                return true;
            },
            [](int32_t player) { return "fallback-" + std::to_string(player); },
        });

    if (service.SendChat("inactive") || service.SendPrivateChat(2, "inactive") ||
        clientSendCount != 0 || inbox.ChatCount() != 0) {
        return false;
    }

    role = LocalTextCommunicationRole::Client;
    if (!service.SendChat("  hello\nworld  ") || sentType != NAMTChat ||
        sentText != "  helloworld  " || (sentFlags & NMFGuaranteed) == 0 ||
        (sentFlags & NMFHighPriority) == 0) {
        return false;
    }
    if (!service.SendPrivateChat(2, " private hello ") ||
        sentType != NAMTPrivateChat || sentTarget != 2) {
        return false;
    }
    std::string plain;
    NetworkChatLine echo;
    if (!bob.Decrypt(sentCipher, plain) || plain != " private hello " ||
        !inbox.PollChat(echo) || echo.kind != CLKPrivate ||
        echo.text != ">Bob:  private hello ") {
        return false;
    }

    clientDeliverySucceeds = false;
    if (service.SendPrivateChat(2, "not echoed") || inbox.ChatCount() != 0 ||
        service.SendPrivateChat(99, "missing key")) {
        return false;
    }
    role = LocalTextCommunicationRole::Host;
    if (!service.SendChat(" host public ") || hostChatCount != 1 ||
        hostText != " host public " ||
        !service.SendPrivateChat(7, " host private ") ||
        hostPrivateCount != 1 || hostTarget != 7 ||
        hostText != " host private " || inbox.ChatCount() != 0) {
        return false;
    }
    return !service.SendChat("\r\n");
}

bool TestCommunicationServices() {
    Game::Multiplayer::CommunicationInbox inbox;
    for (int32_t index = 0; index < 510; ++index) {
        inbox.QueueChat("line " + std::to_string(index));
    }
    NetworkChatLine line;
    if (inbox.ChatCount() != CHAT_MAX_HISTORY_LINES || !inbox.PollChat(line) ||
        line.text != "line 10") {
        return false;
    }

    NetworkVoicePacket voice{};
    voice.playerId = 7;
    voice.codec = VOICE_CODEC_OPUS;
    voice.sampleRate = VOICE_SAMPLE_RATE;
    voice.frameSamples = VOICE_SAMPLES_PER_PACKET;
    voice.data = { 1, 2, 3 };
    if (inbox.QueueVoice(voice) || inbox.AdmitVoiceIntent(7, 1) ||
        !inbox.ActivateVoicePlayer(7) || inbox.ActivateVoicePlayer(7)) {
        return false;
    }
    NetworkVoiceIntentPacket voiceIntent{};
    voiceIntent.sequence = 5;
    voiceIntent.codec = voice.codec;
    voiceIntent.sampleRate = voice.sampleRate;
    voiceIntent.frameSamples = voice.frameSamples;
    voiceIntent.data = voice.data;
    const std::string voiceIntentPayload = BuildVoiceIntentPayload(voiceIntent);
    NetworkVoiceIntentPacket decodedVoiceIntent{};
    if (!ParseVoiceIntentPacket(voiceIntentPayload.data(),
                                static_cast<int32_t>(voiceIntentPayload.size()),
                                decodedVoiceIntent) ||
        decodedVoiceIntent.sequence != voiceIntent.sequence ||
        decodedVoiceIntent.data != voiceIntent.data ||
        !Game::Multiplayer::CommunicationInbox::IsSaneVoice(decodedVoiceIntent)) {
        return false;
    }
    for (uint32_t sequence = 1; sequence <= 70; ++sequence) {
        voice.sequence = sequence;
        if (!inbox.QueueVoice(voice)) return false;
    }
    NetworkVoicePacket receivedVoice{};
    if (inbox.VoiceCount() != 64 || !inbox.PollVoice(receivedVoice) ||
        receivedVoice.sequence != 7) {
        return false;
    }
    voice.sequence = 70;
    if (inbox.QueueVoice(voice)) return false;
    voice.sequence = 69;
    if (inbox.QueueVoice(voice)) return false;
    voice.playerId = 8;
    voice.sequence = UINT32_MAX;
    if (inbox.QueueVoice(voice) || !inbox.ActivateVoicePlayer(8) ||
        !inbox.QueueVoice(voice)) return false;
    voice.sequence = 1;
    if (!inbox.QueueVoice(voice)) return false;
    if (!inbox.AdmitVoiceIntent(7, UINT32_MAX) ||
        !inbox.AdmitVoiceIntent(7, 1) || inbox.AdmitVoiceIntent(7, 1) ||
        inbox.AdmitVoiceIntent(7, UINT32_MAX)) {
        return false;
    }
    inbox.ForgetVoicePlayer(7);
    if (inbox.AdmitVoiceIntent(7, 1)) return false;
    voice.playerId = 7;
    voice.sequence = 1;
    if (inbox.QueueVoice(voice) || !inbox.ActivateVoicePlayer(7) ||
        !inbox.AdmitVoiceIntent(7, 1) || !inbox.QueueVoice(voice)) return false;
    voice.sequence = 0;
    if (inbox.QueueVoice(voice)) return false;
    voice.codec = 2;
    if (inbox.QueueVoice(voice)) return false;
    inbox.ResetVoiceSession();
    if (inbox.VoiceCount() != 0 || inbox.ChatCount() != CHAT_MAX_HISTORY_LINES - 1) {
        return false;
    }
    voice.codec = VOICE_CODEC_OPUS;
    voice.sequence = 1;
    if (inbox.QueueVoice(voice) || inbox.AdmitVoiceIntent(7, 1) ||
        !inbox.ActivateVoicePlayer(7) || !inbox.QueueVoice(voice) ||
        !inbox.AdmitVoiceIntent(7, 1)) return false;
    inbox.ClearVoice();
    if (inbox.VoiceCount() != 0 || inbox.QueueVoice(voice) ||
        inbox.AdmitVoiceIntent(7, 1)) return false;
    inbox.ResetVoiceSession();

    Game::Multiplayer::PrivateChatService alice;
    Game::Multiplayer::PrivateChatService bob;
    if (!alice.Initialize() || !bob.Initialize() ||
        !alice.SetPeer(2, "Bob", bob.PublicKey())) {
        return false;
    }
    std::string cipher;
    std::string plain;
    if (!alice.EncryptFor(2, "private hello", cipher) || !bob.Decrypt(cipher, plain) ||
        plain != "private hello") {
        return false;
    }
    cipher.back() ^= 1;
    if (bob.Decrypt(cipher, plain)) return false;
    alice.ResetPeers();
    if (alice.EncryptFor(2, "removed peer", cipher) || alice.PeerName(2) != "") {
        return false;
    }

    Game::Multiplayer::ModerationRegistry moderation("", "");
    moderation.Load();
    if (!moderation.Ban("Disk-ABC") || !moderation.IsBanned("disk-abc") ||
        moderation.Ban("DISK-ABC")) {
        return false;
    }
    std::string removed;
    if (!moderation.Unban("dIsK-aBc", &removed) || removed != "Disk-ABC" ||
        moderation.IsBanned("Disk-ABC")) {
        return false;
    }
    if (!moderation.GrantGameMaster("Owner-1") ||
        !moderation.IsGameMaster("owner-1") || moderation.GrantGameMaster("OWNER-1") ||
        !moderation.RevokeGameMaster("OwNeR-1", &removed) || removed != "Owner-1" ||
        moderation.IsGameMaster("Owner-1")) {
        return false;
    }
    return moderation.Bans().empty() && moderation.GameMasters().empty();
}

bool TestServerCommandParser() {
    using namespace Game::Multiplayer;

    const ParsedServerCommand team = ServerCommandParser::Parse("  /TEAM   Blue  ");
    if (!team.Valid() || team.kind != ServerCommandKind::Team ||
        team.access != ServerCommandAccess::Player || team.team != ServerCommandTeam::Blue) {
        return false;
    }
    const ParsedServerCommand neutral = ServerCommandParser::Parse("/team neutral");
    if (!neutral.Valid() || neutral.team != ServerCommandTeam::Neutral) return false;
    const ParsedServerCommand green = ServerCommandParser::Parse("/team green");
    if (!green.Valid() || green.team != ServerCommandTeam::Green) return false;

    const ParsedServerCommand badTeam = ServerCommandParser::Parse("/team orange");
    if (badTeam.Valid() ||
        badTeam.error != "usage: /team red|blue|green|neutral") return false;

    const ParsedServerCommand ban = ServerCommandParser::Parse("/BAN Player Seven");
    if (!ban.Valid() || ban.kind != ServerCommandKind::Ban ||
        ban.access != ServerCommandAccess::Administrator || ban.argument != "Player Seven") {
        return false;
    }
    const ParsedServerCommand missingBan = ServerCommandParser::Parse("/ban");
    if (missingBan.Valid() || missingBan.error != "usage: /ban name|identity|netId") return false;

    const ParsedServerCommand grant = ServerCommandParser::Parse("/gm identity-7");
    const ParsedServerCommand revoke = ServerCommandParser::Parse("/unadmin identity-7");
    const ParsedServerCommand list = ServerCommandParser::Parse("/gms");
    if (grant.kind != ServerCommandKind::GrantAdministrator ||
        revoke.kind != ServerCommandKind::RevokeAdministrator ||
        list.kind != ServerCommandKind::ListAdministrators ||
        grant.access != ServerCommandAccess::Administrator) {
        return false;
    }

    const ParsedServerCommand help = ServerCommandParser::Parse("/help");
    const ParsedServerCommand unknown = ServerCommandParser::Parse("/dance");
    return help.Valid() && help.kind == ServerCommandKind::Help &&
           help.access == ServerCommandAccess::Player && !unknown.Valid() &&
           unknown.kind == ServerCommandKind::Unknown && unknown.error == "unknown command: /dance";
}

bool TestServerAdministrationService() {
    using namespace Game::Multiplayer;
    using Game::Simulation::TeamId;

    ServerAdministrationService administration("", "");
    administration.Load();
    const std::vector<ServerAdministrationPlayer> players{
        { 0, "host-disk", "Host", 0 },
        { 7, "disk-alice", "Alice", 18 },
        { 8, "disk-bob", "Bob", 31 },
    };
    std::vector<std::string> results;
    std::vector<std::string> broadcasts;
    int32_t teamPlayer = -1;
    TeamId selectedTeam = TeamId::Neutral;
    int32_t disconnectedPlayer = -1;
    bool disconnectedAsBan = false;

    ServerAdministrationContext context;
    context.players = [&players]() { return players; };
    context.setTeam = [&teamPlayer, &selectedTeam](int32_t player, TeamId team) {
        teamPlayer = player;
        selectedTeam = team;
        return true;
    };
    context.sendResult = [&results](int32_t player, const std::string& message) {
        results.push_back(std::to_string(player) + ":" + message);
    };
    context.broadcastSystem = [&broadcasts](const std::string& message) {
        broadcasts.push_back(message);
    };
    context.disconnectPlayer = [&disconnectedPlayer, &disconnectedAsBan](
        int32_t player, bool ban, const std::string&) {
        disconnectedPlayer = player;
        disconnectedAsBan = ban;
    };

    administration.Execute(7, "/ban Bob", context);
    if (results != std::vector<std::string>{ "7:admin only command" } ||
        disconnectedPlayer != -1) {
        return false;
    }

    results.clear();
    administration.Execute(7, "/team green", context);
    if (teamPlayer != 7 || selectedTeam != TeamId::Green ||
        results != std::vector<std::string>{ "7:team set to green" }) {
        return false;
    }

    administration.Execute(0, "/admin ALICE", context);
    if (!administration.IsAdministrator("DISK-ALICE") || broadcasts.empty() ||
        broadcasts.back() != "Alice is now an admin") {
        return false;
    }

    results.clear();
    administration.Execute(7, "/users", context);
    if (results.size() != 4 || results[0] != "7:users online: 3" ||
        results[2] != "7:#7 Alice [disk-alice] 18 ms") {
        return false;
    }

    administration.Execute(0, "/ban DISK-BOB", context);
    if (!administration.IsBanned("disk-bob") || disconnectedPlayer != 8 ||
        !disconnectedAsBan || broadcasts.back() != "Bob was banned") {
        return false;
    }

    administration.Execute(0, "/unban disk-bob", context);
    return !administration.IsBanned("DISK-BOB") &&
           broadcasts.back() == "disk-bob was unbanned";
}

bool TestServerReplicationInterestPublisher() {
    using namespace Game::Multiplayer;

    Game::Simulation::ServerWorld world;
    Game::Replication::ServerReplicationCoordinator replication;
    ClientReplicationInbox inbox;
    ServerReplicationInterestPublisher publisher(world, replication, inbox);
    ServerReplicationEventPublisher events(world, replication, inbox, publisher);
    std::vector<std::pair<int32_t, NetAppMessageType>> deliveries;
    const ServerReplicationDelivery delivery{
        []() { return std::vector<int32_t>{ 7 }; },
        [&deliveries](int32_t observer, NetAppMessageType type,
                      const NetworkMessageRaw&, NetMsgFlags,
                      Game::Replication::ReplicationStreamKey) {
            deliveries.emplace_back(observer, type);
        },
        [](int32_t player) { return player == 7; },
    };
    publisher.SetDelivery(delivery);
    events.SetDelivery(delivery);

    const Game::Simulation::PlayerSpawn hostSpawn{
        110, { 0.0f, 0.0f, 0.0f }, 0.0f
    };
    const Game::Simulation::PlayerSpawn clientSpawn{
        110, { 100.0f, 0.0f, 0.0f }, 0.0f
    };
    if (!world.AdmitPlayer(0, hostSpawn) || !world.AdmitPlayer(7, clientSpawn)) {
        return false;
    }

    events.PublishPlayerSnapshots();
    std::vector<Game::Client::RemotePlayerPresentationState> hostViews;
    Game::Client::RemotePlayerPresentationState hostView{};
    while (inbox.Poll(hostView)) hostViews.push_back(hostView);
    const size_t lifecycleDeliveries = static_cast<size_t>(std::count_if(
        deliveries.begin(), deliveries.end(), [](const auto& delivery) {
            return delivery.second == NAMTPlayerLifecycle;
        }));
    const size_t snapshotDeliveries = static_cast<size_t>(std::count_if(
        deliveries.begin(), deliveries.end(), [](const auto& delivery) {
            return delivery.second == NAMTPlayerSnapshot;
        }));
    if (!replication.PlayerVisible(0, 7) || !replication.PlayerVisible(7, 0) ||
        !replication.PlayerVisible(0, 0) || !replication.PlayerVisible(7, 7) ||
        lifecycleDeliveries != 2 || snapshotDeliveries != 2 ||
        std::any_of(deliveries.begin(), deliveries.end(), [](const auto& delivery) {
            return delivery.first != 7;
        }) ||
        hostViews.size() != 2 ||
        std::none_of(hostViews.begin(), hostViews.end(), [](const auto& view) {
            return view.playerId == 0 && view.active;
        }) ||
        std::none_of(hostViews.begin(), hostViews.end(), [](const auto& view) {
            return view.playerId == 7 && view.active;
        })) {
        return false;
    }

    if (!world.EnsureStrategicSite(
            { 700, 110, { 300.0f, 0.0f, 0.0f }, 80.0f,
              Game::Simulation::TeamId::Red },
            Game::Simulation::StrategicSiteKind::Camp, 1).Valid() ||
        !world.EnsureStrategicSite(
            { 701, 110, { 600.0f, 0.0f, 0.0f }, 120.0f,
              Game::Simulation::TeamId::Red },
             Game::Simulation::StrategicSiteKind::Keep, 2).Valid() ||
        !world.EnsureSupplyRoute({ 702, 700, 701 }) ||
        !world.EnsureInfluenceAdjacency({ 703, 1, 2 })) {
        return false;
    }
    deliveries.clear();
    events.PublishStrategicTopology();
    Game::Client::ReplicatedStrategicTopologyState hostTopology{};
    if (deliveries != std::vector<std::pair<int32_t, NetAppMessageType>>{
            { 7, NAMTStrategicTopology }
        } ||
        !inbox.Poll(hostTopology) || hostTopology.revision != 2 ||
        hostTopology.sites.size() != 2 || hostTopology.supplyRoutes.size() != 1 ||
        hostTopology.influenceAdjacencies.size() != 1) {
        return false;
    }
    events.PublishStrategicTopology();
    if (deliveries.size() != 1 || inbox.Poll(hostTopology) ||
        !world.RemoveObjective(701)) {
        return false;
    }
    events.PublishStrategicTopology();
    if (deliveries.size() != 2 || !inbox.Poll(hostTopology) ||
        hostTopology.revision != 3 || hostTopology.sites.size() != 1 ||
        !hostTopology.supplyRoutes.empty() ||
        !hostTopology.influenceAdjacencies.empty()) {
        return false;
    }

    deliveries.clear();
    world.RemovePlayer(7);
    publisher.RefreshAll();
    Game::Client::RemotePlayerPresentationState retired{};
    return !replication.PlayerVisible(0, 7) &&
           !replication.PlayerVisible(7, 0) && deliveries.size() == 2 &&
           std::all_of(deliveries.begin(), deliveries.end(), [](const auto& delivery) {
               return delivery == std::pair<int32_t, NetAppMessageType>{
                   7, NAMTPlayerLifecycle
               };
           }) &&
           inbox.Poll(retired) && retired.playerId == 7 && !retired.active;
}

bool TestServerAuthorityScheduler() {
    using Game::Server::AuthorityPublication;
    using Game::Server::ServerAuthorityScheduler;

    Game::Simulation::ServerWorld world;
    std::vector<std::string> order;
    const auto record = [&order](const char* name) {
        return [&order, name]() { order.emplace_back(name); };
    };
    ServerAuthorityScheduler scheduler(world, AuthorityPublication{
        record("players"), record("refresh-players"), record("objectives"),
        record("structures"), record("projectiles"), record("owned"),
        record("fishing"), record("combat"), record("life"),
    });

    const auto start = Game::Simulation::ServerWorld::Clock::time_point{
        std::chrono::seconds(10)
    };
    const auto initial = scheduler.Advance(start);
    if (initial.worldSteps != 0 || initial.playerSteps != 0 ||
        order != std::vector<std::string>{ "combat", "life" }) {
        return false;
    }

    order.clear();
    const auto stepped = scheduler.Advance(start + std::chrono::milliseconds(110));
    if (stepped.worldSteps != 6 || stepped.playerSteps != 3 ||
        order != std::vector<std::string>{
            "players", "combat", "objectives", "structures",
            "projectiles", "owned", "fishing", "life"
        }) {
        return false;
    }

    // A render/transport update with no fixed world progress still drains
    // semantic result queues exactly once and cannot republish snapshots.
    order.clear();
    const auto idle = scheduler.Advance(start + std::chrono::milliseconds(110));
    if (idle.worldSteps != 0 || idle.playerSteps != 0 ||
        order != std::vector<std::string>{ "combat", "life" }) {
        return false;
    }
    scheduler.Reset();
    return true;
}

bool TestServerGameplayCommandService() {
    using namespace Game::Multiplayer;

    Game::Simulation::ServerWorld world;
    Game::Replication::ServerReplicationCoordinator replication;
    ClientReplicationInbox inbox;
    ServerReplicationInterestPublisher interest(world, replication, inbox);
    ServerReplicationEventPublisher events(world, replication, inbox, interest);
    ServerGameplayCommandService commands(world, inbox, interest, events);
    ServerGameplayPacketIngress packetIngress(commands);
    std::vector<NetAppMessageType> deliveries;
    const ServerReplicationDelivery delivery{
        []() { return std::vector<int32_t>{ 7 }; },
        [&deliveries](int32_t player, NetAppMessageType type,
                      const NetworkMessageRaw&, NetMsgFlags,
                      Game::Replication::ReplicationStreamKey) {
            if (player == 7) deliveries.push_back(type);
        },
        [](int32_t player) { return player == 7; },
    };
    interest.SetDelivery(delivery);
    events.SetDelivery(delivery);
    commands.SetDelivery(delivery);

    const Game::Simulation::PlayerSpawn spawn{
        110, { 10.0f, 0.0f, 20.0f }, 0.25f
    };
    const Game::Simulation::PlayerSpawn destination{
        73, { 1000.0f, 0.0f, 2000.0f }, 0.5f
    };
    if (!world.ConfigureSceneSpawn(spawn) ||
        !world.ConfigureSceneSpawn(destination) ||
        !world.AdmitPlayer(0, spawn) || !world.AdmitPlayer(7, spawn)) {
        Error("Gameplay command test: initial admission failed");
        return false;
    }
    interest.RefreshAll();
    Game::Client::RemotePlayerPresentationState initialLifecycle{};
    bool foundInitialClientLifecycle = false;
    while (inbox.Poll(initialLifecycle)) {
        foundInitialClientLifecycle = foundInitialClientLifecycle ||
            (initialLifecycle.active && initialLifecycle.playerId == 7);
    }
    if (!foundInitialClientLifecycle) {
        Error("Gameplay command test: initial interest missing");
        return false;
    }
    deliveries.clear();
    if (!world.AuthorizeSceneTransition(7, destination.sceneId)) {
        Error("Gameplay command test: authorized scene transition failed");
        return false;
    }
    packetIngress.EnterScene(-1, NetworkSceneEntryIntentPacket{ 1, 1 });
    if (!deliveries.empty() || world.PlayerFor(7)->sceneId != spawn.sceneId) {
        Error("Gameplay packet ingress test: invalid sender was admitted");
        return false;
    }
    packetIngress.EnterScene(7, NetworkSceneEntryIntentPacket{ 1, 1 });
    if (deliveries.size() != 4 ||
        deliveries.front() != NAMTSceneEntryState ||
        !std::all_of(deliveries.begin() + 1, deliveries.end(),
                     [](NetAppMessageType type) {
                         return type == NAMTPlayerLifecycle;
                     })) {
        Error("Gameplay command test: delivery order count=%zu first=%u last=%u",
              deliveries.size(),
              deliveries.empty() ? 0U : static_cast<unsigned>(deliveries.front()),
              deliveries.empty() ? 0U : static_cast<unsigned>(deliveries.back()));
        return false;
    }
    Game::Client::RemotePlayerPresentationState lifecycle{};
    if (!inbox.Poll(lifecycle) || lifecycle.playerId != 7 || lifecycle.active) {
        Error("Gameplay command test: host lifecycle missing");
        return false;
    }

    // The authenticated sender replaces a spoofed packet player ID before
    // ingress. Selection mutates player 7 only.
    if (!commands.SelectWeapon(7, { 999, 1, 1, 2 })) {
        Error("Gameplay command test: sender-bound weapon selection rejected");
        return false;
    }
    const auto player = world.PlayerFor(7);
    if (!player || player->selectedWeapon != 2) {
        Error("Gameplay command test: weapon mutation missing");
        return false;
    }

    packetIngress.SelectWeapon(
        -1, NetworkWeaponSelectionIntentPacket{ 2, 1, 3 });
    packetIngress.SelectWeapon(
        7, NetworkWeaponSelectionIntentPacket{ 2, 1, 0xFF });
    if (world.PlayerFor(7)->selectedWeapon != 2) {
        Error("Gameplay packet ingress test: invalid weapon packet mutated state");
        return false;
    }
    packetIngress.SelectWeapon(
        7, NetworkWeaponSelectionIntentPacket{ 2, 1, 3 });
    if (world.PlayerFor(7)->selectedWeapon != 3) {
        Error("Gameplay packet ingress test: valid weapon packet was not admitted");
        return false;
    }

    const size_t beforeRejectedEntry = deliveries.size();
    if (commands.ExecuteSceneEntry(7, { 7, 2, 1 }) ||
        deliveries.size() != beforeRejectedEntry + 1 ||
        deliveries.back() != NAMTSceneEntryState) {
        Error("Gameplay command test: rejected scene reply mismatch");
        return false;
    }
    return true;
}

bool TestServerPlayerSessionService() {
    using namespace Game::Multiplayer;

    ServerSessionManager sessions;
    ServerAdministrationService administration("", "");
    administration.Load();
    Game::Simulation::ServerWorld world;
    Game::Replication::ServerReplicationCoordinator replication;
    ClientReplicationInbox inbox;
    PrivateChatService privateChat;
    CommunicationInbox communication;
    ServerReplicationInterestPublisher interest(world, replication, inbox);
    ServerReplicationEventPublisher events(world, replication, inbox, interest);
    ServerGameplayCommandService commands(world, inbox, interest, events);
    ServerPlayerSessionService playerSessions(
        sessions, administration, world, replication, inbox, privateChat,
        communication, commands, interest, events);

    std::vector<NetAppMessageType> directMessages;
    std::vector<NetAppMessageType> broadcasts;
    int32_t kickedPlayer = -1;
    std::string kickReason;
    int32_t knownKeyRequests = 0;
    const ServerReplicationDelivery delivery{
        [&sessions]() { return sessions.AdmittedPeers(); },
        [&directMessages](int32_t, NetAppMessageType type,
                          const NetworkMessageRaw&, NetMsgFlags,
                          Game::Replication::ReplicationStreamKey) {
            directMessages.push_back(type);
        },
        [&sessions](int32_t player) { return sessions.HasIdentity(player); },
    };
    interest.SetDelivery(delivery);
    events.SetDelivery(delivery);
    commands.SetDelivery(delivery);
    playerSessions.SetTransport({
        [&directMessages](int32_t, NetAppMessageType type,
                          const NetworkMessageRaw&, NetMsgFlags) {
            directMessages.push_back(type);
        },
        [&broadcasts](NetAppMessageType type, const NetworkMessageRaw&) {
            broadcasts.push_back(type);
        },
        [&kickedPlayer, &kickReason](int32_t player, bool,
                                    const std::string& reason) {
            kickedPlayer = player;
            kickReason = reason;
        },
        [&knownKeyRequests](int32_t) { ++knownKeyRequests; },
    });

    const Game::Simulation::PlayerSpawn spawn{
        110, { 0.0f, 0.0f, 0.0f }, 0.0f
    };
    if (!world.ConfigureSceneSpawn(spawn) || !world.AdmitPlayer(0, spawn) ||
        !playerSessions.ConnectPeer(7) ||
        !playerSessions.AdmitIdentity(
            7, AuthenticatedTestIdentity("identity-seven", "Seven"))) {
        return false;
    }
    if (!sessions.HasIdentity(7) || !world.PlayerFor(7) ||
        directMessages.size() < 5 ||
        directMessages[0] != NAMTPlayerAssign ||
        directMessages[1] != NAMTPlayerLifecycle ||
        directMessages[2] != NAMTPlayerLifecycle ||
        directMessages[3] != NAMTSceneEntryState ||
        directMessages[4] != NAMTStrategicTopology ||
        knownKeyRequests != 1 || !broadcasts.empty()) {
        return false;
    }
    NetworkChatLine chat{};
    Game::Client::RemotePlayerPresentationState lifecycle{};
    bool foundHostLifecycle = false;
    bool foundClientLifecycle = false;
    while (inbox.Poll(lifecycle)) {
        foundHostLifecycle = foundHostLifecycle ||
            (lifecycle.playerId == 0 && lifecycle.active);
        foundClientLifecycle = foundClientLifecycle ||
            (lifecycle.playerId == 7 && lifecycle.active);
    }
    if (communication.PollChat(chat) || !foundHostLifecycle ||
        !foundClientLifecycle) {
        return false;
    }

    if (!playerSessions.DisconnectPeer(7) || sessions.HasPeer(7) ||
        world.PlayerFor(7) || !inbox.Poll(lifecycle) || lifecycle.active ||
        communication.PollChat(chat) ||
        playerSessions.DisconnectPeer(7)) {
        return false;
    }

    // If identity admission fails after the world entity was tentatively
    // created, the entity is rolled back and the unauthenticated session stays
    // isolated until transport disconnects it.
    if (!playerSessions.ConnectPeer(8) ||
        playerSessions.AdmitIdentity(8, NetworkIdentity{}) ||
        world.PlayerFor(8) || sessions.HasIdentity(8) || kickedPlayer != 8 ||
        kickReason != "identity admission failed") {
        return false;
    }
    return true;
}

bool TestSecureTransportChannel() {
    using namespace Game::Multiplayer;

    cCryptoSession clientCrypto;
    ServerSessionManager sessions;
    Game::Replication::ServerReplicationCoordinator replication;
    if (!sessions.ConnectPeer(7) ||
        !sessions.AdmitIdentity(
            7, AuthenticatedTestIdentity("identity-seven", "Seven"))) {
        return false;
    }
    std::string clientHello;
    std::string serverReply;
    cCryptoSession* serverCrypto = sessions.CryptoFor(7);
    if (!serverCrypto || !clientCrypto.buildClientHello(clientHello) ||
        !serverCrypto->acceptClientHello(clientHello, serverReply) ||
        !clientCrypto.acceptServerKey(serverReply)) {
        return false;
    }

    SecureTransportChannel channel(clientCrypto, sessions, replication);
    std::string rawToServer;
    std::string rawToClient;
    channel.SetDelivery({
        [&rawToServer](const std::string& payload, NetMsgFlags) {
            rawToServer = payload;
            return true;
        },
        [&rawToClient](int32_t peer, const std::string& payload, NetMsgFlags) {
            if (peer != 7) return false;
            rawToClient = payload;
            return true;
        },
    });

    NetworkMessageRaw raw;
    raw.putString("secure hello", CHAT_MAX_MESSAGE_CHARS);
    const NetMsgFlags reliable =
        static_cast<NetMsgFlags>(NMFGuaranteed | NMFHighPriority);
    if (!channel.SendToServer(NAMTChat, raw, reliable) || rawToServer.empty()) {
        return false;
    }
    std::string decrypted;
    const char* message = nullptr;
    int32_t messageSize = 0;
    if (!channel.PrepareServerMessage(
            7, rawToServer.data(), static_cast<int32_t>(rawToServer.size()),
            decrypted, message, messageSize) ||
        messageSize < static_cast<int32_t>(sizeof(NetAppMessageHeader)) ||
        reinterpret_cast<const NetAppMessageHeader*>(message)->type != NAMTChat) {
        return false;
    }
    channel.RecordInbound(rawToServer.size());

    if (!channel.SendToPeer(7, NAMTChat, raw, reliable) || rawToClient.empty() ||
        !channel.PrepareClientMessage(
            rawToClient.data(), static_cast<int32_t>(rawToClient.size()),
            decrypted, message, messageSize) ||
        reinterpret_cast<const NetAppMessageHeader*>(message)->type != NAMTChat ||
        channel.InboundBytes() != rawToServer.size() ||
        channel.OutboundBytes() != rawToServer.size() + rawToClient.size()) {
        return false;
    }

    const std::string plainChat = BuildAppRawMessage(NAMTChat, raw);
    NetworkMessageRaw empty;
    const std::string plainKeyAccept = BuildAppRawMessage(NAMTKeyAccept, empty);
    if (channel.PrepareServerMessage(
            7, const_cast<char*>(plainChat.data()),
            static_cast<int32_t>(plainChat.size()), decrypted, message, messageSize) ||
        channel.PrepareClientMessage(
            const_cast<char*>(plainChat.data()),
            static_cast<int32_t>(plainChat.size()), decrypted, message, messageSize) ||
        !channel.PrepareClientMessage(
            const_cast<char*>(plainKeyAccept.data()),
            static_cast<int32_t>(plainKeyAccept.size()), decrypted, message, messageSize)) {
        return false;
    }

    channel.ResetCounters();
    channel.SetDelivery({
        [](const std::string&, NetMsgFlags) { return false; },
        [](int32_t, const std::string&, NetMsgFlags) { return false; },
    });
    return !channel.SendPlainToServer(NAMTKeyHello, empty) &&
           channel.InboundBytes() == 0 && channel.OutboundBytes() == 0;
}

bool TestServerCommunicationService() {
    using namespace Game::Multiplayer;

    ServerSessionManager sessions;
    ServerAdministrationService administration("", "");
    PrivateChatService privateChat;
    CommunicationInbox communication;
    Game::Simulation::ServerWorld world;
    Game::Replication::ServerReplicationCoordinator replication;
    ServerCommunicationService service(
        sessions, administration, privateChat, communication, world,
        replication);
    administration.Load();
    if (!privateChat.Initialize()) return false;

    const Game::Simulation::PlayerSpawn spawn{
        110, { 0.0f, 0.0f, 0.0f }, 0.0f
    };
    if (!world.ConfigureSceneSpawn(spawn) || !world.AdmitPlayer(0, spawn) ||
        !sessions.ConnectPeer(7) || !sessions.AdmitIdentity(
            7, AuthenticatedTestIdentity("identity-alice", "Alice")) ||
        !sessions.ConnectPeer(8) || !sessions.AdmitIdentity(
            8, AuthenticatedTestIdentity("identity-bob", "Bob")) ||
        !world.AdmitPlayer(7, spawn) || !world.AdmitPlayer(8, spawn)) {
        return false;
    }
    replication.ReconcilePlayers(world.PlayerSnapshots(), { 0, 7, 8 },
                                 6000.0f);
    communication.ActivateVoicePlayer(7);
    communication.ActivateVoicePlayer(8);

    std::vector<std::pair<int32_t, NetAppMessageType>> direct;
    std::vector<NetAppMessageType> broadcasts;
    std::vector<int32_t> voiceObservers;
    int32_t disconnected = -1;
    service.SetDelivery({
        [&direct](int32_t peer, NetAppMessageType type,
                  const NetworkMessageRaw&, NetMsgFlags) {
            direct.emplace_back(peer, type);
            return true;
        },
        [&broadcasts](NetAppMessageType type, const NetworkMessageRaw&) {
            broadcasts.push_back(type);
        },
        [&voiceObservers](int32_t peer, const std::string& payload) {
            if (payload.empty()) return false;
            voiceObservers.push_back(peer);
            return true;
        },
        []() {
            return std::vector<ServerAdministrationPlayer>{
                { 0, "host-disk", "Host", 0 },
                { 7, "disk-alice", "Alice", 18 },
                { 8, "disk-bob", "Bob", 31 },
            };
        },
        [&disconnected](int32_t player, bool, const std::string&) {
            disconnected = player;
        },
    });

    service.HandleChat(99, "unauthenticated");
    service.HandleChat(7, "hello world");
    NetworkChatLine line{};
    if (broadcasts != std::vector<NetAppMessageType>{ NAMTChat } ||
        !communication.PollChat(line) || line.text != "Alice: hello world") {
        return false;
    }

    service.HandleChat(7, "/team blue");
    const auto aliceState = world.PlayerFor(7);
    if (!aliceState || aliceState->team != Game::Simulation::TeamId::Blue ||
        direct.empty() || direct.back() !=
            std::pair<int32_t, NetAppMessageType>{ 7, NAMTChat }) {
        return false;
    }

    PrivateChatService alice;
    PrivateChatService bob;
    if (!alice.Initialize() || !bob.Initialize()) return false;
    service.HandleChatKey(7, { 0, "Wrong", alice.PublicKey() });
    const size_t beforeKeys = direct.size();
    service.HandleChatKey(7, { 0, "Alice", alice.PublicKey() });
    service.HandleChatKey(8, { 0, "Bob", bob.PublicKey() });
    if (direct.size() != beforeKeys + 4 ||
        !service.SendHostPrivateChat(8, "host secret") ||
        direct.back() !=
            std::pair<int32_t, NetAppMessageType>{ 8, NAMTPrivateChat }) {
        return false;
    }
    if (!communication.PollChat(line) ||
        line.text != ">Bob: host secret" || line.kind != CLKPrivate) {
        return false;
    }

    if (!alice.SetPeer(0, "system", privateChat.PublicKey())) return false;
    std::string cipher;
    if (!alice.EncryptFor(0, "alice secret", cipher)) return false;
    service.HandlePrivateChat(7, { 0, cipher });
    if (!communication.PollChat(line) ||
        line.text != "(private) Alice: alice secret" ||
        line.kind != CLKPrivate) {
        return false;
    }

    NetworkVoiceIntentPacket voice{};
    voice.sequence = 1;
    voice.codec = VOICE_CODEC_OPUS;
    voice.sampleRate = VOICE_SAMPLE_RATE;
    voice.frameSamples = VOICE_SAMPLES_PER_PACKET;
    voice.data = { 1, 2, 3 };
    service.HandleVoice(7, voice);
    service.HandleVoice(7, voice);
    NetworkVoicePacket received{};
    if (voiceObservers != std::vector<int32_t>{ 8 } ||
        !communication.PollVoice(received) || received.playerId != 7 ||
        received.sequence != 1 || communication.PollVoice(received) ||
        disconnected != -1) {
        return false;
    }

    const size_t beforeKnownKeys = direct.size();
    service.SendKnownChatKeys(99);
    service.SendKnownChatKeys(8);
    return direct.size() == beforeKnownKeys + privateChat.Peers().size();
}

bool TestClientSessionIngress() {
    using namespace Game::Multiplayer;

    ClientReplicationInbox replication;
    CommunicationInbox communication;
    PrivateChatService privateChat;
    if (!privateChat.Initialize()) return false;
    ClientSessionIngress ingress(replication, communication, privateChat);

    int identityActions = 0;
    cCryptoSession endpointCrypto;
    ClientProtocolEndpoint endpoint(
        ingress, endpointCrypto,
        { [&identityActions]() { ++identityActions; },
          [&identityActions]() { ++identityActions; } });
    endpoint.OnClientPlayerAssign({ 9 });
    endpoint.OnClientChat("endpoint admitted chat");
    NetworkChatLine endpointLine;
    if (identityActions != 0 || ingress.LocalPlayerId() != 9 ||
        !communication.PollChat(endpointLine) ||
        endpointLine.text != "endpoint admitted chat") {
        return false;
    }
    ingress.Reset();

    if (ingress.AssignPlayer({ 0 }) || !ingress.AssignPlayer({ 7 }) ||
        !ingress.AssignPlayer({ 7 }) || ingress.AssignPlayer({ 8 }) ||
        ingress.LocalPlayerId() != 7 || ingress.LocalLifeEpoch() != 0) {
        return false;
    }

    const NetworkPlayerLifecyclePacket localLifetime{ 7, 10, 1, 1, 118, 1 };
    const NetworkPlayerLifecyclePacket remoteLifetime{ 8, 11, 1, 1, 118, 1 };
    if (!ingress.AcceptPlayerLifecycle(localLifetime) ||
        !ingress.AcceptPlayerLifecycle(remoteLifetime)) {
        return false;
    }

    Game::Simulation::PlayerSnapshot player{};
    player.entity = { 10, 1 };
    player.ownerPlayerId = 7;
    player.sceneId = 118;
    player.serverTick = 20;
    player.lifeEpoch = 1;
    player.health = 48;
    const NetworkPlayerSnapshotPacket snapshot =
        PlayerSimulationNetworkAdapter::ToPacket(player);
    if (!ingress.AcceptPlayerSnapshot(snapshot) ||
        ingress.LocalLifeEpoch() != 1) {
        return false;
    }

    const NetworkPlayerRespawnPacket wrongPlayerRespawn{
        8, 11, 1, 2, 118, 21, 0.0f, 0.0f, 0.0f, 0, 1
    };
    const NetworkPlayerRespawnPacket localRespawn{
        7, 10, 1, 2, 118, 21, 0.0f, 0.0f, 0.0f, 0, 1
    };
    if (ingress.AcceptPlayerRespawn(wrongPlayerRespawn) ||
        !ingress.AcceptPlayerRespawn(localRespawn) ||
        ingress.LocalLifeEpoch() != 2) {
        return false;
    }

    if (!ingress.AcceptChat("system: ready\x01") ||
        ingress.AcceptChat("\x01\x02")) {
        return false;
    }
    NetworkChatLine chat{};
    if (!communication.PollChat(chat) || chat.text != "system: ready" ||
        chat.kind != CLKSystem) {
        return false;
    }

    PrivateChatService sender;
    if (!sender.Initialize() ||
        !sender.SetPeer(0, "system", privateChat.PublicKey())) {
        return false;
    }
    std::string cipher;
    if (!sender.EncryptFor(0, "private ingress", cipher) ||
        !ingress.AcceptPrivateChat({ 8, "Bob\x01", cipher })) {
        return false;
    }
    if (!communication.PollChat(chat) ||
        chat.text != "(private) Bob: private ingress" ||
        chat.kind != CLKPrivate) {
        return false;
    }

    if (!ingress.AcceptChatKey({ 8, "Bob", sender.PublicKey() }) ||
        privateChat.PeerName(8) != "Bob") {
        return false;
    }

    NetworkVoicePacket voice{};
    voice.playerId = 8;
    voice.sequence = 1;
    voice.codec = VOICE_CODEC_OPUS;
    voice.sampleRate = VOICE_SAMPLE_RATE;
    voice.frameSamples = VOICE_SAMPLES_PER_PACKET;
    voice.data = { 1, 2, 3 };
    if (!ingress.AcceptVoice(voice)) return false;
    voice.playerId = 7;
    if (ingress.AcceptVoice(voice)) return false;

    ingress.Reset();
    Game::Simulation::PlayerSnapshot queuedSnapshot{};
    NetworkVoicePacket queuedVoice{};
    if (ingress.LocalPlayerId() != -1 || ingress.LocalLifeEpoch() != 0 ||
        replication.Poll(queuedSnapshot) || communication.PollVoice(queuedVoice) ||
        privateChat.Peers().size() != 1 || !privateChat.PeerName(8).empty()) {
        return false;
    }
    voice.playerId = 8;
    voice.sequence = 1;
    return !ingress.AcceptVoice(voice);
}

} // namespace

int main() {
    ClearLog();
    if (sodium_init() < 0) {
        Error("Network protocol self-test: libsodium initialization failed");
        return 1;
    }
    if (!TestSessionEncryption()) {
        Error("Network protocol self-test: encrypted session failed");
        return 2;
    }
    if (!TestServerSessionManager()) {
        Error("Network protocol self-test: server session lifecycle failed");
        return 11;
    }
    if (!TestPrivateChatEncryption()) {
        Error("Network protocol self-test: private chat sealed box failed");
        return 3;
    }
    if (!TestPacketSerialization()) {
        Error("Network protocol self-test: packet serialization failed");
        return 4;
    }
    if (!TestProjectileNetworkAdapter()) {
        Error("Network protocol self-test: projectile network adapter failed");
        return 5;
    }
    if (!TestClientProjectilePresentationPolicy()) {
        Error("Network protocol self-test: client projectile presentation lifetime failed");
        return 16;
    }
    if (!TestClientCombatPresentationPolicy()) {
        Error("Network protocol self-test: client combat presentation policy failed");
        return 17;
    }
    if (!TestClientPlayerActionPresentationPolicy()) {
        Error("Network protocol self-test: authoritative player action presentation failed");
        return 18;
    }
    if (!TestFishingNetworkAdapter()) {
        Error("Network protocol self-test: fishing network adapter failed");
        return 6;
    }
    if (!TestWorldPvpNetworkAdapter()) {
        Error("Network protocol self-test: world PvP network adapter failed");
        return 7;
    }
    if (!TestLocalVoiceFrameStream()) {
        Error("Network protocol self-test: local voice frame ownership failed");
        return 19;
    }
    if (!TestLocalVoiceSubmissionService()) {
        Error("Network protocol self-test: local voice routing ownership failed");
        return 20;
    }
    if (!TestLocalClientAdmissionService()) {
        Error("Network protocol self-test: local client admission ownership failed");
        return 22;
    }
    if (!TestLocalTextCommunicationService()) {
        Error("Network protocol self-test: local text routing ownership failed");
        return 21;
    }
    if (!TestLifecycleAndCorpseAdapters()) {
        Error("Network protocol self-test: presentation/corpse adapter failed");
        return 9;
    }
    if (!TestPlayerSimulationNetworkAdapter()) {
        Error("Network protocol self-test: player simulation adapter failed");
        return 10;
    }
    if (!TestLocalPlayerCommandStream()) {
        Error("Network protocol self-test: local command stream ownership failed");
        return 17;
    }
    if (!TestClientReplicationInbox()) {
        Error("Network protocol self-test: client replication inbox failed");
        return 12;
    }
    if (!TestProtocolDispatcher()) {
        Error("Network protocol self-test: typed protocol dispatcher failed");
        return 13;
    }
    if (!TestNetworkProtocolIngress()) {
        Error("Network protocol self-test: secure protocol ingress failed");
        return 23;
    }
    if (!TestCommunicationServices()) {
        Error("Network protocol self-test: communication service ownership failed");
        return 14;
    }
    if (!TestServerCommandParser()) {
        Error("Network protocol self-test: typed server command policy failed");
        return 15;
    }
    if (!TestServerAdministrationService()) {
        Error("Network protocol self-test: server administration ownership failed");
        return 20;
    }
    if (!TestServerReplicationInterestPublisher()) {
        Error("Network protocol self-test: replication interest publication failed");
        return 21;
    }
    if (!TestServerAuthorityScheduler()) {
        Error("Network protocol self-test: fixed authority scheduling failed");
        return 22;
    }
    if (!TestServerGameplayCommandService()) {
        Error("Network protocol self-test: authenticated gameplay commands failed");
        return 23;
    }
    if (!TestServerPlayerSessionService()) {
        Error("Network protocol self-test: player session lifecycle failed");
        return 24;
    }
    if (!TestSecureTransportChannel()) {
        Error("Network protocol self-test: secure transport channel failed");
        return 25;
    }
    if (!TestServerCommunicationService()) {
        Error("Network protocol self-test: server communication routing failed");
        return 26;
    }
    if (!TestClientSessionIngress()) {
        Error("Network protocol self-test: client session ingress failed");
        return 27;
    }

    Error("Network protocol self-test passed: typed dispatch, communication ownership, and lifetime fencing");
    return 0;
}
