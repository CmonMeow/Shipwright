#include "FishingGameplay.h"

namespace {

FishingGameplayActionSink sActionSink = nullptr;
void* sActionContext = nullptr;

} // namespace

extern "C" void FishingGameplay_SetActionSink(
    FishingGameplayActionSink sink, void* context) {
    sActionSink = sink;
    sActionContext = context;
}

extern "C" void FishingGameplay_ClearActionSink(void* context) {
    if (context != sActionContext) return;
    sActionSink = nullptr;
    sActionContext = nullptr;
}

extern "C" int32_t FishingGameplay_SubmitAction(
    FishingGameplayAction action) {
    return sActionSink ? sActionSink(action, sActionContext) : false;
}
