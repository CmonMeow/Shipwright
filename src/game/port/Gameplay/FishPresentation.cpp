#include "FishPresentation.h"

#include "../../platform/simulation/FishingSimulation.h"

namespace {

FishPresentationSink sSink{};

} // namespace

extern "C" void FishPresentation_SetSink(const FishPresentationSink* sink) {
    sSink = sink ? *sink : FishPresentationSink{};
}

extern "C" void FishPresentation_ClearSink(void* context) {
    if (context != sSink.context) return;
    sSink = {};
}

extern "C" uint32_t FishPresentation_MakeSpawnKey(
    int32_t sceneId, int32_t roomId, int32_t homeX, int32_t homeY,
    int32_t homeZ) {
    return Game::Simulation::MakeFishSpawnKey(sceneId, roomId, homeX, homeY,
                                               homeZ);
}

extern "C" int32_t FishPresentation_Read(
    const FishPresentationIdentity* identity, FishPresentationState* state) {
    return sSink.read ? sSink.read(identity, state, sSink.context) : false;
}
