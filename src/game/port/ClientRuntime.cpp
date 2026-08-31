#include "ClientRuntime.h"

#include "Network/NativeClientNetworkSession.h"

extern "C" void ClientRuntime_RegisterActors(void) {
    SoH::Network::NativeClientNetworkSession::RegisterActors();
}

extern "C" void ClientRuntime_Initialize(int argc, char* argv[]) {
    SoH::Network::NativeClientNetworkSession::Initialize(argc, argv);
}

extern "C" void ClientRuntime_Shutdown(void) {
    SoH::Network::NativeClientNetworkSession::Shutdown();
}

extern "C" void ClientRuntime_UpdateTransport(void) {
    SoH::Network::NativeClientNetworkSession::UpdateTransport();
}

extern "C" void ClientRuntime_UpdateGameplay(PlayState* play) {
    SoH::Network::NativeClientNetworkSession::UpdateGameplay(play);
}
