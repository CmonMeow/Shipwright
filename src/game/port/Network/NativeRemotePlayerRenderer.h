#pragma once

#include "NativePlayerPresentationState.h"
#include "../../platform/client/CorpsePresentationRegistry.h"
#include "../../platform/client/RemoteFishingEntityState.h"
#include "../../platform/client/RemotePlayerReplicaStore.h"

#include <cstdint>
#include <memory>
#include <optional>

struct Actor;
struct PlayState;

namespace SoH::Network {

struct NativePlayerWorldPosition {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Sole owner of Ocarina Actor resources used to present authoritative remote
// players and corpses. NativeClientNetworkSession never stores or dereferences a native
// remote-player Actor pointer.
class NativeRemotePlayerRenderer final {
  public:
    NativeRemotePlayerRenderer();
    ~NativeRemotePlayerRenderer();

    NativeRemotePlayerRenderer(const NativeRemotePlayerRenderer&) = delete;
    NativeRemotePlayerRenderer& operator=(const NativeRemotePlayerRenderer&) = delete;

    void Bind(Game::Client::RemotePlayerReplicaStore* players,
              Game::Client::RemoteFishingEntityState* fishing,
              Game::Client::CorpsePresentationRegistry* corpses);
    void RegisterActorType();

    NativePlayerPresentationState& UpsertPlayer(
        Game::Simulation::EntityId entity,
        const NativePlayerPresentationState& initialState);
    NativePlayerPresentationState* FindPlayer(Game::Simulation::EntityId entity);
    NativePlayerPresentationState* FindPlayer(int32_t playerId);
    const NativePlayerPresentationState* FindPlayer(int32_t playerId) const;
    bool IsPlayerReady(int32_t playerId) const;
    void MarkPlayerReady(Game::Simulation::EntityId entity);
    void ResetFishingVisuals(Game::Simulation::EntityId entity);
    void RetirePlayer(Game::Simulation::EntityId entity);

    void UpsertCorpse(Game::Simulation::EntityId entity,
                      const NativePlayerPresentationState& state);
    void RetireCorpse(Game::Simulation::EntityId entity);

    void Reconcile(PlayState* play);
    bool HasFishingPlayerInScene(int32_t sceneId) const;
    std::optional<NativePlayerWorldPosition> WorldPositionForPlayer(
        int32_t playerId) const;
    void Reset();
    void DetachAfterSceneShutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;

    static NativeRemotePlayerRenderer* sActive;
    static void ActorInit(Actor* actor, PlayState* play);
    static void ActorDestroy(Actor* actor, PlayState* play);
    static void ActorUpdate(Actor* actor, PlayState* play);
    static void ActorDraw(Actor* actor, PlayState* play);
};

} // namespace SoH::Network
