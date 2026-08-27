#include <sysdef.h>

#include "Network/ShipwrightNetworkRuntime.h"

#include <Windows.h>

#include <cstring>
#include <string>

namespace {

using SoH::Network::ShipwrightNetworkRuntime;

constexpr unsigned short kRuntimePort = 47778;

uint32_t FishingIdentityHash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    return value ^ (value >> 16);
}

float TestPondFishLength() {
    const uint32_t seed = 0x49U * 0x9E3779B9U ^ 100U * 0x85EBCA6BU ^
                          666U * 0xC2B2AE35U ^ 354U * 0x27D4EB2FU;
    return 38.0f + static_cast<float>(FishingIdentityHash(seed) & 0x00FFFFFF) / 16777216.0f * 4.99999f;
}

NetworkPlayerStatePacket MakeState(int value) {
    NetworkPlayerStatePacket packet{};
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
    packet.stateFlags = NETWORK_PLAYER_VISIBLE | NETWORK_PLAYER_GROUNDED;
    packet.modelGroup = 2;
    packet.itemAction = 3;
    for (int limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb) {
        packet.jointTable[limb][0] = static_cast<short>(value + limb);
    }
    return packet;
}

NetworkActorEventPacket MakeFishEvent(int value, unsigned char eventType) {
    NetworkActorEventPacket packet{};
    packet.eventId = value + eventType;
    packet.sceneId = 0x49;
    packet.roomId = 3;
    packet.actorId = 0xFE;
    // Pond fish remain valid without a scene-water registry; wild params
    // 400/401 are separately covered by ServerCollisionSelfTest and must use
    // the exact server-derived water-box spawn key.
    packet.actorParams = 100;
    packet.homeX = 666;
    packet.homeY = -45;
    packet.homeZ = 354;
    packet.x = 666.0f;
    packet.y = -45.0f;
    packet.z = 354.0f;
    packet.eventType = eventType;
    return packet;
}

NetworkVoicePacket MakeVoice(int value) {
    NetworkVoicePacket packet;
    packet.sequence = static_cast<unsigned __int32>(value);
    packet.codec = VOICE_CODEC_ADPCM;
    packet.sampleRate = VOICE_SAMPLE_RATE;
    packet.frameSamples = VOICE_SAMPLES_PER_PACKET;
    packet.data.assign(4 + VOICE_SAMPLES_PER_PACKET / 2, static_cast<unsigned char>(value));
    return packet;
}

NetworkActorEventPacket MakeActorEvent(int value) {
    NetworkActorEventPacket packet{};
    packet.eventId = value;
    packet.sceneId = 110;
    packet.roomId = 3;
    packet.actorId = 0x14D;
    packet.actorParams = -17;
    packet.homeX = value;
    packet.homeY = value * 2;
    packet.homeZ = value * 3;
    packet.x = (float)value;
    packet.y = (float)(value * 2);
    packet.z = (float)(value * 3);
    packet.eventType = NETWORK_ACTOR_EVENT_OWL_DEPART;
    return packet;
}

NetworkActorEventPacket MakeGrassEvent(int value) {
    NetworkActorEventPacket packet{};
    packet.eventId = value + 7000;
    packet.sceneId = 110;
    packet.roomId = 3;
    packet.actorId = 0x125;
    packet.actorParams = 1;
    packet.homeX = value;
    packet.homeY = value * 2;
    packet.homeZ = value * 3;
    // A carried grass actor can break away from its placed home. The server
    // accepts this semantic event only while its current position remains near
    // the authoritative player; weapon cuts use separate weapon evidence.
    packet.x = 0.0f;
    packet.y = 20.0f;
    packet.z = 20.0f;
    packet.eventType = NETWORK_ACTOR_EVENT_GRASS_THROWN_BREAK;
    return packet;
}

NetworkActorEventPacket MakeUnauthorizedGrassCut(int value) {
    NetworkActorEventPacket packet{};
    packet.eventId = value + 8000;
    packet.sceneId = 110;
    packet.roomId = 3;
    packet.actorId = 0x125;
    packet.actorParams = 0;
    packet.homeX = 300;
    packet.homeY = 0;
    packet.homeZ = 0;
    packet.x = 300.0f;
    packet.y = 0.0f;
    packet.z = 0.0f;
    packet.eventType = NETWORK_ACTOR_EVENT_GRASS_CUT;
    return packet;
}

NetworkActorEventPacket MakeBoulderEvent(int value) {
    NetworkActorEventPacket packet{};
    packet.eventId = value + 9000;
    packet.sceneId = 110;
    packet.roomId = 3;
    packet.actorId = 0x127;
    packet.actorParams = 0;
    packet.homeX = 20;
    packet.homeY = 0;
    packet.homeZ = 0;
    packet.x = 20.0f;
    packet.y = 0.0f;
    packet.z = 0.0f;
    packet.eventType = NETWORK_ACTOR_EVENT_BOULDER_BREAK;
    return packet;
}

int RunHost() {
    ShipwrightNetworkRuntime network;
    if (!network.Host(kRuntimePort, "Shipwright secure runtime test")) {
        return 10;
    }

    bool chatReceived = false;
    bool privateReceived = false;
    bool stateReceived = false;
    bool bowStringScaleReceived = false;
    bool voiceReceived = false;
    bool actorEventReceived = false;
    bool fishHookReceived = false;
    bool fishHookAcknowledged = false;
    bool fishCanonicalStateReceived = false;
    bool fishCanonicalAcknowledged = false;
    bool fishReleaseReceived = false;
    bool projectileImpactSent = false;
    bool offAxisProjectileImpactSent = false;
    bool offAxisProjectileImpactRejected = false;
    bool projectileImpactWitnessed = false;
    bool projectileImpactAcknowledged = false;
    bool projectileRetiredUnexpectedly = false;
    bool bombHeldOriginReceived = false;
    bool bombHeldAcknowledged = false;
    bool bombReleasedReceived = false;
    bool bombReleasedAcknowledged = false;
    bool boulderBreakReceived = false;
    bool boulderBreakAcknowledged = false;
    bool arrowNativeDisplayPitchReceived = false;
    bool arrowAimedDisplayPitchReceived = false;
    bool arrowStuckDisplayPitchReceived = false;
    bool arrowDamageReceived = false;
    bool arrowDamageAcknowledged = false;
    bool clientMeleeDamageReceived = false;
    bool clientMeleeDamageAcknowledged = false;
    bool hostMeleeSent = false;
    bool clientSawHostMeleeDamage = false;
    bool witnessArrowReady = false;
    bool grassCutReceived = false;
    bool unauthorizedGrassCutAccepted = false;
    bool grassRestored = false;
    bool grassRestoreAcknowledged = false;
    bool corpseReceived = false;
    bool responseSent = false;
    bool clientComplete = false;
    unsigned __int64 projectileStuckAt = 0;

    const unsigned __int64 timeout = GetTickCount64() + 25000;
    while (GetTickCount64() < timeout && !clientComplete) {
        network.Update();

        NetworkChatLine line;
        while (network.PollChat(line)) {
            chatReceived = chatReceived || line.text.find("runtime-client-chat") != std::string::npos;
            privateReceived = privateReceived || line.text.find("runtime-client-private") != std::string::npos;
            clientComplete = clientComplete || line.text.find("runtime-client-complete") != std::string::npos;
            clientSawHostMeleeDamage = clientSawHostMeleeDamage ||
                                       line.text.find("runtime-host-melee-damage") != std::string::npos;
        }

        NetworkPlayerStatePacket state{};
        while (network.PollPlayerState(state)) {
            stateReceived = stateReceived || (state.playerId > 0 && state.sceneId == 0x49 &&
                                               state.x == 666.0f && state.y == -87.0f && state.z == 354.0f &&
                                               state.fishingLineHooked == 1 &&
                                               state.fishingLureDrawOffset[0] == 17.25f &&
                                               state.fishingLureSpin == 0.375f &&
                                               state.fishingLureZOffset == -725.0f &&
                                               state.fishingSinkingLureUnderwater == 1 &&
                                               state.fishingLineGravity == 2.25f);
            fishCanonicalStateReceived = fishCanonicalStateReceived ||
                                         (state.playerId > 0 && state.sceneId == 0x49 &&
                                          state.fishingFishActive == 1 && state.fishingFishIsLoach == 0 &&
                                          std::fabs(state.fishingFishLength - TestPondFishLength()) < 0.001f);
            bowStringScaleReceived = bowStringScaleReceived ||
                                     (state.playerId > 0 && state.itemAction == 8 &&
                                      state.bowStringScale == 0.625f);
            corpseReceived = corpseReceived ||
                             (state.playerId < -1 && (state.stateFlags & NETWORK_PLAYER_DEAD) != 0 &&
                              state.jointTable[0][0] == 1234);
        }
        NetworkVoicePacket voice;
        while (network.PollVoice(voice)) {
            voiceReceived = voice.playerId > 0 && voice.sequence == 444 && !voice.data.empty();
        }
        NetworkActorEventPacket actorEvent{};
        while (network.PollActorEvent(actorEvent)) {
            actorEventReceived = actorEventReceived ||
                                 (actorEvent.sourcePlayerId > 0 && actorEvent.eventId == 111);
            fishHookReceived = fishHookReceived ||
                               (actorEvent.sourcePlayerId > 0 &&
                                actorEvent.eventType == NETWORK_ACTOR_EVENT_FISH_HOOK);
            fishReleaseReceived = fishReleaseReceived ||
                                  (actorEvent.sourcePlayerId > 0 &&
                                   actorEvent.eventType == NETWORK_ACTOR_EVENT_FISH_RELEASE);
            grassCutReceived = grassCutReceived ||
                               (actorEvent.sourcePlayerId > 0 &&
                                actorEvent.eventType == NETWORK_ACTOR_EVENT_GRASS_THROWN_BREAK);
            unauthorizedGrassCutAccepted = unauthorizedGrassCutAccepted ||
                                           (actorEvent.sourcePlayerId > 0 &&
                                            actorEvent.eventType == NETWORK_ACTOR_EVENT_GRASS_CUT);
            boulderBreakReceived = boulderBreakReceived ||
                                   (actorEvent.sourcePlayerId > 0 &&
                                    actorEvent.eventType == NETWORK_ACTOR_EVENT_BOULDER_BREAK);
        }
        NetworkDynamicObjectStatePacket objectState{};
        while (network.PollDynamicObjectState(objectState)) {
            if (objectState.actorId == 0x125 && objectState.actorParams == 1) {
                grassRestored = grassRestored || objectState.destroyed == 0;
            }
        }
        NetworkProjectileStatePacket projectile{};
        while (network.PollProjectileState(projectile)) {
            // Native EnArrow keeps launch aim in world.rot.x, but derives the
            // rendered model pitch in shape.rot.x. Math_Atan2S(x, y) computes
            // atan2(y, x), so a level shot must be displayed at zero. The
            // packet deliberately requests a different yaw; authority must
            // keep the accepted Link aim of zero.
            arrowNativeDisplayPitchReceived = arrowNativeDisplayPitchReceived ||
                                              (projectile.playerId > 0 &&
                                               projectile.projectileKind == NETWORK_PROJECTILE_ARROW &&
                                               projectile.phase == NETWORK_ARROW_FLYING &&
                                               projectile.rotationX == 0 && projectile.rotationY == 0);
            arrowAimedDisplayPitchReceived = arrowAimedDisplayPitchReceived ||
                                             (projectile.playerId > 0 &&
                                              projectile.projectileKind == NETWORK_PROJECTILE_ARROW &&
                                              projectile.phase == NETWORK_ARROW_FLYING &&
                                              std::abs(static_cast<int>(static_cast<short>(
                                                  projectile.rotationX - 0x1000))) <= 2 &&
                                              projectile.rotationY == 0x4000);
            if (projectile.playerId > 0 && projectile.projectileKind == NETWORK_PROJECTILE_ARROW &&
                projectile.phase == NETWORK_ARROW_FLYING && witnessArrowReady && !offAxisProjectileImpactSent &&
                std::abs(static_cast<int>(static_cast<short>(projectile.rotationY - 0x4000))) < 0x1000) {
                NetworkProjectileImpactPacket offAxisImpact{ projectile.playerId, projectile.projectileId,
                                                               projectile.sceneId, projectile.x, projectile.y + 100.0f,
                                                               projectile.z };
                offAxisProjectileImpactSent = true;
                offAxisProjectileImpactRejected = !network.SendProjectileImpact(offAxisImpact);
            }
            if (projectile.playerId > 0 && projectile.projectileKind == NETWORK_PROJECTILE_ARROW &&
                projectile.phase == NETWORK_ARROW_FLYING && offAxisProjectileImpactRejected &&
                !projectileImpactSent &&
                std::abs(static_cast<int>(static_cast<short>(projectile.rotationY - 0x4000))) < 0x1000) {
                NetworkProjectileImpactPacket impact{ projectile.playerId, projectile.projectileId,
                                                       projectile.sceneId, projectile.x, projectile.y,
                                                       projectile.z };
                projectileImpactSent = network.SendProjectileImpact(impact);
            }
            if (projectile.playerId > 0 && projectile.projectileKind == NETWORK_PROJECTILE_ARROW &&
                projectile.phase == NETWORK_ARROW_STUCK) {
                projectileImpactWitnessed = true;
                arrowStuckDisplayPitchReceived = arrowStuckDisplayPitchReceived ||
                                                 (std::abs(static_cast<int>(static_cast<short>(
                                                      projectile.rotationX - 0x1000))) <= 0x0800 &&
                                                  projectile.rotationY == 0x4000);
                if (projectileStuckAt == 0) {
                    projectileStuckAt = GetTickCount64();
                }
            }
            if (projectile.playerId > 0 && projectile.projectileKind == NETWORK_PROJECTILE_ARROW &&
                !projectile.active && projectileStuckAt != 0) {
                projectileRetiredUnexpectedly = true;
            }
            bombHeldOriginReceived = bombHeldOriginReceived ||
                                     (projectile.playerId > 0 &&
                                      projectile.projectileKind == NETWORK_PROJECTILE_BOMB &&
                                      projectile.phase == NETWORK_BOMB_HELD && projectile.x == 20.0f &&
                                      projectile.y == 50.0f && projectile.z == 0.0f);
            bombReleasedReceived = bombReleasedReceived ||
                                   (projectile.playerId > 0 &&
                                    projectile.projectileKind == NETWORK_PROJECTILE_BOMB &&
                                    projectile.phase == NETWORK_BOMB_RELEASED);
        }
        NetworkPlayerDamagePacket damage{};
        while (network.PollPlayerDamage(damage)) {
            arrowDamageReceived = arrowDamageReceived ||
                                  (damage.sourcePlayerId > 0 && damage.targetPlayerId == 0 && damage.damage == 8);
            clientMeleeDamageReceived = clientMeleeDamageReceived ||
                                        (damage.sourcePlayerId > 0 && damage.targetPlayerId == 0 &&
                                         damage.damage == 16);
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
            NetworkPlayerStatePacket hostMelee = MakeState(556);
            hostMelee.x = 0.0f;
            hostMelee.y = 0.0f;
            hostMelee.z = 60.0f;
            hostMelee.itemAction = 5;
            hostMelee.meleeWeaponState = 1;
            hostMelee.meleeBase[0] = 0.0f;
            hostMelee.meleeBase[1] = 30.0f;
            hostMelee.meleeBase[2] = 45.0f;
            hostMelee.meleeTip[0] = 0.0f;
            hostMelee.meleeTip[1] = 30.0f;
            hostMelee.meleeTip[2] = -10.0f;
            hostMeleeSent = network.SendPlayerState(hostMelee);
        }
        if (hostMeleeSent && clientSawHostMeleeDamage && !witnessArrowReady) {
            NetworkPlayerStatePacket movedHost = MakeState(557);
            movedHost.x = 500.0f;
            movedHost.y = 0.0f;
            movedHost.z = 60.0f;
            witnessArrowReady = network.SendPlayerState(movedHost) && network.SendChat("runtime-witness-ready");
        }
        if (projectileImpactWitnessed && !projectileImpactAcknowledged) {
            projectileImpactAcknowledged = network.SendChat("runtime-impact-complete");
        }
        if (bombHeldOriginReceived && !bombHeldAcknowledged) {
            bombHeldAcknowledged = network.SendChat("runtime-bomb-held");
        }
        if (bombReleasedReceived && !bombReleasedAcknowledged) {
            bombReleasedAcknowledged = network.SendChat("runtime-bomb-released");
        }
        if (boulderBreakReceived && !boulderBreakAcknowledged) {
            boulderBreakAcknowledged = network.SendChat("runtime-boulder-break");
        }
        if (grassRestored && !grassRestoreAcknowledged) {
            grassRestoreAcknowledged = network.SendChat("runtime-grass-restored");
        }

        if (!responseSent && network.IsSecure() && chatReceived && privateReceived && stateReceived && voiceReceived &&
            actorEventReceived && fishHookReceived) {
            const auto players = network.Players();
            int32_t clientId = -1;
            for (const auto& player : players) {
                if (player.playerId > 0) {
                    clientId = player.playerId;
                }
            }
            if (clientId > 0 && network.SendPrivateChat(clientId, "runtime-host-private")) {
                network.SendChat("runtime-host-chat");
                NetworkPlayerStatePacket targetState = MakeState(555);
                targetState.x = 0.0f;
                targetState.y = 0.0f;
                targetState.z = 60.0f;
                network.SendPlayerState(targetState);
                NetworkActorEventPacket hostActorEvent = MakeActorEvent(555);
                hostActorEvent.homeX = 0;
                hostActorEvent.homeY = 0;
                hostActorEvent.homeZ = 60;
                hostActorEvent.x = 0.0f;
                hostActorEvent.y = 0.0f;
                hostActorEvent.z = 60.0f;
                network.SendActorEvent(hostActorEvent);
                network.SendVoice(MakeVoice(777));
                responseSent = true;
            }
        }
        Sleep(5);
    }

    network.Disconnect();
    Error("Runtime host summary: chat=%d private=%d state=%d bowString=%d voice=%d actor=%d arrowPitch=%d arrowDamage=%d "
          "clientMelee=%d hostMeleeSeen=%d fishCanonical=%d impact=%d aimedPitch=%d stuckPitch=%d stuckPersistent=%d response=%d complete=%d",
          chatReceived, privateReceived, stateReceived, bowStringScaleReceived, voiceReceived, actorEventReceived,
          arrowNativeDisplayPitchReceived, arrowDamageReceived, clientMeleeDamageReceived, clientSawHostMeleeDamage,
          fishCanonicalStateReceived,
          projectileImpactWitnessed, arrowAimedDisplayPitchReceived, arrowStuckDisplayPitchReceived,
          !projectileRetiredUnexpectedly,
          responseSent, clientComplete);
    return clientComplete && responseSent && fishReleaseReceived && fishCanonicalStateReceived &&
                   bowStringScaleReceived && projectileImpactSent &&
                   offAxisProjectileImpactRejected && projectileImpactWitnessed &&
                   arrowNativeDisplayPitchReceived && arrowAimedDisplayPitchReceived &&
                   arrowStuckDisplayPitchReceived && arrowDamageReceived &&
                   clientMeleeDamageReceived &&
                    clientSawHostMeleeDamage && grassCutReceived && grassRestored && corpseReceived &&
                    !unauthorizedGrassCutAccepted && !projectileRetiredUnexpectedly &&
                    bombHeldOriginReceived && bombReleasedReceived
                    && boulderBreakReceived
               ? 0
               : 11;
}

int RunClient() {
    ShipwrightNetworkRuntime network;
    if (!network.Connect("127.0.0.1:47778")) {
        return 20;
    }

    bool initialSent = false;
    bool privateSent = false;
    bool chatReceived = false;
    bool privateReceived = false;
    bool stateReceived = false;
    bool voiceReceived = false;
    bool actorEventReceived = false;
    bool fishHookSent = false;
    bool fishHookAcknowledged = false;
    bool postHookStateSent = false;
    bool fishCanonicalAcknowledged = false;
    bool fishReleaseSent = false;
    bool bowStateSent = false;
    bool projectileSent = false;
    bool projectileImpactAcknowledged = false;
    bool bombPlayerStateSent = false;
    bool bombHeldSent = false;
    bool bombHeldAcknowledged = false;
    bool bombReleaseSent = false;
    bool bombReleasedAcknowledged = false;
    bool boulderBreakSent = false;
    bool boulderBreakAcknowledged = false;
    bool arrowDamageAcknowledged = false;
    bool clientMeleeNearMissSent = false;
    bool clientMeleeSent = false;
    bool clientMeleeDamageAcknowledged = false;
    bool hostMeleeDamageReceived = false;
    bool hostMeleeDamageAcknowledged = false;
    bool witnessArrowReady = false;
    bool witnessBowStateSent = false;
    bool witnessProjectileSent = false;
    bool grassCutSent = false;
    bool unauthorizedGrassCutSent = false;
    bool grassRestoreAcknowledged = false;
    bool deathSent = false;
    bool respawnSent = false;
    bool prematureRespawnReceived = false;
    bool staleDeadSent = false;
    bool duplicateRespawnReceived = false;
    bool corpseReceived = false;
    unsigned __int64 bowStateSentAt = 0;
    unsigned __int64 clientMeleeNearMissSentAt = 0;
    unsigned __int64 deathSentAt = 0;
    unsigned __int64 respawnReceivedAt = 0;
    unsigned __int64 staleDeadSentAt = 0;
    unsigned __int64 bombReleasedAt = 0;

    const unsigned __int64 timeout = GetTickCount64() + 40000;
    while (GetTickCount64() < timeout) {
        network.Update();
        if (!initialSent && network.IsSecure() && network.LocalPlayerId() > 0) {
            NetworkPlayerStatePacket initialState = MakeState(110);
            initialState.sceneId = 0x49;
            initialState.x = 666.0f;
            initialState.y = -87.0f;
            initialState.z = 354.0f;
            initialState.itemAction = NETWORK_PLAYER_ITEM_FISHING_POLE;
            initialState.fishingLineScale = 0.0005f;
            initialState.fishingLineGravity = 2.25f;
            initialState.fishingState = 4;
            initialState.fishingLineHooked = 1;
            initialState.fishingLureDrawOffset[0] = 17.25f;
            initialState.fishingLureSpin = 0.375f;
            initialState.fishingLureZOffset = -725.0f;
            initialState.fishingLureOffset[1] = 42.0f;
            initialState.fishingSinkingLureUnderwater = 1;
            initialState.fishingFishActive = 1;
            initialState.fishingFishLength = TestPondFishLength();
            NetworkActorEventPacket initialActorEvent = MakeActorEvent(111);
            initialActorEvent.sceneId = 0x49;
            initialActorEvent.homeX = initialActorEvent.x = 666;
            initialActorEvent.homeY = initialActorEvent.y = -87;
            initialActorEvent.homeZ = initialActorEvent.z = 354;
            initialSent = network.SendChat("runtime-client-chat") && network.SendPlayerState(initialState) &&
                          network.SendActorEvent(initialActorEvent) && network.SendVoice(MakeVoice(444));
        }
        if (initialSent && !fishHookSent) {
            fishHookSent = network.SendActorEvent(MakeFishEvent(111, NETWORK_ACTOR_EVENT_FISH_HOOK));
        }
        if (initialSent && !grassCutSent) {
            grassCutSent = network.SendActorEvent(MakeGrassEvent(111));
        }
        if (initialSent && !unauthorizedGrassCutSent) {
            unauthorizedGrassCutSent = network.SendActorEvent(MakeUnauthorizedGrassCut(111));
        }
        if (initialSent && !privateSent) {
            privateSent = network.SendPrivateChat(0, "runtime-client-private");
        }

        NetworkChatLine line;
        while (network.PollChat(line)) {
            chatReceived = chatReceived || line.text.find("runtime-host-chat") != std::string::npos;
            privateReceived = privateReceived || line.text.find("runtime-host-private") != std::string::npos;
            projectileImpactAcknowledged = projectileImpactAcknowledged ||
                                           line.text.find("runtime-impact-complete") != std::string::npos;
            bombHeldAcknowledged = bombHeldAcknowledged ||
                                   line.text.find("runtime-bomb-held") != std::string::npos;
            bombReleasedAcknowledged = bombReleasedAcknowledged ||
                                       line.text.find("runtime-bomb-released") != std::string::npos;
            boulderBreakAcknowledged = boulderBreakAcknowledged ||
                                       line.text.find("runtime-boulder-break") != std::string::npos;
            arrowDamageAcknowledged = arrowDamageAcknowledged ||
                                      line.text.find("runtime-arrow-damage") != std::string::npos;
            clientMeleeDamageAcknowledged = clientMeleeDamageAcknowledged ||
                                            line.text.find("runtime-client-melee-damage") != std::string::npos;
            witnessArrowReady = witnessArrowReady || line.text.find("runtime-witness-ready") != std::string::npos;
            grassRestoreAcknowledged = grassRestoreAcknowledged ||
                                       line.text.find("runtime-grass-restored") != std::string::npos;
            fishHookAcknowledged = fishHookAcknowledged ||
                                   line.text.find("runtime-fish-hook") != std::string::npos;
            fishCanonicalAcknowledged = fishCanonicalAcknowledged ||
                                        line.text.find("runtime-fish-canonical") != std::string::npos;
        }
        NetworkPlayerStatePacket state{};
        while (network.PollPlayerState(state)) {
            stateReceived = stateReceived ||
                            (state.playerId == 0 && state.x == 0.0f && state.y == 0.0f && state.z == 60.0f);
            corpseReceived = corpseReceived ||
                             (state.playerId < -1 && (state.stateFlags & NETWORK_PLAYER_DEAD) != 0 &&
                              state.jointTable[0][0] == 1234);
        }
        NetworkVoicePacket voice;
        while (network.PollVoice(voice)) {
            voiceReceived = voice.playerId == 0 && voice.sequence == 777 && !voice.data.empty();
        }
        NetworkActorEventPacket actorEvent{};
        while (network.PollActorEvent(actorEvent)) {
            actorEventReceived = actorEventReceived ||
                                 (actorEvent.sourcePlayerId == 0 && actorEvent.eventId == 555);
        }
        if (fishHookAcknowledged && !postHookStateSent) {
            NetworkPlayerStatePacket hookedState = MakeState(111);
            hookedState.sceneId = 0x49;
            hookedState.x = 666.0f;
            hookedState.y = -87.0f;
            hookedState.z = 354.0f;
            hookedState.itemAction = NETWORK_PLAYER_ITEM_FISHING_POLE;
            hookedState.fishingLineScale = 0.0005f;
            hookedState.fishingLineGravity = 2.25f;
            hookedState.fishingState = 4;
            hookedState.fishingLineHooked = 1;
            hookedState.fishingLureOffset[1] = 42.0f;
            hookedState.fishingFishActive = 1;
            // Authority must replace these forged client values with the
            // canonical identity selected by the accepted fish actor key.
            hookedState.fishingFishIsLoach = 1;
            hookedState.fishingFishLength = 1.0f;
            hookedState.fishingFishOffset[1] = 42.0f;
            postHookStateSent = network.SendPlayerState(hookedState);
        }
        if (actorEventReceived && postHookStateSent && fishCanonicalAcknowledged && !fishReleaseSent) {
            fishReleaseSent = network.SendActorEvent(MakeFishEvent(111, NETWORK_ACTOR_EVENT_FISH_RELEASE));
        }
        if (fishReleaseSent && !bowStateSent) {
            NetworkPlayerStatePacket bowState = MakeState(112);
            bowState.x = 0.0f;
            bowState.y = 0.0f;
            bowState.z = 0.0f;
            bowState.itemAction = 8;
            bowState.bowStringScale = 0.625f;
            bowState.aimPitch = 0;
            bowState.aimYaw = 0;
            bowStateSent = network.SendPlayerState(bowState);
            bowStateSentAt = GetTickCount64();
        }
        if (bowStateSent && !projectileSent && GetTickCount64() - bowStateSentAt >= 100) {
            NetworkProjectileStatePacket arrow{};
            arrow.projectileId = 9001;
            arrow.sceneId = 110;
            arrow.active = 1;
            arrow.projectileKind = NETWORK_PROJECTILE_ARROW;
            arrow.phase = NETWORK_ARROW_FLYING;
            arrow.x = 0.0f;
            arrow.y = 42.0f;
            arrow.z = 0.0f;
            arrow.rotationY = 0x1FFF;
            projectileSent = network.SendProjectileState(arrow);
        }
        if (arrowDamageAcknowledged && !clientMeleeNearMissSent) {
            NetworkPlayerStatePacket nearMiss = MakeState(113);
            nearMiss.x = 0.0f;
            nearMiss.y = 0.0f;
            nearMiss.z = 0.0f;
            nearMiss.itemAction = 5;
            nearMiss.meleeWeaponState = 1;
            nearMiss.meleeBase[0] = -13.0f;
            nearMiss.meleeBase[1] = 30.0f;
            nearMiss.meleeBase[2] = 40.0f;
            nearMiss.meleeTip[0] = -13.0f;
            nearMiss.meleeTip[1] = 30.0f;
            // The host's native collision radius is 12. A complete visible
            // blade at x=-13 is a one-unit miss and must not gain phantom width.
            nearMiss.meleeTip[2] = 80.0f;
            clientMeleeNearMissSent = network.SendPlayerState(nearMiss);
            clientMeleeNearMissSentAt = GetTickCount64();
        }
        if (clientMeleeNearMissSent && !clientMeleeSent &&
            GetTickCount64() - clientMeleeNearMissSentAt >= 250) {
            if (clientMeleeDamageAcknowledged) {
                Error("Sword authority test failed: blade damaged beyond its visible tip");
                network.Disconnect();
                return 23;
            }
            NetworkPlayerStatePacket meleeState = MakeState(114);
            meleeState.x = 0.0f;
            meleeState.y = 0.0f;
            meleeState.z = 0.0f;
            meleeState.itemAction = 5;
            meleeState.meleeWeaponState = 1;
            // Move the whole visible blade to the opposite one-unit miss. The
            // interior of the swept blade surface crosses Link even though
            // both sampled blades and both endpoint trajectories miss.
            meleeState.meleeBase[0] = 13.0f;
            meleeState.meleeBase[1] = 30.0f;
            meleeState.meleeBase[2] = 40.0f;
            meleeState.meleeTip[0] = 13.0f;
            meleeState.meleeTip[1] = 30.0f;
            meleeState.meleeTip[2] = 80.0f;
            clientMeleeSent = network.SendPlayerState(meleeState);
        }
        NetworkPlayerDamagePacket damage{};
        while (network.PollPlayerDamage(damage)) {
            hostMeleeDamageReceived = hostMeleeDamageReceived ||
                                      (damage.sourcePlayerId == 0 && damage.targetPlayerId == network.LocalPlayerId() &&
                                       damage.damage == 16);
        }
        NetworkPlayerRespawnPacket respawn{};
        while (network.PollPlayerRespawn(respawn)) {
            if (GetTickCount64() - deathSentAt < NET_RESPAWN_MS) {
                prematureRespawnReceived = true;
            }
            if (respawn.playerId == network.LocalPlayerId()) {
                if (respawnSent) {
                    duplicateRespawnReceived = true;
                } else {
                    respawnSent = true;
                    respawnReceivedAt = GetTickCount64();
                    NetworkPlayerStatePacket respawnState = MakeState(118);
                    network.SendPlayerState(respawnState);
                }
            }
        }
        if (respawnSent && !staleDeadSent && GetTickCount64() - respawnReceivedAt >= 150) {
            // Simulate a delayed unreliable snapshot arriving after the reliable
            // respawn command and the living state that acknowledges it.
            NetworkPlayerStatePacket staleDeadState = MakeState(117);
            staleDeadState.stateFlags |= NETWORK_PLAYER_DEAD;
            staleDeadState.jointTable[0][0] = 1234;
            staleDeadSent = network.SendPlayerState(staleDeadState);
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
        if (witnessArrowReady && !witnessBowStateSent) {
            NetworkPlayerStatePacket witnessBow = MakeState(115);
            witnessBow.x = 0.0f;
            witnessBow.y = 0.0f;
            witnessBow.z = 0.0f;
            witnessBow.itemAction = 8;
            witnessBow.aimPitch = 0x1000;
            witnessBow.aimYaw = 0x4000;
            witnessBowStateSent = network.SendPlayerState(witnessBow);
            bowStateSentAt = GetTickCount64();
        }
        if (witnessBowStateSent && !witnessProjectileSent && GetTickCount64() - bowStateSentAt >= 200) {
            NetworkProjectileStatePacket witnessArrow{};
            witnessArrow.projectileId = 9002;
            witnessArrow.sceneId = 110;
            witnessArrow.active = 1;
            witnessArrow.projectileKind = NETWORK_PROJECTILE_ARROW;
            witnessArrow.phase = NETWORK_ARROW_FLYING;
            witnessArrow.projectileType = 2; // ARROW_NORMAL
            witnessArrow.x = 0.0f;
            witnessArrow.y = 42.0f;
            witnessArrow.z = 0.0f;
            witnessArrow.rotationY = 0x4000;
            witnessProjectileSent = network.SendProjectileState(witnessArrow);
        }
        if (projectileImpactAcknowledged && !bombPlayerStateSent) {
            NetworkPlayerStatePacket bombState = MakeState(116);
            bombState.x = 0.0f;
            bombState.y = 0.0f;
            bombState.z = 0.0f;
            bombState.itemAction = 18;
            bombPlayerStateSent = network.SendPlayerState(bombState);
            bowStateSentAt = GetTickCount64();
        }
        if (bombPlayerStateSent && !bombHeldSent && GetTickCount64() - bowStateSentAt >= 100) {
            NetworkProjectileStatePacket bomb{};
            bomb.projectileId = 9003;
            bomb.sceneId = 110;
            bomb.active = 1;
            bomb.projectileKind = NETWORK_PROJECTILE_BOMB;
            bomb.phase = NETWORK_BOMB_HELD;
            bomb.x = 20.0f;
            bomb.y = 50.0f;
            bomb.z = 0.0f;
            bombHeldSent = network.SendProjectileState(bomb);
        }
        if (bombHeldAcknowledged && !bombReleaseSent) {
            NetworkProjectileStatePacket bomb{};
            bomb.projectileId = 9003;
            bomb.sceneId = 110;
            bomb.active = 1;
            bomb.projectileKind = NETWORK_PROJECTILE_BOMB;
            bomb.phase = NETWORK_BOMB_RELEASED;
            bomb.x = 20.0f;
            bomb.y = 50.0f;
            bomb.z = 0.0f;
            bomb.velocityX = 100.0f;
            bomb.velocityY = 120.0f;
            bombReleaseSent = network.SendProjectileState(bomb);
            if (bombReleaseSent) {
                bombReleasedAt = GetTickCount64();
            }
        }
        if (bombReleaseSent && !boulderBreakSent && GetTickCount64() - bombReleasedAt >= 2500) {
            // The reliable semantic request is queued until the server-owned
            // 3.5-second bomb fuse reaches its explosion phase.
            boulderBreakSent = network.SendActorEvent(MakeBoulderEvent(111));
        }

        const bool gameplayChecksComplete =
            initialSent && privateSent && chatReceived && privateReceived && stateReceived && voiceReceived &&
            actorEventReceived && fishHookSent && fishReleaseSent && bowStateSent && projectileSent &&
            postHookStateSent && fishCanonicalAcknowledged &&
            arrowDamageAcknowledged && clientMeleeNearMissSent && clientMeleeSent && clientMeleeDamageAcknowledged &&
            hostMeleeDamageReceived && hostMeleeDamageAcknowledged && witnessProjectileSent &&
            projectileImpactAcknowledged && bombPlayerStateSent && bombHeldSent && bombHeldAcknowledged &&
            bombReleaseSent && bombReleasedAcknowledged && boulderBreakSent && boulderBreakAcknowledged &&
            grassCutSent && unauthorizedGrassCutSent && grassRestoreAcknowledged;
        if (gameplayChecksComplete && !deathSent) {
            NetworkPlayerStatePacket deadState = MakeState(117);
            deadState.stateFlags |= NETWORK_PLAYER_DEAD;
            deadState.jointTable[0][0] = 1234;
            deathSent = network.SendPlayerState(deadState);
            deathSentAt = GetTickCount64();
        }
        if (gameplayChecksComplete && respawnSent && staleDeadSent && corpseReceived && !prematureRespawnReceived &&
            GetTickCount64() - staleDeadSentAt >= NET_RESPAWN_MS + 500) {
            // Keep pumping past one complete telemetry interval so the byte
            // counters are converted into the rates shown in the title bar.
            for (int i = 0; i < 440; ++i) {
                if ((i % 10) == 0) {
                    network.SendChat("runtime-telemetry");
                }
                network.Update();
                Sleep(5);
            }
            if (network.InboundBytesPerSecond() <= 0 || network.OutboundBytesPerSecond() <= 0) {
                Error("Runtime telemetry failed: in=%d B/s out=%d B/s", network.InboundBytesPerSecond(),
                      network.OutboundBytesPerSecond());
                return 22;
            }
            network.SendChat("runtime-client-complete");
            for (int i = 0; i < 100; ++i) {
                network.Update();
                Sleep(5);
            }
            network.Disconnect();
            return 0;
        }
        Sleep(5);
    }

    network.Disconnect();
    Error("Runtime client timeout: initial=%d privateSent=%d chat=%d private=%d state=%d voice=%d actor=%d "
          "arrowDamage=%d clientMeleeNearMiss=%d clientMelee=%d clientMeleeAck=%d hostMeleeDamage=%d witnessArrow=%d impact=%d",
          initialSent, privateSent, chatReceived, privateReceived, stateReceived, voiceReceived,
          actorEventReceived, arrowDamageAcknowledged, clientMeleeNearMissSent, clientMeleeSent,
          clientMeleeDamageAcknowledged,
          hostMeleeDamageReceived, witnessProjectileSent, projectileImpactAcknowledged);
    return 21;
}

bool StartChild(const std::string& executable, const char* argument, PROCESS_INFORMATION& process) {
    std::string command = '"' + executable + "\" " + argument;
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    std::memset(&process, 0, sizeof(process));
    return CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
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
    const DWORD wait = WaitForMultipleObjects(2, processes, TRUE, 45000);
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
