#pragma once

#include "platform/client/RemoteProjectileReplicaStore.h"

#include <cstdint>
#include <memory>

struct Actor;
struct PlayState;

namespace Game::Multiplayer {

// Ocarina-specific rendering adapter for protocol-independent projectile
// replicas. This is the sole owner of native remote-projectile Actor pointers.
class NativeProjectileRenderer final {
  public:
    NativeProjectileRenderer();
    ~NativeProjectileRenderer();

    NativeProjectileRenderer(const NativeProjectileRenderer&) = delete;
    NativeProjectileRenderer& operator=(const NativeProjectileRenderer&) = delete;

    void Bind(Game::Client::RemoteProjectileReplicaStore* replicas);
    void RegisterActorType();
    void Track(Game::Simulation::EntityId entity);
    void Retire(Game::Simulation::EntityId entity);
    void Reconcile(PlayState* play);
    void Reset();
    void DetachAfterSceneShutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;

    static NativeProjectileRenderer* sActive;
    static void ActorInit(Actor* actor, PlayState* play);
    static void ActorDestroy(Actor* actor, PlayState* play);
    static void ActorUpdate(Actor* actor, PlayState* play);
    static void ActorDraw(Actor* actor, PlayState* play);
};

} // namespace Game::Multiplayer
