#pragma once

struct PlayState;

namespace SoH::Network::NativeClientNetworkSession {

void RegisterActors();
void Initialize();
void Shutdown();
void UpdateTransport();
void UpdateGameplay(PlayState* play);

} // namespace SoH::Network::NativeClientNetworkSession
