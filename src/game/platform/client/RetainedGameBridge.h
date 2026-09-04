#ifndef RETAINED_GAME_BRIDGE_H
#define RETAINED_GAME_BRIDGE_H

#pragma once

#include <stdint.h>

#include "engine/audio/AudioChannelsSetting.h"

#ifdef __cplusplus
union Gfx;
namespace Engine {
class ResourceManager;
}
class Application;
extern "C" {
#endif
void RetainedGame_BeginFrame(void);
void RetainedGame_UpdateGameplay(struct PlayState* play);
void RetainedGame_PresentGraphics(Gfx* commands);
void RetainedGame_SetAudioChannels(AudioChannelsSetting channels);
uint32_t RetainedGame_GetPresentationFps(void);
uint32_t RetainedGame_GetPresentationFrameCount(void);
void RetainedGame_PreparePixelDepth(float x, float y);
uint16_t RetainedGame_GetPixelDepth(float x, float y);
float RetainedGame_GetAspectRatio(void);
float RetainedGame_GetDimensionFromLeftEdge(float value);
float RetainedGame_GetDimensionFromRightEdge(float value);
int16_t RetainedGame_GetRectDimensionFromLeftEdge(float value);
int16_t RetainedGame_GetRectDimensionFromRightEdge(float value);
uint32_t RetainedGame_GetRenderWidth(void);
uint32_t RetainedGame_GetRenderHeight(void);
bool RetainedGame_IsGraphicsDebugging(void);
bool RetainedGame_IsGraphicsDebuggingRequested(void);
void RetainedGame_DebugDisplayList(void* commands);

#ifdef __cplusplus
}
Engine::ResourceManager* RetainedGame_GetResourceManager();
void RetainedGame_Bind(Application* application);
#endif

#endif
