#ifndef OTR_GLOBALS_H
#define OTR_GLOBALS_H

#pragma once

#define M_PIf 3.14159265358979323846f
#define M_PI_2f 1.57079632679489661923f // pi/2
#define M_SQRT2f 1.41421356237309504880f
#define M_SQRT1_2f 0.70710678118654752440f /* 1/sqrt(2) */

#ifdef __cplusplus
#include <engine/Context.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

struct ExtensionEntry {
    std::string path;
    std::string ext;
};

extern std::unordered_map<std::string, ExtensionEntry> ExtensionCache;

const std::string appShortName = "oot";

class OTRGlobals {
  public:
    static OTRGlobals* Instance;

    std::shared_ptr<Engine::Context> context;

    OTRGlobals();
    ~OTRGlobals();

    void Initialize();
    void RunExtract();
    uint32_t GetInterpolationFPS();
    std::shared_ptr<std::vector<std::string>> ListFiles(std::string path);

};
#endif

#ifndef __cplusplus
void InitOTR(int argc, char* argv[]);
void DeinitOTR(void);
void OTRAudio_Init();
void InitAudio();
void Graph_StartFrame();
void Graph_ProcessGfxCommands(Gfx* commands);
void Graph_ProcessFrame(void (*run_one_game_iter)(void));
void OTRGetPixelDepthPrepare(float x, float y);
uint16_t OTRGetPixelDepth(float x, float y);
int32_t OTRGetLastScancode();
char* GetResourceDataByNameHandlingMQ(const char* path);

uint64_t GetPerfCounter();
uint64_t osGetTime(void);
uint32_t osGetCount(void);
uint32_t OTRGetCurrentWidth(void);
uint32_t OTRGetCurrentHeight(void);
float OTRGetAspectRatio(void);
float OTRGetDimensionFromLeftEdge(float v);
float OTRGetDimensionFromRightEdge(float v);
int16_t OTRGetRectDimensionFromLeftEdge(float v);
int16_t OTRGetRectDimensionFromRightEdge(float v);
uint32_t OTRGetGameRenderWidth();
uint32_t OTRGetGameRenderHeight();
int AudioPlayer_Buffered(void);
int AudioPlayer_GetDesiredBuffered(void);
void AudioPlayer_Play(const uint8_t* buf, uint32_t len);
void AudioMgr_CreateNextAudioBuffer(int16_t* samples, uint32_t num_samples);
int Controller_ShouldRumble(size_t slot);
void Gfx_RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement);
void Gfx_UnregisterBlendedTexture(const char* name);
void Gfx_TextureCacheDelete(const uint8_t* addr);
void Messagebox_ShowErrorBox(char* title, char* body);

uint32_t Interpolation_GetFPS();
uint32_t Interpolation_GetFrameCount();
#endif

#ifdef __cplusplus
extern "C" {
#endif
uint64_t GetUnixTimestamp();
#ifdef __cplusplus
};
#endif

#endif
