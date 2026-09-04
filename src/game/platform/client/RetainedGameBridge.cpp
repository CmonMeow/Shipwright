#include "platform/client/RetainedGameBridge.h"
#include "platform/client/Application.h"

#include <cmath>

#include "z64.h"

namespace {

Application* gApplication = nullptr;

} // namespace

void RetainedGame_Bind(Application* application) {
    gApplication = application;
}

extern "C" void RetainedGame_BeginFrame() {
    if (gApplication) {
        gApplication->BeginFrame();
    }
}

extern "C" void RetainedGame_UpdateGameplay(PlayState* play) {
    if (gApplication) {
        gApplication->UpdateGameplay(play);
    }
}

extern "C" void RetainedGame_PresentGraphics(Gfx* commands) {
    if (gApplication) {
        gApplication->PresentGraphics(commands);
    }
}

extern "C" void RetainedGame_SetAudioChannels(AudioChannelsSetting channels) {
    if (gApplication) {
        gApplication->SetAudioChannels(channels);
    }
}

extern "C" uint32_t RetainedGame_GetPresentationFps() {
    return gApplication ? gApplication->GetPresentationFps() : 20;
}

extern "C" uint32_t RetainedGame_GetPresentationFrameCount() {
    return gApplication ? gApplication->GetPresentationFrameCount() : 1;
}

extern "C" void RetainedGame_PreparePixelDepth(float x, float y) {
    if (gApplication) {
        gApplication->PreparePixelDepth(x, y);
    }
}

extern "C" uint16_t RetainedGame_GetPixelDepth(float x, float y) {
    return gApplication ? gApplication->GetPixelDepth(x, y) : 0;
}

extern "C" float RetainedGame_GetAspectRatio() {
    return gApplication ? gApplication->GetAspectRatio() : 4.0f / 3.0f;
}

extern "C" float RetainedGame_GetDimensionFromLeftEdge(float value) {
    return SCREEN_WIDTH / 2 - SCREEN_HEIGHT / 2 * RetainedGame_GetAspectRatio() + value;
}

extern "C" float RetainedGame_GetDimensionFromRightEdge(float value) {
    return SCREEN_WIDTH / 2 + SCREEN_HEIGHT / 2 * RetainedGame_GetAspectRatio() - (SCREEN_WIDTH - value);
}

extern "C" int16_t RetainedGame_GetRectDimensionFromLeftEdge(float value) {
    return static_cast<int16_t>(std::floor(RetainedGame_GetDimensionFromLeftEdge(value)));
}

extern "C" int16_t RetainedGame_GetRectDimensionFromRightEdge(float value) {
    return static_cast<int16_t>(std::ceil(RetainedGame_GetDimensionFromRightEdge(value)));
}

extern "C" uint32_t RetainedGame_GetRenderWidth() {
    return gApplication ? gApplication->GetRenderWidth() : SCREEN_WIDTH;
}

extern "C" uint32_t RetainedGame_GetRenderHeight() {
    return gApplication ? gApplication->GetRenderHeight() : SCREEN_HEIGHT;
}

extern "C" bool RetainedGame_IsGraphicsDebugging() {
    return gApplication && gApplication->IsGraphicsDebugging();
}

extern "C" bool RetainedGame_IsGraphicsDebuggingRequested() {
    return gApplication && gApplication->IsGraphicsDebuggingRequested();
}

extern "C" void RetainedGame_DebugDisplayList(void* commands) {
    if (gApplication) {
        gApplication->DebugDisplayList(commands);
    }
}

Engine::ResourceManager* RetainedGame_GetResourceManager() {
    return gApplication ? &gApplication->Resources() : nullptr;
}
