#include "NativeClientInboundReplication.h"

#include "engine/input/PCInput.h"
#include "NativeCorpsePresentationController.h"
#include "NativeLocalProjectileController.h"
#include "NativeRemotePlayerPresentationController.h"
#include "NativeRemoteProjectilePresentationController.h"
#include "NetworkRuntime.h"
#include "../../platform/client/ClientGameplaySession.h"
#include "../Enhancements/debugger/colViewer.h"

#include "global.h"
#include "variables.h"

#include <chrono>

namespace SoH::Network {
namespace {

double NowSeconds() {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

NativeClientInboundReplication::NativeClientInboundReplication(
    NativeClientInboundDependencies dependencies)
    : mDependencies(dependencies) {
}

void NativeClientInboundReplication::ReceivePlayers(PlayState* play) {
    const int32_t localPlayerId = mDependencies.runtime.LocalPlayerId();
    SetLocalCollisionPlayerId(localPlayerId);

    Game::Client::RemotePlayerPresentationState lifecycle{};
    while (mDependencies.runtime.PollPlayerLifecycle(lifecycle)) {
        if (!lifecycle.active) {
            RemoveAuthoritativePlayerCollision(lifecycle.playerId);
        }
        mDependencies.remotePlayers.ApplyLifecycle(
            lifecycle, localPlayerId,
            [this](int32_t playerId) {
                mDependencies.remoteProjectiles.RetireOwner(playerId);
            });
    }

    Game::Simulation::PlayerSnapshot snapshot{};
    while (mDependencies.runtime.PollPlayerSnapshot(snapshot)) {
        const bool localPlayer = snapshot.ownerPlayerId == localPlayerId;
        if (snapshot.health == 0) {
            RemoveAuthoritativePlayerCollision(snapshot.ownerPlayerId);
        } else {
            RecordAuthoritativePlayerCollision(
                snapshot.ownerPlayerId, snapshot);
        }

        if (localPlayer) {
            if (!mDependencies.gameplay.Scene().IsAuthorized(snapshot.sceneId)) {
                continue;
            }
            if (mDependencies.gameplay.Vitals().Apply(snapshot, localPlayerId) !=
                Game::Client::LocalPlayerVitalsUpdate::Applied) {
                continue;
            }
            gSaveContext.healthCapacity = STARTING_HEALTH;
            gSaveContext.health = mDependencies.gameplay.Vitals().Health();
            gSaveContext.healthAccumulator = 0;
            mDependencies.gameplay.Commands().ObserveAuthoritativeWeapon(
                snapshot.selectedWeapon, snapshot.serverTick);
            if (play && play->sceneNum == snapshot.sceneId) {
                if (Player* local = GET_PLAYER(play)) {
                    mDependencies.gameplay.Prediction().Reconcile(
                        snapshot,
                        { local->actor.world.pos.x, local->actor.world.pos.y,
                          local->actor.world.pos.z });
                }
            } else {
                mDependencies.gameplay.Prediction().Reconcile(
                    snapshot, snapshot.position);
            }
            continue;
        }
        mDependencies.remotePlayers.ApplySnapshot(snapshot, NowSeconds());
    }

    Game::Replication::FishingPresentationState fishing{};
    while (mDependencies.runtime.PollFishingPresentation(fishing)) {
        mDependencies.remotePlayers.ApplyFishingPresentation(
            fishing, NowSeconds());
    }
    Game::Client::RemoteLureEntity lure{};
    while (mDependencies.runtime.PollLureState(lure)) {
        mDependencies.remotePlayers.ApplyLure(lure);
    }
    Game::Client::RemoteFishEntity fish{};
    while (mDependencies.runtime.PollFishState(fish)) {
        mDependencies.remotePlayers.ApplyFish(fish);
    }
    Game::Client::LocalProjectileIntentDecision projectileResult{};
    while (mDependencies.runtime.PollProjectileIntentResult(projectileResult)) {
        mDependencies.localProjectiles.ApplyAuthorityResult(projectileResult);
    }
    Game::Client::RemoteProjectileReplicaState projectile{};
    while (mDependencies.runtime.PollProjectileState(projectile)) {
        if (projectile.logicalId.ownerPlayerId == localPlayerId) {
            mDependencies.localProjectiles.ApplyAuthoritativeState(
                projectile, localPlayerId);
        }
        mDependencies.remoteProjectiles.Apply(
            projectile, localPlayerId, NowSeconds());
    }
}

void NativeClientInboundReplication::ReceiveCorpses() {
    Game::Client::CorpsePresentationState corpse{};
    while (mDependencies.runtime.PollCorpseState(corpse)) {
        mDependencies.corpses.Apply(corpse);
    }
}

void NativeClientInboundReplication::ReceiveWorld() {
    Game::Client::ReplicatedStrategicTopologyState topology{};
    while (mDependencies.runtime.PollStrategicTopology(topology)) {
        mDependencies.gameplay.World().ApplyStrategicTopology(topology);
    }
    Game::Client::ReplicatedObjectiveState objective{};
    while (mDependencies.runtime.PollObjectiveState(objective)) {
        mDependencies.gameplay.World().ApplyObjective(
            objective.snapshot, objective.active);
    }
    Game::Client::ReplicatedStructureState structure{};
    while (mDependencies.runtime.PollStructureState(structure)) {
        mDependencies.gameplay.World().ApplyStructure(
            structure.snapshot, structure.active);
    }
}

void NativeClientInboundReplication::ReceiveSceneAuthority(PlayState* play) {
    Game::Client::LocalSceneAuthority authority{};
    while (mDependencies.runtime.PollSceneEntryState(authority)) {
        const auto result = mDependencies.gameplay.Scene().Apply(authority);
        if (!result.Applied()) continue;
        if (result.kind != Game::Client::LocalSceneAuthorityKind::Rejected ||
            (play && play->sceneNum == result.state.sceneId)) {
            mDependencies.gameplay.Prediction().Reset(result.state.lifeEpoch);
        }
        if (result.kind == Game::Client::LocalSceneAuthorityKind::Bootstrap ||
            result.kind == Game::Client::LocalSceneAuthorityKind::Accepted) {
            Game::Simulation::PlayerSnapshot baseline{};
            baseline.entity = result.state.entity;
            baseline.ownerPlayerId = result.state.playerId;
            baseline.sceneId = result.state.sceneId;
            baseline.lifeEpoch = result.state.lifeEpoch;
            baseline.position = { result.state.position.x,
                                  result.state.position.y,
                                  result.state.position.z };
            mDependencies.gameplay.Prediction().SeedAuthoritative(baseline);
            mDependencies.gameplay.BeginScene();
            mDependencies.localProjectiles.ResetBindings();
            PCInput_DiscardActionIntents();
        }
    }

    if (!play) return;
    Player* player = GET_PLAYER(play);
    if (!player) return;
    const auto placement =
        mDependencies.gameplay.Scene().TakePlacement(play->sceneNum);
    if (!placement) return;
    player->actor.world.pos = { placement->position.x, placement->position.y,
                                placement->position.z };
    player->actor.prevPos = player->actor.world.pos;
    player->actor.shape.rot.y = placement->heading;
    player->actor.world.rot.y = placement->heading;
}

bool NativeClientInboundReplication::EnsureSceneAuthorized(PlayState* play) {
    if (!play) return false;
    if (mDependencies.runtime.LocalPlayerId() < 0) {
        mDependencies.gameplay.Scene().Reset();
        return false;
    }
    if (mDependencies.gameplay.Scene().IsAuthorized(play->sceneNum)) {
        return true;
    }
    if (mDependencies.gameplay.Scene().PendingPlacementScene()) return false;
    const auto request = mDependencies.gameplay.Scene().Prepare(play->sceneNum);
    if (!request) return false;
    const bool sent = mDependencies.runtime.SendSceneEntryIntent(*request);
    mDependencies.gameplay.Scene().ResolveTransport(request->sequence, sent);
    return false;
}

} // namespace SoH::Network
