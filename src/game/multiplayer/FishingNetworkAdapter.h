#pragma once

#include "NetworkProtocol.h"
#include "platform/replication/EntityLifetimeRegistry.h"
#include "platform/client/RemoteFishingEntityState.h"
#include "platform/simulation/FishingSimulation.h"
#include "platform/replication/FishingPresentationState.h"
#include "platform/simulation/ServerWorld.h"

#include <optional>

namespace Game::Multiplayer::FishingNetworkAdapter {

enum class LifetimeApplyKind : uint8_t {
    Rejected,
    Established,
    Replaced,
    Retired,
};

struct LifetimeApplyResult {
    LifetimeApplyKind kind = LifetimeApplyKind::Rejected;
    int32_t ownerPlayerId = -1;
    Game::Simulation::EntityId entity{};
    std::optional<Game::Simulation::EntityId> previousEntity;

    bool Accepted() const { return kind != LifetimeApplyKind::Rejected; }
};

NetworkFishStatePacket ToPacket(const Game::Simulation::FishSnapshot& fish,
                                uint32_t sequence, bool active);
NetworkLureStatePacket ToPacket(const Game::Simulation::FishingLureSnapshot& lure,
                                uint32_t sequence, bool active);
Game::Client::RemoteFishEntity ToRemoteEntity(
    const NetworkFishStatePacket& packet);
Game::Client::RemoteLureEntity ToRemoteEntity(
    const NetworkLureStatePacket& packet);
Game::Replication::FishingPresentationState ToState(
    const NetworkFishingPresentationPacket& packet);
Game::Replication::FishingPresentationIntent ToIntent(
    const NetworkFishingPresentationIntentPacket& packet);
NetworkFishingPresentationPacket ToPacket(
    const Game::Replication::FishingPresentationState& presentation);
NetworkFishingPresentationIntentPacket ToIntentPacket(
    const Game::Replication::FishingPresentationState& presentation);
Game::Simulation::LureControlCommand ToCommand(
    const NetworkLureControlIntentPacket& packet);
Game::Simulation::FishActionCommand ToCommand(
    const NetworkFishIntentPacket& packet);

bool IsSane(const NetworkFishingPresentationPacket& packet);
bool IsSane(const NetworkFishingPresentationIntentPacket& packet);
bool IsSane(const NetworkFishIntentPacket& packet);
bool IsSane(const NetworkFishStatePacket& packet);
bool IsSane(const NetworkLureControlIntentPacket& packet);
bool IsSane(const NetworkLureStatePacket& packet);

LifetimeApplyResult ApplyLifetime(
    const NetworkFishStatePacket& packet,
    Game::Replication::EntityLifetimeRegistry& lifetimes);
LifetimeApplyResult ApplyLifetime(
    const NetworkLureStatePacket& packet,
    Game::Replication::EntityLifetimeRegistry& lifetimes);

} // namespace Game::Multiplayer::FishingNetworkAdapter
