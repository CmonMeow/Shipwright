#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "z64.h"
#include <stdarg.h>

#ifdef __cplusplus
#define this thisx
extern "C"
{
#endif

#include <runtime/log/Log.h>
#include <port/ItemTableTypes.h>

#define osSyncPrintf(...) Error(__VA_ARGS__)

void gSPSegment(void* value, int segNum, uintptr_t target);
void gSPSegmentLoadRes(void* value, int segNum, uintptr_t target);
void gSPDisplayList(Gfx* pkt, Gfx* dl);
void gDPSetTileSizeInterp(Gfx* pkt, int t, float uls, float ult, float lrs, float lrt);
void gSPDisplayListOffset(Gfx* pkt, Gfx* dl, int offset);
void gSPVertex(Gfx* pkt, uintptr_t v, int n, int v0);
void gSPInvalidateTexCache(Gfx* pkt, uintptr_t texAddr);


void cleararena(void);
void bootproc(void);
void ViConfig_UpdateVi(uint32_t mode);
void ViConfig_UpdateBlack(void);
int32_t DmaMgr_CompareName(const char* name1, const char* name2);
int32_t DmaMgr_DmaRomToRam(uintptr_t rom, uintptr_t ram, size_t size);
int32_t DmaMgr_DmaHandler(OSPiHandle* pihandle, OSIoMesg* mb, int32_t direction);
void DmaMgr_Error(DmaRequest* req, const char* file, const char* errorName, const char* errorDesc);
const char* DmaMgr_GetFileNameImpl(uintptr_t vrom);
const char* DmaMgr_GetFileName(uintptr_t vrom);
void DmaMgr_ProcessMsg(DmaRequest* req);
void DmaMgr_ThreadEntry(void* arg0);
int32_t DmaMgr_SendRequestImpl(DmaRequest* req, uintptr_t ram, uintptr_t vrom, size_t size, uint32_t unk, OSMesgQueue* queue, OSMesg msg);
int32_t DmaMgr_SendRequest0(uintptr_t ram, uintptr_t vrom, size_t size);
void DmaMgr_Init(void);
int32_t DmaMgr_SendRequest2(DmaRequest* req, uintptr_t ram, uintptr_t vrom, size_t size, uint32_t unk5, OSMesgQueue* queue, OSMesg msg,
                        const char* file, int32_t line);
int32_t DmaMgr_SendRequest1(void* ram0, uintptr_t vrom, size_t size, const char* file, int32_t line);
void* Yaz0_FirstDMA(void);
void* Yaz0_NextDMA(void* curSrcPos);
void Yaz0_DecompressImpl(Yaz0Header* hdr, uint8_t* dst);
void Yaz0_Decompress(uintptr_t romStart, void* dst, size_t size);
void Locale_Init(void);
void Locale_ResetRegion(void);
uint32_t func_80001F48(void);
uint32_t func_80001F8C(void);
uint32_t Locale_IsRegionNative(void);
#ifdef __WIIU__
void _assert(const char* exp, const char* file, int32_t line);
#elif defined(__linux__)
void __assert(const char* exp, const char* file, int32_t line) __THROW;
#elif !defined(__APPLE__) && !defined(__SWITCH__) && !defined(__OpenBSD__)
void __assert(const char* exp, const char* file, int32_t line);
#endif
#if defined(__APPLE__) && defined(NDEBUG)
void __assert(const char* exp, const char* file, int32_t line);
#endif
void func_80002384(const char* exp, const char* file, uint32_t line);
OSPiHandle* osDriveRomInit(void);
void StackCheck_Init(StackEntry* entry, void* stackTop, void* stackBottom, uint32_t initValue, int32_t minSpace,
                     const char* name);
void StackCheck_Cleanup(StackEntry* entry);
int32_t StackCheck_GetState(StackEntry* entry);
uint32_t StackCheck_CheckAll(void);
uint32_t StackCheck_Check(StackEntry* entry);
float LogUtils_CheckFloatRange(const char* exp, int32_t line, const char* valueName, float value, const char* minName, float min,
                             const char* maxName, float max);
int32_t LogUtils_CheckIntRange(const char* exp, int32_t line, const char* valueName, int32_t value, const char* minName, int32_t min,
                           const char* maxName, int32_t max);
void LogUtils_LogHexDump(void* ptr, ptrdiff_t size0);
void LogUtils_LogPointer(int32_t value, uint32_t max, void* ptr, const char* name, const char* file, int32_t line);
void LogUtils_CheckBoundary(const char* name, int32_t value, int32_t unk, const char* file, int32_t line);
void LogUtils_CheckNullPointer(const char* exp, void* ptr, const char* file, int32_t line);
void LogUtils_CheckValidPointer(const char* exp, void* ptr, const char* file, int32_t line);
void LogUtils_LogThreadId(const char* name, int32_t line);
void LogUtils_HungupThread(const char* name, int32_t line);
void LogUtils_ResetHungup(void);
void __osPiCreateAccessQueue(void);
void __osPiGetAccess(void);
void __osPiRelAccess(void);
int32_t osSendMesg(OSMesgQueue* mq, OSMesg mesg, int32_t flag);
void osStopThread(OSThread* thread);
void osViExtendVStart(uint32_t arg0);
int32_t osRecvMesg(OSMesgQueue* mq, OSMesg* msg, int32_t flag);
void __osInitialize_common(void);
void __osInitialize_autodetect(void);
void __osExceptionPreamble();
// ? __osException(?);
void __osEnqueueAndYield(OSThread**);
void __osEnqueueThread(OSThread**, OSThread*);
OSThread* __osPopThread(OSThread**);
// ? __osNop(?);
void __osDispatchThread();
void __osCleanupThread(void);
void __osDequeueThread(OSThread** queue, OSThread* thread);
void osDestroyThread(OSThread* thread);
void osCreateThread(OSThread* thread, OSId id, void (*entry)(void*), void* arg, void* sp, OSPri pri);
void __osSetSR(uint32_t);
uint32_t __osGetSR();
void osWritebackDCache(void* vaddr, int32_t nbytes);
void* osViGetNextFramebuffer(void);
void osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf, int32_t cmdMsgCnt);
void __osDevMgrMain(void* arg);
int32_t __osPiRawStartDma(int32_t dir, uint32_t cartAddr, void* dramAddr, size_t size);
uint32_t osVirtualToPhysical(void* vaddr);
void osViBlack(uint8_t active);
int32_t __osSiRawReadIo(void* devAddr, uint32_t* dst);
OSId osGetThreadId(OSThread* thread);
void osViSetMode(OSViMode* mode);
uint32_t __osProbeTLB(void*);
uint32_t osGetMemSize(void);
void osSetEventMesg(OSEvent e, OSMesgQueue* mq, OSMesg msg);
int32_t _Printf(PrintCallback, void* arg, const char* fmt, va_list ap);
void osUnmapTLBAll(void);
int32_t osEPiStartDma(OSPiHandle* handle, OSIoMesg* mb, int32_t direction);
void osInvalICache(void* vaddr, int32_t nbytes);
void osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msg, int32_t count);
void osInvalDCache(void* vaddr, int32_t nbytes);
int32_t __osSiDeviceBusy(void);
int32_t osJamMesg(OSMesgQueue* mq, OSMesg mesg, int32_t flag);
void osSetThreadPri(OSThread* thread, OSPri pri);
OSPri osGetThreadPri(OSThread* thread);
int32_t __osEPiRawReadIo(OSPiHandle* handle, uint32_t devAddr, uint32_t* data);
void osViSwapBuffer(void* vaddr);
int32_t __osEPiRawStartDma(OSPiHandle* handle, int32_t direction, uint32_t cartAddr, void* dramAddr, size_t size);
void __osTimerServicesInit(void);
void __osTimerInterrupt(void);
void __osSetTimerIntr(OSTime time);
OSTime __osInsertTimer(OSTimer* timer);
#ifndef __cplusplus
void __osSetGlobalIntMask(OSHWIntr mask);
#endif
void __osSetCompare(uint32_t);
#ifndef __cplusplus
void __osResetGlobalIntMask(OSHWIntr mask);
#endif
int32_t __osDisableInt(void);
void __osRestoreInt(int32_t);
void __osViInit(void);
void __osViSwapContext(void);
OSMesgQueue* osPiGetCmdQueue(void);
int32_t osEPiReadIo(OSPiHandle* handle, uint32_t devAddr, uint32_t* data);
OSPiHandle* osCartRomInit(void);
void __osSetFpcCsr(uint32_t);
uint32_t __osGetFpcCsr();
int32_t osEPiWriteIo(OSPiHandle* handle, uint32_t devAddr, uint32_t data);
void osMapTLBRdb(void);
void osYieldThread(void);
uint32_t __osGetCause();
int32_t __osEPiRawWriteIo(OSPiHandle* handle, uint32_t devAddr, uint32_t data);
void _Litob(_Pft* args, uint8_t type);
//ldiv_t ldiv(int32_t num, int32_t denom);
//lldiv_t lldiv(int64_t num, int64_t denom);
void _Ldtob(_Pft* args, uint8_t type);
int32_t __osSiRawWriteIo(void* devAddr, uint32_t val);
void osCreateViManager(OSPri pri);
OSViContext* __osViGetCurrentContext(void);
void osStartThread(OSThread* thread);
void osViSetYScale(float scale);
void osViSetXScale(float value);
void __osSetWatchLo(uint32_t);

void func_80026230(PlayState* play, Color_RGBA8* color, int16_t arg2, int16_t arg3);
void func_80026400(PlayState* play, Color_RGBA8* color, int16_t arg2, int16_t arg3);
void func_80026608(PlayState* play);
void func_80026690(PlayState* play, Color_RGBA8* color, int16_t arg2, int16_t arg3);
void func_80026860(PlayState* play, Color_RGBA8* color, int16_t arg2, int16_t arg3);
void func_80026A6C(PlayState* play);
void EffectSs_InitInfo(PlayState* play, int32_t tableSize);
void EffectSs_ClearAll(PlayState* play);
void EffectSs_Delete(EffectSs* effectSs);
void EffectSs_Reset(EffectSs* effectSs);
void EffectSs_Insert(PlayState* play, EffectSs* effectSs);
void EffectSs_Spawn(PlayState* play, int32_t type, int32_t priority, void* initParams);
void EffectSs_UpdateAll(PlayState* play);
void EffectSs_DrawAll(PlayState* play);
int16_t func_80027DD4(int16_t arg0, int16_t arg1, int32_t arg2);
int16_t func_80027E34(int16_t arg0, int16_t arg1, float arg2);
uint8_t func_80027E84(uint8_t arg0, uint8_t arg1, float arg2);
void EffectSs_DrawGEffect(PlayState* play, EffectSs* this, void* texture);
void EffectSsDust_Spawn(PlayState* play, uint16_t drawFlags, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                        Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t scale, int16_t scaleStep, int16_t life,
                        uint8_t updateMode);
void func_8002829C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep);
void func_80028304(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep);
void func_8002836C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep, int16_t life);
void func_800283D4(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep, int16_t life);
void func_8002843C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep, int16_t life);
void func_800284A4(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep);
void func_80028510(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor, int16_t scale, int16_t scaleStep);
void func_8002857C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel);
void func_800285EC(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel);
void func_8002865C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep);
void func_800286CC(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep);
void func_8002873C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep,
                   int16_t life);
void func_800287AC(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep,
                   int16_t life);
void func_8002881C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor);
void func_80028858(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                   Color_RGBA8* envColor);
void func_80028990(PlayState* play, float randScale, Vec3f* srcPos);
void func_80028A54(PlayState* play, float randScale, Vec3f* srcPos);
void EffectSsKiraKira_SpawnSmallYellow(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel);
void EffectSsKiraKira_SpawnSmall(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                 Color_RGBA8* primColor, Color_RGBA8* envColor);
void EffectSsKiraKira_SpawnDispersed(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                     Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t scale, int32_t life);
void EffectSsKiraKira_SpawnFocused(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                   Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t scale, int32_t life);
void EffectSsBlast_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, Color_RGBA8* primColor,
                         Color_RGBA8* envColor, int16_t scale, int16_t scaleStep, int16_t sclaeStepDecay, int16_t life);
void EffectSsBlast_SpawnWhiteCustomScale(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale,
                                         int16_t scaleStep, int16_t life);
void EffectSsBlast_SpawnShockwave(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                  Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t life);
void EffectSsBlast_SpawnWhiteShockwave(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel);
void EffectSsGSpk_SpawnAccel(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                             Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t scale, int16_t scaleStep);
void EffectSsGSpk_SpawnNoAccel(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                               Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t scale, int16_t scaleStep);
void EffectSsGSpk_SpawnFuse(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel);
void EffectSsGSpk_SpawnRandColor(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                 int16_t scale, int16_t scaleStep);
void EffectSsGSpk_SpawnSmall(PlayState* play, Actor* actor, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                             Color_RGBA8* primColor, Color_RGBA8* envColor);
void EffectSsDFire_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep,
                         int16_t alpha, int16_t fadeDelay, int32_t life);
void EffectSsDFire_SpawnFixedScale(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t alpha,
                                   int16_t fadeDelay);
void EffectSsBubble_Spawn(PlayState* play, Vec3f* pos, float yPosOffset, float yPosRandScale, float xzPosRandScale,
                          float scale);
void EffectSsGRipple_Spawn(PlayState* play, Vec3f* pos, int16_t radius, int16_t radiusMax, int16_t life);
void EffectSsGSplash_Spawn(PlayState* play, Vec3f* pos, Color_RGBA8* primColor, Color_RGBA8* envColor,
                           int16_t type, int16_t scale);
void EffectSsGMagma_Spawn(PlayState* play, Vec3f* pos);
void EffectSsGFire_Spawn(PlayState* play, Vec3f* pos);
void EffectSsLightning_Spawn(PlayState* play, Vec3f* pos, Color_RGBA8* primColor, Color_RGBA8* envColor,
                             int16_t scale, int16_t yaw, int16_t life, int16_t numBolts);
void EffectSsDtBubble_SpawnColorProfile(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale,
                                        int16_t life, int16_t colorProfile, int16_t randXZ);
void EffectSsDtBubble_SpawnCustomColor(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel,
                                       Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t scale, int16_t life, int16_t randXZ);
void EffectSsHahen_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t unused, int16_t scale,
                         int16_t objId, int16_t life, Gfx* dList);
void EffectSsHahen_SpawnBurst(PlayState* play, Vec3f* pos, float burstScale, int16_t unused, int16_t scale,
                              int16_t randScaleRange, int16_t count, int16_t objId, int16_t life, Gfx* dList);
void EffectSsStick_Spawn(PlayState* play, Vec3f* pos, int16_t yaw);
void EffectSsSibuki_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t moveDelay,
                          int16_t direction, int16_t scale);
void EffectSsSibuki_SpawnBurst(PlayState* play, Vec3f* pos);
void EffectSsSibuki2_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale);
void EffectSsGMagma2_Spawn(PlayState* play, Vec3f* pos, Color_RGBA8* primColor, Color_RGBA8* envColor,
                           int16_t updateRate, int16_t drawMode, int16_t scale);
void EffectSsStone1_Spawn(PlayState* play, Vec3f* pos, int32_t arg2);
void EffectSsHitMark_Spawn(PlayState* play, int32_t type, int16_t scale, Vec3f* pos);
void EffectSsHitMark_SpawnFixedScale(PlayState* play, int32_t type, Vec3f* pos);
void EffectSsHitMark_SpawnCustomScale(PlayState* play, int32_t type, int16_t scale, Vec3f* pos);
void EffectSsFhgFlash_SpawnLightBall(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale,
                                     uint8_t param);
void EffectSsFhgFlash_SpawnShock(PlayState* play, Actor* actor, Vec3f* pos, int16_t scale, uint8_t param);
void EffectSsKFire_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scaleMax, uint8_t type);
void EffectSsSolderSrchBall_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t unused,
                                  int16_t* linkDetected);
void EffectSsKakera_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* arg3, int16_t gravity, int16_t arg5,
                          int16_t arg6, int16_t arg7, int16_t arg8, int16_t scale, int16_t arg10, int16_t arg11, int32_t life, int16_t colorIdx,
                          int16_t objId, Gfx* dList);
void EffectSsIcePiece_Spawn(PlayState* play, Vec3f* pos, float scale, Vec3f* velocity, Vec3f* accel, int32_t life);
void EffectSsIcePiece_SpawnBurst(PlayState* play, Vec3f* refPos, float scale);
void EffectSsEnIce_SpawnFlyingVec3f(PlayState* play, Actor* actor, Vec3f* pos, int16_t primR, int16_t primG, int16_t primB,
                                    int16_t primA, int16_t envR, int16_t envG, int16_t envB, float scale);
void EffectSsEnIce_SpawnFlyingVec3s(PlayState* play, Actor* actor, Vec3s* pos, int16_t primR, int16_t primG, int16_t primB,
                                    int16_t primA, int16_t envR, int16_t envG, int16_t envB, float scale);
void EffectSsEnIce_Spawn(PlayState* play, Vec3f* pos, float scale, Vec3f* velocity, Vec3f* accel,
                         Color_RGBA8* primColor, Color_RGBA8* envColor, int32_t life);
void EffectSsFireTail_Spawn(PlayState* play, Actor* actor, Vec3f* pos, float scale, Vec3f* arg4, int16_t arg5,
                            Color_RGBA8* primColor, Color_RGBA8* envColor, int16_t type, int16_t bodyPart, int32_t life);
void EffectSsFireTail_SpawnFlame(PlayState* play, Actor* actor, Vec3f* pos, float arg3, int16_t bodyPart,
                                 float colorIntensity);
void EffectSsFireTail_SpawnFlameOnPlayer(PlayState* play, float scale, int16_t bodyPart, float colorIntensity);
void EffectSsEnFire_SpawnVec3f(PlayState* play, Actor* actor, Vec3f* pos, int16_t scale, int16_t unk_12, int16_t flags,
                               int16_t bodyPart);
void EffectSsEnFire_SpawnVec3s(PlayState* play, Actor* actor, Vec3s* vec, int16_t scale, int16_t arg4, int16_t flags,
                               int16_t bodyPart);
void EffectSsExtra_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scoreIdx);
void EffectSsFCircle_Spawn(PlayState* play, Actor* actor, Vec3f* pos, int16_t radius, int16_t height);
void EffectSsDeadDb_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep,
                          int16_t primR, int16_t primG, int16_t primB, int16_t primA, int16_t envR, int16_t envG, int16_t envB, int16_t unused,
                          int32_t arg14, int16_t playSound);
void EffectSsDeadDd_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep,
                          int16_t primR, int16_t primG, int16_t primB, int16_t alpha, int16_t envR, int16_t envG, int16_t envB, int16_t alphaStep,
                          int32_t life);
void EffectSsDeadDd_SpawnRandYellow(PlayState* play, Vec3f* pos, int16_t scale, int16_t scaleStep, float randPosScale,
                                    int32_t randIter, int32_t life);
void EffectSsDeadDs_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale, int16_t scaleStep,
                          int16_t alpha, int32_t life);
void EffectSsDeadDs_SpawnStationary(PlayState* play, Vec3f* pos, int16_t scale, int16_t scaleStep, int16_t alpha,
                                    int32_t life);
void EffectSsDeadSound_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, uint16_t sfxId,
                             int16_t lowerPriority, int16_t repeatMode, int32_t life);
void EffectSsDeadSound_SpawnStationary(PlayState* play, Vec3f* pos, uint16_t sfxId, int16_t lowerPriority,
                                       int16_t repeatMode, int32_t life);
void EffectSsIceSmoke_Spawn(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel, int16_t scale);
void Overlay_LoadGameState(GameStateOverlay* overlayEntry);
void Overlay_FreeGameState(GameStateOverlay* overlayEntry);
void ActorShape_Init(ActorShape* shape, float yOffset, ActorShadowFunc shadowDraw, float shadowScale);
void ActorShadow_DrawCircle(Actor* actor, Lights* lights, PlayState* play);
void ActorShadow_DrawWhiteCircle(Actor* actor, Lights* lights, PlayState* play);
void ActorShadow_DrawHorse(Actor* actor, Lights* lights, PlayState* play);
void ActorShadow_DrawFeet(Actor* actor, Lights* lights, PlayState* play);
void Actor_SetFeetPos(Actor* actor, int32_t limbIndex, int32_t leftFootIndex, Vec3f* leftFootPos, int32_t rightFootIndex,
                      Vec3f* rightFootPos);
void func_8002BE04(PlayState* play, Vec3f* arg1, Vec3f* arg2, float* arg3);
void func_8002C124(TargetContext* targetCtx, PlayState* play);
int32_t Flags_GetSwitch(PlayState* play, int32_t flag);
void Flags_SetSwitch(PlayState* play, int32_t flag);
void Flags_UnsetSwitch(PlayState* play, int32_t flag);
int32_t Flags_GetUnknown(PlayState* play, int32_t flag);
void Flags_SetUnknown(PlayState* play, int32_t flag);
void Flags_UnsetUnknown(PlayState* play, int32_t flag);
int32_t Flags_GetTreasure(PlayState* play, int32_t flag);
void Flags_SetTreasure(PlayState* play, int32_t flag);
int32_t Flags_GetClear(PlayState* play, int32_t flag);
void Flags_SetClear(PlayState* play, int32_t flag);
void Flags_UnsetClear(PlayState* play, int32_t flag);
int32_t Flags_GetTempClear(PlayState* play, int32_t flag);
void Flags_SetTempClear(PlayState* play, int32_t flag);
void Flags_UnsetTempClear(PlayState* play, int32_t flag);
int32_t Flags_GetCollectible(PlayState* play, int32_t flag);
void Flags_SetCollectible(PlayState* play, int32_t flag);
void TitleCard_InitBossName(PlayState* play, TitleCardContext* titleCtx, void* texture, int16_t x, int16_t y, uint8_t width,
                            uint8_t height, int16_t hasTranslation);
void TitleCard_InitPlaceName(PlayState* play, TitleCardContext* titleCtx, void* texture, int32_t x, int32_t y,
                             int32_t width, int32_t height, int32_t delay);
int32_t func_8002D53C(PlayState* play, TitleCardContext* titleCtx);
void Actor_Kill(Actor* actor);
void Actor_SetFocus(Actor* actor, float offset);
void Actor_SetScale(Actor* actor, float scale);
void Actor_SetObjectDependency(PlayState* play, Actor* actor);
void Actor_UpdatePos(Actor* actor);
void Actor_UpdateVelocityXZGravity(Actor* actor);
void Actor_MoveXZGravity(Actor* actor);
void Actor_UpdateVelocityXYZ(Actor* actor);
void Actor_MoveXYZ(Actor* actor);
void Actor_SetProjectileSpeed(Actor* actor, float arg1);
int16_t Actor_WorldYawTowardActor(Actor* actorA, Actor* actorB);
int16_t Actor_WorldYawTowardPoint(Actor* actor, Vec3f* refPoint);
float Actor_WorldDistXYZToActor(Actor* actorA, Actor* actorB);
float Actor_WorldDistXYZToPoint(Actor* actor, Vec3f* refPoint);
int16_t Actor_WorldPitchTowardActor(Actor* actorA, Actor* actorB);
int16_t Actor_WorldPitchTowardPoint(Actor* actor, Vec3f* refPoint);
float Actor_WorldDistXZToActor(Actor* actorA, Actor* actorB);
float Actor_WorldDistXZToPoint(Actor* actor, Vec3f* refPoint);
void Actor_WorldToActorCoords(Actor* actor, Vec3f* result, Vec3f* arg2);
float Actor_HeightDiff(Actor* actorA, Actor* actorB);
float Player_GetHeight(Player* player);
int32_t Player_ActionHandler_2(Player* player, PlayState* play);
float func_8002DCE4(Player* player);
int32_t func_8002DD6C(Player* player);
int32_t func_8002DD78(Player* player);
int32_t func_8002DDE4(PlayState* play);
int32_t func_8002DDF4(PlayState* play);
int32_t func_8002DEEC(Player* player);
int32_t func_8002DF38(PlayState* play, Actor* actor, uint8_t csMode);
int32_t Player_SetCsActionWithHaltedActors(PlayState* play, Actor* actor, uint8_t arg2);
void Player_FinishFishingCatch(PlayState* play);
void func_8002DF90(DynaPolyActor* dynaActor);
void func_8002DFA4(DynaPolyActor* dynaActor, float arg1, int16_t arg2);
int32_t Player_IsFacingActor(Actor* actor, int16_t angle, PlayState* play);
int32_t Actor_ActorBIsFacingActorA(Actor* actorA, Actor* actorB, int16_t angle);
int32_t Actor_IsFacingPlayer(Actor* actor, int16_t angle);
int32_t Actor_ActorAIsFacingActorB(Actor* actorA, Actor* actorB, int16_t angle);
int32_t Actor_IsFacingAndNearPlayer(Actor* actor, float range, int16_t angle);
int32_t Actor_ActorAIsFacingAndNearActorB(Actor* actorA, Actor* actorB, float range, int16_t angle);
void Actor_UpdateBgCheckInfo(PlayState* play, Actor* actor, float wallCheckHeight, float wallCheckRadius,
                             float ceilingCheckHeight, int32_t flags);
Hilite* func_8002EABC(Vec3f* object, Vec3f* eye, Vec3f* lightDir, GraphicsContext* gfxCtx);
Hilite* func_8002EB44(Vec3f* object, Vec3f* eye, Vec3f* lightDir, GraphicsContext* gfxCtx);
void func_8002EBCC(Actor* actor, PlayState* play, int32_t flag);
void func_8002ED80(Actor* actor, PlayState* play, int32_t flag);
PosRot* Actor_GetFocus(PosRot* arg0, Actor* actor);
PosRot* Actor_GetWorld(PosRot* arg0, Actor* actor);
PosRot* Actor_GetWorldPosShapeRot(PosRot* arg0, Actor* actor);
int32_t func_8002F0C8(Actor* actor, Player* player, int32_t arg2);
uint32_t Actor_ProcessTalkRequest(Actor* actor, PlayState* play);
int32_t func_8002F1C4(Actor* actor, PlayState* play, float arg2, float arg3, uint32_t arg4);
int32_t func_8002F298(Actor* actor, PlayState* play, float arg2, uint32_t arg3);
int32_t func_8002F2CC(Actor* actor, PlayState* play, float arg2);
int32_t func_8002F2F4(Actor* actor, PlayState* play);
uint32_t Actor_TextboxIsClosing(Actor* actor, PlayState* play);
int8_t func_8002F368(PlayState* play);
void Actor_GetScreenPos(PlayState* play, Actor* actor, int16_t* x, int16_t* y);
uint32_t Actor_HasParent(Actor* actor, PlayState* play);
void Actor_OfferCarry(Actor* actor, PlayState* play);
uint32_t Actor_HasNoParent(Actor* actor, PlayState* play);
void func_8002F5F0(Actor* actor, PlayState* play);
void func_8002F698(PlayState* play, Actor* actor, float arg2, int16_t arg3, float arg4, uint32_t arg5, uint32_t arg6);
void func_8002F6D4(PlayState* play, Actor* actor, float arg2, int16_t arg3, float arg4, uint32_t arg5);
void func_8002F71C(PlayState* play, Actor* actor, float arg2, int16_t arg3, float arg4);
void func_8002F758(PlayState* play, Actor* actor, float arg2, int16_t arg3, float arg4, uint32_t arg5);
void func_8002F7A0(PlayState* play, Actor* actor, float arg2, int16_t arg3, float arg4);
void Player_PlaySfx(Actor* actor, uint16_t sfxId);
void Audio_PlayActorSound2(Actor* actor, uint16_t sfxId);
void func_8002F850(PlayState* play, Actor* actor);
void func_8002F8F0(Actor* actor, uint16_t sfxId);
void func_8002F91C(Actor* actor, uint16_t sfxId);
void func_8002F948(Actor* actor, uint16_t sfxId);
void func_8002F974(Actor* actor, uint16_t sfxId);
void func_8002F994(Actor* actor, int32_t arg1);
int32_t func_8002F9EC(PlayState* play, Actor* actor, CollisionPoly* poly, int32_t bgId, Vec3f* pos);
void Actor_DisableLens(PlayState* play);
void func_800304DC(PlayState* play, ActorContext* actorCtx, ActorEntry* actorEntry);
void Actor_UpdateAll(PlayState* play, ActorContext* actorCtx);
int32_t func_800314D4(PlayState* play, Actor* actorB, Vec3f* arg2, float arg3);
void func_800315AC(PlayState* play, ActorContext* actorCtx);
void func_80031A28(PlayState* play, ActorContext* actorCtx);
void func_80031B14(PlayState* play, ActorContext* actorCtx);
void func_80031C3C(ActorContext* actorCtx, PlayState* play);
Actor* Actor_Spawn(ActorContext* actorCtx, PlayState* play, int16_t actorId, float posX, float posY, float posZ,
                   int16_t rotX, int16_t rotY, int16_t rotZ, int16_t params);
Actor* Actor_SpawnAsChild(ActorContext* actorCtx, Actor* parent, PlayState* play, int16_t actorId, float posX,
                          float posY, float posZ, int16_t rotX, int16_t rotY, int16_t rotZ, int16_t params);
Actor* Actor_SpawnEntry(ActorContext* actorCtx, ActorEntry* actorEntry, PlayState* play);
Actor* Actor_Delete(ActorContext* actorCtx, Actor* actor, PlayState* play);
Actor* func_80032AF0(PlayState* play, ActorContext* actorCtx, Actor** actorPtr, Player* player);
Actor* Actor_Find(ActorContext* actorCtx, int32_t actorId, int32_t actorCategory);
void Enemy_StartFinishingBlow(PlayState* play, Actor* actor);
int16_t func_80032CB4(int16_t* arg0, int16_t arg1, int16_t arg2, int16_t arg3);
void Actor_SpawnFloorDustRing(PlayState* play, Actor* actor, Vec3f* posXZ, float radius, int32_t amountMinusOne,
                              float randAccelWeight, int16_t scale, int16_t scaleStep, uint8_t useLighting);
void func_80033480(PlayState* play, Vec3f* posBase, float randRangeDiameter, int32_t amountMinusOne, int16_t scaleBase,
                   int16_t scaleStep, uint8_t arg6);
Actor* Actor_GetCollidedExplosive(PlayState* play, Collider* collider);
Actor* func_80033684(PlayState* play, Actor* explosiveActor);
Actor* Actor_GetProjectileActor(PlayState* play, Actor* refActor, float radius);
void Actor_ChangeCategory(PlayState* play, ActorContext* actorCtx, Actor* actor, uint8_t actorCategory);
void Actor_SetTextWithPrefix(PlayState* play, Actor* actor, int16_t textIdLower);
int16_t Actor_TestFloorInDirection(Actor* actor, PlayState* play, float distance, int16_t angle);
int32_t Actor_IsTargeted(PlayState* play, Actor* actor);
int32_t Actor_OtherIsTargeted(PlayState* play, Actor* actor);
float func_80033AEC(Vec3f* arg0, Vec3f* arg1, float arg2, float arg3, float arg4, float arg5);
void func_80033C30(Vec3f* arg0, Vec3f* arg1, uint8_t alpha, PlayState* play);
void func_80033DB8(PlayState* play, int16_t arg1, int16_t arg2);
void func_80033E1C(PlayState* play, int16_t arg1, int16_t arg2, int16_t arg3);
void func_80033E88(Actor* actor, PlayState* play, int16_t arg2, int16_t arg3);
float Rand_ZeroFloat(float f);
float Rand_CenteredFloat(float f);
void Actor_DrawDoorLock(PlayState* play, int32_t arg1, int32_t arg2);
void Actor_SetColorFilter(Actor* actor, int16_t colorFlag, int16_t colorIntensityMax, int16_t xluFlag, int16_t duration);
Hilite* func_800342EC(Vec3f* object, PlayState* play);
Hilite* func_8003435C(Vec3f* object, PlayState* play);
int32_t Npc_UpdateTalking(PlayState* play, Actor* actor, int16_t* talkState, float interactRange,
                      NpcGetTextIdFunc getTextId, NpcUpdateTalkStateFunc updateTalkState);
int16_t Npc_GetTrackingPresetMaxPlayerYaw(int16_t presetIndex);
void Npc_TrackPoint(Actor* actor, NpcInteractInfo* interactInfo, int16_t presetIndex, int16_t trackingMode);
void func_80034BA0(PlayState* play, SkelAnime* skelAnime, OverrideLimbDraw overrideLimbDraw,
                   PostLimbDraw postLimbDraw, Actor* actor, int16_t alpha);
void func_80034CC4(PlayState* play, SkelAnime* skelAnime, OverrideLimbDraw overrideLimbDraw,
                   PostLimbDraw postLimbDraw, Actor* actor, int16_t alpha);
int16_t Actor_UpdateAlphaByDistance(Actor* actor, PlayState* play, int16_t arg2, float arg3);
void Animation_ChangeByInfo(SkelAnime* skelAnime, AnimationInfo* animationInfo, int32_t index);
void func_80034F54(PlayState* play, int16_t* arg1, int16_t* arg2, int32_t arg3);
void Actor_Noop(Actor* actor, PlayState* play);
void Gfx_DrawDListOpa(PlayState* play, Gfx* dlist);
void Gfx_DrawDListXlu(PlayState* play, Gfx* dlist);
Actor* Actor_FindNearby(PlayState* play, Actor* refActor, int16_t actorId, uint8_t actorCategory, float range);
int32_t func_800354B4(PlayState* play, Actor* actor, float range, int16_t arg3, int16_t arg4, int16_t arg5);
void func_8003555C(PlayState* play, Vec3f* pos, Vec3f* velocity, Vec3f* accel);
void func_800355B8(PlayState* play, Vec3f* pos);
uint8_t func_800355E4(PlayState* play, Collider* collider);
uint8_t Actor_ApplyDamage(Actor* actor);
void Actor_SetDropFlag(Actor* actor, ColliderInfo* colBody, int32_t freezeFlag);
void Actor_SetDropFlagJntSph(Actor* actor, ColliderJntSph* colBody, int32_t freezeFlag);
void func_80035844(Vec3f* arg0, Vec3f* arg1, Vec3s* arg2, int32_t arg3);
void func_800359B8(Actor* actor, int16_t arg1, Vec3s* arg2);
int32_t Flags_GetEventChkInf(int32_t flag);
void Flags_SetEventChkInf(int32_t flag);
void Flags_UnsetEventChkInf(int32_t flag);
int32_t Flags_GetItemGetInf(int32_t flag);
void Flags_SetItemGetInf(int32_t flag);
void Flags_UnsetItemGetInf(int32_t flag);
int32_t Flags_GetInfTable(int32_t flag);
void Flags_SetInfTable(int32_t flag);
void Flags_UnsetInfTable(int32_t flag);
int32_t Flags_GetEventInf(int32_t flag);
void Flags_SetEventInf(int32_t flag);
void Flags_UnsetEventInf(int32_t flag);
uint16_t func_80037C30(PlayState* play, int16_t arg1);
int32_t func_80037D98(PlayState* play, Actor* actor, int16_t arg2, int32_t* arg3);
int32_t func_80038290(PlayState* play, Actor* actor, Vec3s* arg2, Vec3s* arg3, Vec3f arg4);

// ? func_80038600(?);
uint16_t DynaSSNodeList_GetNextNodeIdx(DynaSSNodeList*);
void func_80038A28(CollisionPoly* poly, float tx, float ty, float tz, MtxF* dest);
float CollisionPoly_GetPointDistanceFromPlane(CollisionPoly* poly, Vec3f* point);
CollisionHeader* BgCheck_GetCollisionHeader(CollisionContext* colCtx, int32_t bgId);
void CollisionPoly_GetVerticesByBgId(CollisionPoly* poly, int32_t bgId, CollisionContext* colCtx, Vec3f* dest);
int32_t BgCheck_CheckStaticCeiling(StaticLookup* lookup, uint16_t xpFlags, CollisionContext* colCtx, float* outY, Vec3f* pos,
                               float checkHeight, CollisionPoly** outPoly);
int32_t BgCheck_CheckLineAgainstSSList(SSList* headNodeId, CollisionContext* colCtx, uint16_t xpFlags1, uint16_t xpFlags2,
                                   Vec3f* posA, Vec3f* posB, Vec3f* outPos, CollisionPoly** outPoly, float* outDistSq,
                                   float chkDist, int32_t bccFlags);
void BgCheck_GetStaticLookupIndicesFromPos(CollisionContext* colCtx, Vec3f* pos, Vec3i* arg2);
void BgCheck_Allocate(CollisionContext* colCtx, PlayState* play, CollisionHeader* colHeader);
int32_t BgCheck_PosInStaticBoundingBox(CollisionContext* colCtx, Vec3f* pos);
float BgCheck_EntityRaycastFloor1(CollisionContext* colCtx, CollisionPoly** outPoly, Vec3f* pos);
float BgCheck_EntityRaycastFloor2(PlayState* play, CollisionContext* colCtx, CollisionPoly** outPoly,
                                Vec3f* pos);
float BgCheck_EntityRaycastFloor3(CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId, Vec3f* pos);
float BgCheck_EntityRaycastFloor4(CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId, Actor* actor,
                                Vec3f* arg4);
float BgCheck_EntityRaycastFloor5(PlayState* play, CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId,
                                Actor* actor, Vec3f* pos);
float BgCheck_EntityRaycastFloor6(CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId, Actor* actor, Vec3f* pos,
                                float chkDist);
float BgCheck_EntityRaycastFloor7(CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId, Actor* actor, Vec3f* pos);
float BgCheck_AnyRaycastFloor1(CollisionContext* colCtx, CollisionPoly* outPoly, Vec3f* pos);
float BgCheck_AnyRaycastFloor2(CollisionContext* colCtx, CollisionPoly* outPoly, int32_t* bgId, Vec3f* pos);
float BgCheck_CameraRaycastFloor2(CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId, Vec3f* pos);
float BgCheck_EntityRaycastFloor8(CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId, Actor* actor, Vec3f* pos);
float BgCheck_EntityRaycastFloor9(CollisionContext* colCtx, CollisionPoly** outPoly, int32_t* bgId, Vec3f* pos);
int32_t BgCheck_CheckWallImpl(CollisionContext* colCtx, uint16_t xpFlags, Vec3f* posResult, Vec3f* posNext, Vec3f* posPrev,
                          float radius, CollisionPoly** outPoly, int32_t* outBgId, Actor* actor, float checkHeight, uint8_t argA);
int32_t BgCheck_EntitySphVsWall1(CollisionContext* colCtx, Vec3f* posResult, Vec3f* posNext, Vec3f* posPrev, float radius,
                             CollisionPoly** outPoly, float checkHeight);
int32_t BgCheck_EntitySphVsWall2(CollisionContext* colCtx, Vec3f* posResult, Vec3f* posNext, Vec3f* posPrev, float radius,
                             CollisionPoly** outPoly, int32_t* outBgId, float checkHeight);
int32_t BgCheck_EntitySphVsWall3(CollisionContext* colCtx, Vec3f* posResult, Vec3f* posNext, Vec3f* posPrev, float radius,
                             CollisionPoly** outPoly, int32_t* outBgId, Actor* actor, float checkHeight);
int32_t BgCheck_EntitySphVsWall4(CollisionContext* colCtx, Vec3f* posResult, Vec3f* posNext, Vec3f* posPrev, float radius,
                             CollisionPoly** outPoly, int32_t* outBgId, Actor* actor, float checkHeight);
int32_t BgCheck_AnyCheckCeiling(CollisionContext* colCtx, float* outY, Vec3f* pos, float checkHeight);
int32_t BgCheck_EntityCheckCeiling(CollisionContext* colCtx, float* arg1, Vec3f* arg2, float arg3, CollisionPoly** outPoly,
                               int32_t* outBgId, Actor* actor);
int32_t BgCheck_CheckLineImpl(CollisionContext* colCtx, uint16_t xpFlags1, uint16_t xpFlags2, Vec3f* posA, Vec3f* posB,
                          Vec3f* posResult, CollisionPoly** outPoly, int32_t* bgId, Actor* actor, float chkDist,
                          uint32_t bccFlags);
int32_t BgCheck_CameraLineTest1(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult,
                            CollisionPoly** outPoly, int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace, int32_t* bgId);
int32_t BgCheck_CameraLineTest2(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult,
                            CollisionPoly** outPoly, int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace, int32_t* bgId);
int32_t BgCheck_EntityLineTest1(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult,
                            CollisionPoly** outPoly, int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace, int32_t* bgId);
int32_t BgCheck_EntityLineTest2(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult,
                            CollisionPoly** outPoly, int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace, int32_t* bgId,
                            Actor* actor);
int32_t BgCheck_EntityLineTest3(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult,
                            CollisionPoly** outPoly, int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace, int32_t* bgId,
                            Actor* actor, float chkDist);
int32_t BgCheck_ProjectileLineTest(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult,
                               CollisionPoly** outPoly, int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace,
                               int32_t* bgId);
int32_t BgCheck_AnyLineTest1(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult, CollisionPoly** outPoly,
                         int32_t chkOneFace);
int32_t BgCheck_AnyLineTest2(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult, CollisionPoly** outPoly,
                         int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace);
int32_t BgCheck_AnyLineTest3(CollisionContext* colCtx, Vec3f* posA, Vec3f* posB, Vec3f* posResult, CollisionPoly** outPoly,
                         int32_t chkWall, int32_t chkFloor, int32_t chkCeil, int32_t chkOneFace, int32_t* bgId);
int32_t BgCheck_SphVsFirstPoly(CollisionContext* colCtx, Vec3f* center, float radius);
void SSNodeList_Initialize(SSNodeList*);
void SSNodeList_Alloc(PlayState* play, SSNodeList* this, int32_t tblMax, int32_t numPolys);
uint16_t SSNodeList_GetNextNodeIdx(SSNodeList* this);
int32_t DynaPoly_IsBgIdBgActor(int32_t bgId);
void DynaPoly_Init(PlayState* play, DynaCollisionContext* dyna);
void DynaPoly_Alloc(PlayState* play, DynaCollisionContext* dyna);
void func_8003EBF8(PlayState* play, DynaCollisionContext* dyna, int32_t bgId);
void func_8003EC50(PlayState* play, DynaCollisionContext* dyna, int32_t bgId);
void func_8003ECA8(PlayState* play, DynaCollisionContext* dyna, int32_t bgId);
int32_t DynaPoly_SetBgActor(PlayState* play, DynaCollisionContext* dyna, Actor* actor, CollisionHeader* colHeader);
DynaPolyActor* DynaPoly_GetActor(CollisionContext* colCtx, int32_t bgId);
void DynaPoly_DeleteBgActor(PlayState* play, DynaCollisionContext* dyna, int32_t bgId);
void func_8003EE6C(PlayState* play, DynaCollisionContext* dyna);
void func_8003F8EC(PlayState* play, DynaCollisionContext* dyna, Actor* actor);
void DynaPoly_Setup(PlayState* play, DynaCollisionContext* dyna);
void DynaPoly_UpdateBgActorTransforms(PlayState* play, DynaCollisionContext* dyna);
float BgCheck_RaycastFloorDyna(DynaRaycast* dynaRaycast);
int32_t BgCheck_SphVsDynaWall(CollisionContext* colCtx, uint16_t xpFlags, float* outX, float* outZ, Vec3f* pos, float radius,
                          CollisionPoly** outPoly, int32_t* outBgId, Actor* actor);
int32_t BgCheck_CheckDynaCeiling(CollisionContext* colCtx, uint16_t xpFlags, float* outY, Vec3f* pos, float chkDist,
                             CollisionPoly** outPoly, int32_t* outBgId, Actor* actor);
int32_t BgCheck_CheckLineAgainstDyna(CollisionContext* colCtx, uint16_t xpFlags, Vec3f* posA, Vec3f* posB, Vec3f* posResult,
                                 CollisionPoly** outPoly, float* distSq, int32_t* outBgId, Actor* actor, float chkDist,
                                 int32_t bccFlags);
int32_t BgCheck_SphVsFirstDynaPoly(CollisionContext* colCtx, uint16_t xpFlags, CollisionPoly** outPoly, int32_t* outBgId,
                               Vec3f* center, float radius, Actor* actor, uint16_t bciFlags);
void CollisionHeader_GetVirtual(void* colHeader, CollisionHeader** dest);
void func_800418D0(CollisionContext* colCtx, PlayState* play);
void BgCheck_ResetPolyCheckTbl(SSNodeList* nodeList, int32_t numPolys);
uint32_t SurfaceType_GetCamDataIndex(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint16_t func_80041A4C(CollisionContext* colCtx, uint32_t camId, int32_t bgId);
uint16_t SurfaceType_GetCameraSType(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint16_t SurfaceType_GetNumCameras(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
Vec3s* func_80041C10(CollisionContext* colCtx, int32_t camId, int32_t bgId);
Vec3s* SurfaceType_GetCamPosData(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_GetSceneExitIndex(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t func_80041D4C(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t func_80041D70(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t func_80041D94(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t func_80041DB8(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t func_80041DE4(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t func_80041E18(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t func_80041E4C(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t func_80041E80(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t func_80041EA4(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t func_80041EC8(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_IsHorseBlocked(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t func_80041F10(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint16_t SurfaceType_GetSfx(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_GetSlope(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_GetLightSettingIndex(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_GetEcho(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_IsHookshotSurface(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t SurfaceType_IsIgnoredByEntities(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t SurfaceType_IsIgnoredByProjectiles(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t SurfaceType_IsConveyor(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_GetConveyorSpeed(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_GetConveyorDirection(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
uint32_t SurfaceType_IsWallDamage(CollisionContext* colCtx, CollisionPoly* poly, int32_t bgId);
int32_t WaterBox_GetSurface1(PlayState* play, CollisionContext* colCtx, float x, float z, float* ySurface,
                         WaterBox** outWaterBox);
int32_t WaterBox_GetSurface2(PlayState* play, CollisionContext* colCtx, Vec3f* pos, float surfaceChkDist,
                         WaterBox** outWaterBox);
int32_t WaterBox_GetSurfaceImpl(PlayState* play, CollisionContext* colCtx, float x, float z, float* ySurface,
                            WaterBox** outWaterBox);
uint32_t WaterBox_GetCamDataIndex(CollisionContext* colCtx, WaterBox* waterBox);
uint16_t WaterBox_GetCameraSType(CollisionContext* colCtx, WaterBox* waterBox);
uint32_t WaterBox_GetLightSettingIndex(CollisionContext* colCtx, WaterBox* waterBox);
int32_t func_80042708(CollisionPoly* polyA, CollisionPoly* polyB, Vec3f* point, Vec3f* closestPoint);
int32_t func_800427B4(CollisionPoly* polyA, CollisionPoly* polyB, Vec3f* pointA, Vec3f* pointB, Vec3f* closestPoint);
void BgCheck_DrawDynaCollision(PlayState*, CollisionContext*);
void BgCheck_DrawStaticCollision(PlayState*, CollisionContext*);
void func_80043334(CollisionContext* colCtx, Actor* actor, int32_t bgId);
int32_t func_800433A4(CollisionContext* colCtx, int32_t bgId, Actor* actor);
void DynaPolyActor_Init(DynaPolyActor* dynaActor, int32_t flags);
void DynaPolyActor_UnsetAllInteractFlags(DynaPolyActor* dynaActor);
void DynaPolyActor_SetActorOnTop(DynaPolyActor* dynaActor);
void DynaPoly_SetPlayerOnTop(CollisionContext* colCtx, int32_t floorBgId);
void DynaPoly_SetPlayerAbove(CollisionContext* colCtx, int32_t floorBgId);
void DynaPolyActor_SetSwitchPressed(DynaPolyActor* dynaActor);
int32_t DynaPolyActor_IsActorOnTop(DynaPolyActor* dynaActor);
int32_t DynaPolyActor_IsPlayerOnTop(DynaPolyActor* dynaActor);
int32_t DynaPolyActor_IsPlayerAbove(DynaPolyActor* dynaActor);
int32_t DynaPolyActor_IsSwitchPressed(DynaPolyActor* dynaActor);
int32_t func_800435D8(PlayState* play, DynaPolyActor* dynaActor, int16_t arg2, int16_t arg3, int16_t arg4);
void Camera_Init(Camera* camera, View* view, CollisionContext* colCtx, PlayState* play);
void Camera_InitPlayerSettings(Camera* camera, Player* player);
int16_t Camera_ChangeStatus(Camera* camera, int16_t status);
Vec3s Camera_Update(Camera* camera);
void Camera_Finish(Camera* camera);
int32_t Camera_ChangeMode(Camera* camera, int16_t mode);
int32_t Camera_CheckValidMode(Camera* camera, int16_t mode);
int32_t Camera_ChangeSetting(Camera* camera, int16_t setting);
int32_t Camera_ChangeDataIdx(Camera* camera, int32_t camDataIdx);
int16_t Camera_GetInputDirYaw(Camera* camera);
Vec3s* Camera_GetCamDir(Vec3s* dir, Camera* camera);
int16_t Camera_GetCamDirPitch(Camera* camera);
int16_t Camera_GetCamDirYaw(Camera* camera);
int32_t Camera_AddQuake(Camera* camera, int32_t arg1, int16_t y, int32_t countdown);
int32_t Camera_SetParam(Camera* camera, int32_t param, void* value);
int32_t func_8005AC48(Camera* camera, int16_t arg1);
int16_t func_8005ACFC(Camera* camera, int16_t arg1);
int16_t func_8005AD1C(Camera* camera, int16_t arg1);
int32_t Camera_ResetAnim(Camera* camera);
int32_t Camera_SetCSParams(Camera* camera, CutsceneCameraPoint* atPoints, CutsceneCameraPoint* eyePoints, Player* player,
                       int16_t relativeToPlayer);
int32_t Camera_ChangeDoorCam(Camera* camera, Actor* doorActor, int16_t camDataIdx, float arg3, int16_t timer1, int16_t timer2,
                         int16_t timer3);
int32_t Camera_Copy(Camera* dstCamera, Camera* srcCamera);
Vec3f* Camera_GetSkyboxOffset(Vec3f* dst, Camera* camera);
void Camera_SetCameraData(Camera* camera, int16_t setDataFlags, void* data0, void* data1, int16_t data2, int16_t data3,
                          UNK_TYPE arg6);
int32_t func_8005B198(void);
int16_t func_8005B1A4(Camera* camera);
DamageTable* DamageTable_Get(int32_t index);
void DamageTable_Clear(DamageTable* table);
void Collider_DrawRedPoly(GraphicsContext* gfxCtx, Vec3f* vA, Vec3f* vB, Vec3f* vC);
void Collider_DrawPoly(GraphicsContext* gfxCtx, Vec3f* vA, Vec3f* vB, Vec3f* vC, uint8_t r, uint8_t g, uint8_t b);
int32_t Collider_InitJntSph(PlayState* play, ColliderJntSph* collider);
int32_t Collider_FreeJntSph(PlayState* play, ColliderJntSph* collider);
int32_t Collider_DestroyJntSph(PlayState* play, ColliderJntSph* collider);
int32_t Collider_SetJntSphToActor(PlayState* play, ColliderJntSph* dest, ColliderJntSphInitToActor* src);
int32_t Collider_SetJntSphAllocType1(PlayState* play, ColliderJntSph* dest, Actor* actor,
                                 ColliderJntSphInitType1* src);
int32_t Collider_SetJntSphAlloc(PlayState* play, ColliderJntSph* dest, Actor* actor, ColliderJntSphInit* src);
int32_t Collider_SetJntSph(PlayState* play, ColliderJntSph* dest, Actor* actor, ColliderJntSphInit* src,
                       ColliderJntSphElement* elements);
int32_t Collider_ResetJntSphAT(PlayState* play, Collider* collider);
int32_t Collider_ResetJntSphAC(PlayState* play, Collider* collider);
int32_t Collider_ResetJntSphOC(PlayState* play, Collider* collider);
int32_t Collider_InitCylinder(PlayState* play, ColliderCylinder* collider);
int32_t Collider_DestroyCylinder(PlayState* play, ColliderCylinder* collider);
int32_t Collider_SetCylinderToActor(PlayState* play, ColliderCylinder* collider, ColliderCylinderInitToActor* src);
int32_t Collider_SetCylinderType1(PlayState* play, ColliderCylinder* collider, Actor* actor,
                              ColliderCylinderInitType1* src);
int32_t Collider_SetCylinder(PlayState* play, ColliderCylinder* collider, Actor* actor, ColliderCylinderInit* src);
int32_t Collider_ResetCylinderAT(PlayState* play, Collider* collider);
int32_t Collider_ResetCylinderAC(PlayState* play, Collider* collider);
int32_t Collider_ResetCylinderOC(PlayState* play, Collider* collider);
int32_t Collider_InitTris(PlayState* play, ColliderTris* tris);
int32_t Collider_FreeTris(PlayState* play, ColliderTris* tris);
int32_t Collider_DestroyTris(PlayState* play, ColliderTris* tris);
int32_t Collider_SetTrisAllocType1(PlayState* play, ColliderTris* dest, Actor* actor, ColliderTrisInitType1* src);
int32_t Collider_SetTrisAlloc(PlayState* play, ColliderTris* dest, Actor* actor, ColliderTrisInit* src);
int32_t Collider_SetTris(PlayState* play, ColliderTris* dest, Actor* actor, ColliderTrisInit* src,
                     ColliderTrisElement* elements);
int32_t Collider_ResetTrisAT(PlayState* play, Collider* collider);
int32_t Collider_ResetTrisAC(PlayState* play, Collider* collider);
int32_t Collider_ResetTrisOC(PlayState* play, Collider* collider);
int32_t Collider_InitQuad(PlayState* play, ColliderQuad* collider);
int32_t Collider_DestroyQuad(PlayState* play, ColliderQuad* collider);
int32_t Collider_SetQuadType1(PlayState* play, ColliderQuad* collider, Actor* actor, ColliderQuadInitType1* src);
int32_t Collider_SetQuad(PlayState* play, ColliderQuad* collider, Actor* actor, ColliderQuadInit* src);
int32_t Collider_ResetQuadAT(PlayState* play, Collider* collider);
int32_t Collider_ResetQuadAC(PlayState* play, Collider* collider);
int32_t Collider_ResetQuadOC(PlayState* play, Collider* collider);
int32_t Collider_InitLine(PlayState* play, OcLine* line);
int32_t Collider_DestroyLine(PlayState* play, OcLine* line);
int32_t Collider_SetLinePoints(PlayState* play, OcLine* ocLine, Vec3f* a, Vec3f* b);
int32_t Collider_SetLine(PlayState* play, OcLine* dest, OcLine* src);
int32_t Collider_ResetLineOC(PlayState* play, OcLine* line);
void CollisionCheck_InitContext(PlayState* play, CollisionCheckContext* colChkCtx);
void CollisionCheck_DestroyContext(PlayState* play, CollisionCheckContext* colChkCtx);
void CollisionCheck_ClearContext(PlayState* play, CollisionCheckContext* colChkCtx);
void CollisionCheck_EnableSAC(PlayState* play, CollisionCheckContext* colChkCtx);
void CollisionCheck_DisableSAC(PlayState* play, CollisionCheckContext* colChkCtx);
void Collider_Draw(PlayState* play, Collider* collider);
void CollisionCheck_DrawCollision(PlayState* play, CollisionCheckContext* colChkCtx);
int32_t CollisionCheck_SetAT(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider);
int32_t CollisionCheck_SetAT_SAC(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider, int32_t index);
int32_t CollisionCheck_SetAC(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider);
int32_t CollisionCheck_SetAC_SAC(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider, int32_t index);
int32_t CollisionCheck_SetOC(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider);
int32_t CollisionCheck_SetOC_SAC(PlayState* play, CollisionCheckContext* colChkCtx, Collider* collider, int32_t index);
int32_t CollisionCheck_SetOCLine(PlayState* play, CollisionCheckContext* colChkCtx, OcLine* collider);
void CollisionCheck_BlueBlood(PlayState* play, Collider* collider, Vec3f* v);
void CollisionCheck_AT(PlayState* play, CollisionCheckContext* colChkCtx);
void CollisionCheck_OC(PlayState* play, CollisionCheckContext* colChkCtx);
void CollisionCheck_InitInfo(CollisionCheckInfo* info);
void CollisionCheck_ResetDamage(CollisionCheckInfo* info);
void CollisionCheck_SetInfoNoDamageTable(CollisionCheckInfo* info, CollisionCheckInfoInit* init);
void CollisionCheck_SetInfo(CollisionCheckInfo* info, DamageTable* damageTable, CollisionCheckInfoInit* init);
void CollisionCheck_SetInfo2(CollisionCheckInfo* info, DamageTable* damageTable, CollisionCheckInfoInit2* init);
void CollisionCheck_SetInfoGetDamageTable(CollisionCheckInfo* info, int32_t index, CollisionCheckInfoInit2* init);
void CollisionCheck_Damage(PlayState* play, CollisionCheckContext* colChkCtx);
int32_t CollisionCheck_LineOCCheckAll(PlayState* play, CollisionCheckContext* colChkCtx, Vec3f* a, Vec3f* b);
int32_t CollisionCheck_LineOCCheck(PlayState* play, CollisionCheckContext* colChkCtx, Vec3f* a, Vec3f* b,
                               Actor** exclusions, int32_t numExclusions);
void Collider_UpdateCylinder(Actor* actor, ColliderCylinder* collider);
void Collider_SetCylinderPosition(ColliderCylinder* collider, Vec3s* pos);
void Collider_SetQuadVertices(ColliderQuad* collider, Vec3f* a, Vec3f* b, Vec3f* c, Vec3f* d);
void Collider_SetTrisVertices(ColliderTris* collider, int32_t index, Vec3f* a, Vec3f* b, Vec3f* c);
void Collider_SetTrisDim(PlayState* play, ColliderTris* collider, int32_t index, ColliderTrisElementDimInit* init);
void Collider_UpdateSpheres(int32_t limb, ColliderJntSph* collider);
void CollisionCheck_PlayMetalSound(void);
void CollisionCheck_PlayMetalSoundAt(Vec3f* actorPos);
void CollisionCheck_PlayWoodSoundAt(Vec3f* actorPos);
int32_t CollisionCheck_CylSideVsLineSeg(float radius, float height, float offset, Vec3f* actorPos, Vec3f* itemPos,
                                    Vec3f* itemProjPos, Vec3f* out1, Vec3f* out2);
uint8_t CollisionCheck_GetSwordDamage(int32_t dmgFlags, PlayState* play);
void SaveContext_Init(void);
void PlayerAction_Reset(PlayState* play);
void SoundSource_InitAll(PlayState* play);
void SoundSource_UpdateAll(PlayState* play);
void SoundSource_PlaySfxAtFixedWorldPos(PlayState* play, Vec3f* pos, int32_t duration, uint16_t sfxId);
void Flags_UnsetAllEnv(PlayState* play);
void Flags_SetEnv(PlayState* play, int16_t flag);
void Flags_UnsetEnv(PlayState* play, int16_t flag);
int32_t Flags_GetEnv(PlayState* play, int16_t flag);
float func_8006C5A8(float target, TransformData* transData, int32_t refIdx);
void SkelCurve_Clear(SkelAnimeCurve* skelCurve);
int32_t SkelCurve_Init(PlayState* play, SkelAnimeCurve* skelCurve, SkelCurveLimbList* limbListSeg,
                   TransformUpdateIndex* transUpdIdx);
void SkelCurve_Destroy(PlayState* play, SkelAnimeCurve* skelCurve);
void SkelCurve_SetAnim(SkelAnimeCurve* skelCurve, TransformUpdateIndex* transUpdIdx, float arg2, float animFinalFrame,
                       float animCurFrame, float animSpeed);
int32_t SkelCurve_Update(PlayState* play, SkelAnimeCurve* skelCurve);
void SkelCurve_Draw(Actor* actor, PlayState* play, SkelAnimeCurve* skelCurve,
                    OverrideCurveLimbDraw overrideLimbDraw, PostCurveLimbDraw postLimbDraw, int32_t lod, void* data);
int32_t func_8006F0A0(int32_t arg0);
uint16_t Environment_GetPixelDepth(int32_t x, int32_t y);
void Environment_GraphCallback(GraphicsContext* gfxCtx, void* param);
void Environment_Init(PlayState* play, EnvironmentContext* envCtx, int32_t unused);
uint8_t Environment_SmoothStepToU8(uint8_t* pvalue, uint8_t target, uint8_t scale, uint8_t step, uint8_t minStep);
uint8_t Environment_SmoothStepToS8(int8_t* pvalue, int8_t target, uint8_t scale, uint8_t step, uint8_t minStep);
float Environment_LerpWeight(uint16_t max, uint16_t min, uint16_t val);
float Environment_LerpWeightAccelDecel(uint16_t endFrame, uint16_t startFrame, uint16_t curFrame, uint16_t accelDuration, uint16_t decelDuration);
void Environment_UpdateSkybox(PlayState* play, uint8_t skyboxId, EnvironmentContext* envCtx, SkyboxContext* skyboxCtx);
void Environment_EnableUnderwaterLights(PlayState* play, int32_t waterLightsIndex);
void Environment_DisableUnderwaterLights(PlayState* play);
void Environment_Update(PlayState* play, EnvironmentContext* envCtx, LightContext* lightCtx,
                        MessageContext* msgCtx, GameOverContext* gameOverCtx, GraphicsContext* gfxCtx);
void Environment_DrawSunAndMoon(PlayState* play);
void Environment_DrawSunLensFlare(PlayState* play, EnvironmentContext* envCtx, View* view,
                                  GraphicsContext* gfxCtx, Vec3f pos, int32_t unused);
void Environment_DrawLensFlare(PlayState* play, EnvironmentContext* envCtx, View* view,
                               GraphicsContext* gfxCtx, Vec3f pos, int32_t unused, int16_t arg6, float arg7, int16_t arg8, uint8_t arg9);
void Environment_DrawRain(PlayState* play, View* view, GraphicsContext* gfxCtx);
void func_80074CE8(PlayState* play, uint32_t arg1);
void Environment_DrawSkyboxFilters(PlayState* play);
void Environment_UpdateLightningStrike(PlayState* play);
void Environment_AddLightningBolts(PlayState* play, uint8_t num);
void Environment_DrawLightning(PlayState* play, int32_t unused);
void Environment_PlaySceneSequence(PlayState* play);
void Environment_DrawCustomLensFlare(PlayState* play);
void Environment_FillScreen(GraphicsContext* gfxCtx, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, uint8_t drawFlags);
void Environment_DrawSandstorm(PlayState* play, uint8_t sandstormState);
void Environment_AdjustLights(PlayState* play, float arg1, float arg2, float arg3, float arg4);
int32_t Environment_GetBgsDayCount(void);
void Environment_ClearBgsDayCount(void);
int32_t Environment_GetTotalDays(void);
void Environment_ForcePlaySequence(uint16_t seqId);
int32_t Environment_IsForcedSequenceDisabled(void);
void Environment_PlayStormNatureAmbience(PlayState* play);
void Environment_StopStormNatureAmbience(PlayState* play);
void Environment_WarpSongLeave(PlayState* play);
float Math_CosS(int16_t angle);
float Math_SinS(int16_t angle);
int32_t Math_ScaledStepToS(int16_t* pValue, int16_t target, int16_t step);
int32_t Math_StepToS(int16_t* pValue, int16_t target, int16_t step);
int32_t Math_StepToF(float* pValue, float target, float step);
int32_t Math_StepUntilAngleS(int16_t* pValue, int16_t limit, int16_t step);
int32_t Math_StepUntilS(int16_t* pValue, int16_t limit, int16_t step);
int32_t Math_StepToAngleS(int16_t* pValue, int16_t target, int16_t step);
int32_t Math_StepUntilF(float* pValue, float limit, float step);
int32_t Math_AsymStepToF(float* pValue, float target, float incrStep, float decrStep);
void func_80077D10(float* arg0, int16_t* arg1, Input* input);
int16_t Rand_S16Offset(int16_t base, int16_t range);
void Math_Vec3f_Copy(Vec3f* dest, Vec3f* src);
void Math_Vec3s_ToVec3f(Vec3f* dest, Vec3s* src);
void Math_Vec3f_Sum(Vec3f* a, Vec3f* b, Vec3f* dest);
void Math_Vec3f_Diff(Vec3f* a, Vec3f* b, Vec3f* dest);
void Math_Vec3s_DiffToVec3f(Vec3f* dest, Vec3s* a, Vec3s* b);
void Math_Vec3f_Scale(Vec3f* vec, float scaleF);
float Math_Vec3f_DistXYZ(Vec3f* a, Vec3f* b);
float Math_Vec3f_DistXYZAndStoreDiff(Vec3f* a, Vec3f* b, Vec3f* dest);
float Math_Vec3f_DistXZ(Vec3f* a, Vec3f* b);
int16_t Math_Vec3f_Yaw(Vec3f* a, Vec3f* b);
int16_t Math_Vec3f_Pitch(Vec3f* a, Vec3f* b);
void Actor_ProcessInitChain(Actor* actor, InitChainEntry* initChain);
float Math_SmoothStepToF(float* pValue, float target, float fraction, float step, float minStep);
void Math_ApproachF(float* pValue, float target, float fraction, float step);
void Math_ApproachZeroF(float* pValue, float fraction, float step);
float Math_SmoothStepToDegF(float* pValue, float target, float fraction, float step, float minStep);
int16_t Math_SmoothStepToS(int16_t* pValue, int16_t target, int16_t scale, int16_t step, int16_t minStep);
void Math_ApproachS(int16_t* pValue, int16_t target, int16_t scale, int16_t step);
void Color_RGBA8_Copy(Color_RGBA8* dst, Color_RGBA8* src);
void Sfx_PlaySfxCentered(uint16_t sfxId);
void Sfx_PlaySfxCentered2(uint16_t sfxId);
void Sfx_PlaySfxAtPos(Vec3f* arg0, uint16_t sfxId);
int16_t getHealthMeterXOffset();
int16_t getHealthMeterYOffset();
void HealthMeter_Init(PlayState* play);
void HealthMeter_Update(PlayState* play);
void HealthMeter_Draw(PlayState* play);
void HealthMeter_HandleCriticalAlarm(PlayState* play);
uint32_t HealthMeter_IsCritical(void);
void Lights_PointSetInfo(LightInfo* info, int16_t x, int16_t y, int16_t z, uint8_t r, uint8_t g, uint8_t b, int16_t radius, int32_t type);
void Lights_PointNoGlowSetInfo(LightInfo* info, int16_t x, int16_t y, int16_t z, uint8_t r, uint8_t g, uint8_t b, int16_t radius);
void Lights_PointGlowSetInfo(LightInfo* info, int16_t x, int16_t y, int16_t z, uint8_t r, uint8_t g, uint8_t b, int16_t radius);
void Lights_PointSetColorAndRadius(LightInfo* info, uint8_t r, uint8_t g, uint8_t b, int16_t radius);
void Lights_DirectionalSetInfo(LightInfo* info, int8_t x, int8_t y, int8_t z, uint8_t r, uint8_t g, uint8_t b);
void Lights_Reset(Lights* lights, uint8_t ambentR, uint8_t ambentG, uint8_t ambentB);
void Lights_Draw(Lights* lights, GraphicsContext* gfxCtx);
void Lights_BindAll(Lights* lights, LightNode* listHead, Vec3f* vec);
void LightContext_Init(PlayState* play, LightContext* lightCtx);
void LightContext_SetAmbientColor(LightContext* lightCtx, uint8_t r, uint8_t g, uint8_t b);
void LightContext_SetFog(LightContext* lightCtx, uint8_t arg1, uint8_t arg2, uint8_t arg3, int16_t numLights, int16_t arg5);
Lights* LightContext_NewLights(LightContext* lightCtx, GraphicsContext* gfxCtx);
void LightContext_InitList(PlayState* play, LightContext* lightCtx);
void LightContext_DestroyList(PlayState* play, LightContext* lightCtx);
LightNode* LightContext_InsertLight(PlayState* play, LightContext* lightCtx, LightInfo* info);
void LightContext_RemoveLight(PlayState* play, LightContext* lightCtx, LightNode* node);
Lights* Lights_NewAndDraw(GraphicsContext* gfxCtx, uint8_t ambientR, uint8_t ambientG, uint8_t ambientB, uint8_t numLights, uint8_t r, uint8_t g,
                          uint8_t b, int8_t x, int8_t y, int8_t z);
Lights* Lights_New(GraphicsContext* gfxCtx, uint8_t ambientR, uint8_t ambientG, uint8_t ambientB);
void Lights_GlowCheckPrepare(PlayState* play);
void Lights_GlowCheck(PlayState* play);
void Lights_DrawGlow(PlayState* play);
void ZeldaArena_CheckPointer(void* ptr, size_t size, const char* name, const char* action);
void* ZeldaArena_Malloc(size_t size);
void* ZeldaArena_MallocDebug(size_t size, const char* file, int32_t line);
void* ZeldaArena_MallocR(size_t size);
void* ZeldaArena_MallocRDebug(size_t size, const char* file, int32_t line);
void* ZeldaArena_Realloc(void* ptr, size_t newSize);
void* ZeldaArena_ReallocDebug(void* ptr, size_t newSize, const char* file, int32_t line);
void ZeldaArena_Free(void* ptr);
void ZeldaArena_FreeDebug(void* ptr, const char* file, int32_t line);
void* ZeldaArena_Calloc(size_t num, size_t size);
void ZeldaArena_Display();
void ZeldaArena_GetSizes(uint32_t* outMaxFree, uint32_t* outFree, uint32_t* outAlloc);
void ZeldaArena_Check();
void ZeldaArena_Init(void* start, size_t size);
void ZeldaArena_Cleanup();
uint8_t ZeldaArena_IsInitalized();
void PreNmiBuff_Init(PreNmiBuff* this);
void PreNmiBuff_SetReset(PreNmiBuff* this);
uint32_t PreNmiBuff_IsResetting(PreNmiBuff* this);
float OLib_Vec3fDist(Vec3f* a, Vec3f* b);
float OLib_Vec3fDistXZ(Vec3f* a, Vec3f* b);
float OLib_ClampMinDist(float val, float min);
float OLib_ClampMaxDist(float val, float max);
Vec3f* OLib_Vec3fDistNormalize(Vec3f* dest, Vec3f* a, Vec3f* b);
Vec3f* OLib_VecSphGeoToVec3f(Vec3f* dest, VecSph* sph);
VecSph* OLib_Vec3fToVecSph(VecSph* dest, Vec3f* vec);
VecSph* OLib_Vec3fToVecSphGeo(VecSph* arg0, Vec3f* arg1);
VecSph* OLib_Vec3fDiffToVecSphGeo(VecSph* arg0, Vec3f* a, Vec3f* b);
Vec3f* OLib_Vec3fDiffRad(Vec3f* dest, Vec3f* a, Vec3f* b);
void Interface_ChangeAlpha(uint16_t alphaType);
void Inventory_SwapAgeEquipment(void);
void Interface_InitHorsebackArchery(PlayState* play);
void func_800849EC(PlayState* play);
void Interface_LoadItemIcon1(PlayState* play, uint16_t button);
void Interface_LoadItemIcon2(PlayState* play, uint16_t button);
uint8_t Item_Give(uint8_t item);
uint8_t Item_CheckObtainability(uint8_t item);
void Inventory_DeleteItem(uint16_t item, uint16_t invSlot);
int32_t Inventory_ReplaceItem(PlayState* play, uint16_t oldItem, uint16_t newItem);
int32_t Inventory_HasEmptyBottle(void);
bool Inventory_HasEmptyBottleSlot(void);
int32_t Inventory_HasSpecificBottle(uint8_t bottleItem);
void Inventory_UpdateBottleItem(PlayState* play, uint8_t item, uint8_t cButton);
bool Inventory_HatchPocketCucco(PlayState* play);
void Interface_SetDoAction(PlayState* play, uint16_t action);
int32_t Health_ChangeBy(PlayState* play, int16_t healthChange);
void Rupees_ChangeBy(int64_t rupeeChange);
void Inventory_ChangeAmmo(int16_t item, int16_t ammoChange);
void Interface_Update(PlayState* play);
void FrameAdvance_Init(FrameAdvanceContext* frameAdvCtx);
int32_t FrameAdvance_Update(FrameAdvanceContext* frameAdvCtx, Input* input);
uint8_t PlayerGrounded(Player* player);
void Player_SetBootData(PlayState* play, Player* player);
void Player_StartAnimMovement(PlayState* play, Player* player, int32_t flags);
int32_t Player_InBlockingCsMode(PlayState* play, Player* player);
int32_t Player_TryCsAction(PlayState* play, Actor* actor, int32_t csAction);
int32_t Player_InCsMode(PlayState* play);
int32_t Player_CheckHostileLockOn(Player* player);
int32_t Player_IsChildWithHylianShield(Player* player);
int32_t Player_ActionToModelGroup(int32_t actionParam);
void Player_SetModelsForHoldingShield(Player* player);
void Player_SetModels(Player* player, int32_t modelGroup);
void Player_SetModelGroup(Player* player, int32_t modelGroup);
void func_8008EC70(Player* player);
void Player_SetEquipmentData(PlayState* play, Player* player);
void Player_UpdateBottleHeld(PlayState* play, Player* player, int32_t item, int32_t actionParam);
void func_80837C0C(PlayState* play, Player* this, int32_t arg2, float arg3, float arg4, int16_t arg5, int32_t arg6);
void func_80838280(Player* this);
void Player_ReleaseLockOn(Player* player);
void Player_ClearZTargeting(Player* player);
void Player_SetAutoLockOnActor(PlayState* play, Actor* actor);
int32_t func_8008EF44(PlayState* play, int32_t ammo);
int32_t Player_IsBurningStickInRange(PlayState* play, Vec3f* pos, float radius, float arg3);
int32_t Player_GetStrength(void);
uint8_t Player_GetMask(PlayState* play);
Player* Player_UnsetMask(PlayState* play);
int32_t Player_HasMirrorShieldEquipped(PlayState* play);
int32_t Player_HasMirrorShieldSetToDraw(PlayState* play);
void Player_DrawHookshotReticle(PlayState* play, Player* player, float hookshotRange);
int32_t Player_HoldsHookshot(Player* player);
int32_t Player_HoldsBow(Player* player);
int32_t Player_HoldsSlingshot(Player* player);
int32_t func_8008F128(Player* player);
int32_t Player_ActionToMeleeWeapon(int32_t actionParam);
int32_t Player_GetMeleeWeaponHeld(Player* player);
int32_t Player_HoldsTwoHandedWeapon(Player* player);
int32_t Player_HoldsBrokenKnife(Player* player);
int32_t Player_ActionToBottle(Player* player, int32_t actionParam);
int32_t Player_GetBottleHeld(Player* player);
int32_t func_8008F2BC(Player* player, int32_t actionParam);
int32_t Player_GetEnvironmentalHazard(PlayState* play);
void Player_DrawImpl(PlayState* play, void** skeleton, Vec3s* jointTable, int32_t dListCount, int32_t lod, int32_t tunic,
                   int32_t boots, int32_t face, OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* this);
int32_t Player_OverrideLimbDrawGameplayCommon(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* data);
int32_t Player_OverrideLimbDrawGameplayDefault(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* data);
int32_t Player_OverrideLimbDrawGameplayFirstPerson(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* data);
int32_t Player_OverrideLimbDrawGameplayCrawling(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* data);
int32_t Player_OverrideLimbDrawPresentation(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* data);
void Player_PostLimbDrawPresentation(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3s* rot, void* data);
uint8_t func_80090480(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponDim, Vec3f* newTip,
                 Vec3f* newBase);
void Player_PostLimbDrawGameplay(PlayState* play, int32_t limbIndex, Gfx** dList, Vec3s* rot, void* data);
uint32_t func_80091738(PlayState* play, uint8_t* segment, SkelAnime* skelAnime);
void PreNMI_Init(GameState* thisx);
Vec3f* Quake_AddVec(Vec3f* dst, Vec3f* arg1, VecSph* arg2);
void Quake_UpdateShakeInfo(QuakeRequest* req, ShakeInfo* shake, float y, float x);
int16_t Quake_Callback1(QuakeRequest* req, ShakeInfo* shake);
int16_t Quake_Callback2(QuakeRequest* req, ShakeInfo* shake);
int16_t Quake_Callback3(QuakeRequest* req, ShakeInfo* shake);
int16_t Quake_Callback4(QuakeRequest* req, ShakeInfo* shake);
int16_t Quake_Callback5(QuakeRequest* req, ShakeInfo* shake);
int16_t Quake_Callback6(QuakeRequest* req, ShakeInfo* shake);
int16_t Quake_GetFreeIndex(void);
QuakeRequest* Quake_AddImpl(Camera* cam, uint32_t callbackIdx);
void Quake_Remove(QuakeRequest* req);
QuakeRequest* Quake_GetRequest(int16_t idx);
QuakeRequest* Quake_SetValue(int16_t idx, int16_t valueType, int16_t value);
uint32_t Quake_SetSpeed(int16_t idx, int16_t value);
uint32_t Quake_SetCountdown(int16_t idx, int16_t value);
int16_t Quake_GetCountdown(int16_t idx);
uint32_t Quake_SetQuakeValues(int16_t idx, int16_t y, int16_t x, int16_t zoom, int16_t rotZ);
uint32_t Quake_SetUnkValues(int16_t idx, int16_t arg1, SubQuakeRequest14 arg2);
void Quake_Init(void);
int16_t Quake_Add(Camera* cam, uint32_t callbackIdx);
uint32_t Quake_RemoveFromIdx(int16_t idx);
int16_t Quake_Calc(Camera* camera, QuakeCamCalc* camData);
Gfx* Gfx_SetFog(Gfx* gfx, int32_t r, int32_t g, int32_t b, int32_t a, int32_t near, int32_t far);
Gfx* Gfx_SetFogWithSync(Gfx* gfx, int32_t r, int32_t g, int32_t b, int32_t a, int32_t near, int32_t far);
Gfx* Gfx_SetFog2(Gfx* gfx, int32_t r, int32_t g, int32_t b, int32_t a, int32_t near, int32_t far);
Gfx* Gfx_SetupDL(Gfx* gfx, uint32_t i);
Gfx* Gfx_SetupDL_57(Gfx* gfx);
Gfx* Gfx_SetupDL_52NoCD(Gfx* gfx);
void Gfx_SetupDL_57Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_51Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_54Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_26Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_25Xlu2(GraphicsContext* gfxCtx);
void func_80093C80(PlayState* play);
void Gfx_SetupDL_25Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_25Xlu(GraphicsContext* gfxCtx);
Gfx* Gfx_SetupDL_64(Gfx* gfx);
Gfx* Gfx_SetupDL_34(Gfx* gfx);
void Gfx_SetupDL_44Xlu(GraphicsContext* gfxCtx);
void Gfx_SetupDL_36Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_28Opa(GraphicsContext* gfxCtx);
Gfx* Gfx_SetupDL_28(Gfx* gfx);
void Gfx_SetupDL_38Xlu(GraphicsContext* gfxCtx);
void Gfx_SetupDL_4Xlu(GraphicsContext* gfxCtx);
void Gfx_SetupDL_37Opa(GraphicsContext* gfxCtx);
Gfx* Gfx_SetupDL_39(Gfx* gfx);
void Gfx_SetupDL_39Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_39Overlay(GraphicsContext* gfxCtx);
void Gfx_SetupDL_39Ptr(Gfx** gfxp);
void Gfx_SetupDL_40Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_41Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_47Xlu(GraphicsContext* gfxCtx);
Gfx* Gfx_SetupDL_20NoCD(Gfx* gfx);
Gfx* Gfx_SetupDL_66(Gfx* gfx);
Gfx* func_800947AC(Gfx* gfx);
void Gfx_SetupDL_42Opa(GraphicsContext* gfxCtx);
void Gfx_SetupDL_42Overlay(GraphicsContext* gfxCtx);
void Gfx_SetupDL_27Xlu(GraphicsContext* gfxCtx);
void Gfx_SetupDL_60NoCDXlu(GraphicsContext* gfxCtx);
void Gfx_SetupDL_61Xlu(GraphicsContext* gfxCtx);
void Gfx_SetupDL_56Ptr(Gfx** gfxp);
Gfx* Gfx_BranchTexScroll(Gfx** gfxp, uint32_t x, uint32_t y, int32_t width, int32_t height);
Gfx* func_80094E78(GraphicsContext* gfxCtx, uint32_t x, uint32_t y);
Gfx* Gfx_TexScroll(GraphicsContext* gfxCtx, uint32_t x, uint32_t y, int32_t width, int32_t height);
Gfx* Gfx_TexScrollEx(GraphicsContext* gfxCtx, uint32_t x, uint32_t y, int32_t width, int32_t height, int32_t xStep, int32_t yStep);
Gfx* Gfx_TwoTexScroll(GraphicsContext* gfxCtx, int32_t tile1, uint32_t x1, uint32_t y1, int32_t width1, int32_t height1, int32_t tile2, uint32_t x2,
                      uint32_t y2, int32_t width2, int32_t height2);
Gfx* Gfx_TwoTexScrollEx(GraphicsContext* gfxCtx, int32_t tile1, uint32_t x1, uint32_t y1, int32_t width1, int32_t height1, int32_t tile2, uint32_t x2,
                        uint32_t y2, int32_t width2, int32_t height2, int32_t xStep1, int32_t yStep1, int32_t xStep2, int32_t yStep2);
Gfx* Gfx_TwoTexScrollEnvColor(GraphicsContext* gfxCtx, int32_t tile1, uint32_t x1, uint32_t y1, int32_t width1, int32_t height1, int32_t tile2,
                              uint32_t x2, uint32_t y2, int32_t width2, int32_t height2, int32_t r, int32_t g, int32_t b, int32_t a);
Gfx* Gfx_TwoTexScrollEnvColorEx(GraphicsContext* gfxCtx, int32_t tile1, uint32_t x1, uint32_t y1, int32_t width1, int32_t height1, int32_t tile2,
                              uint32_t x2, uint32_t y2, int32_t width2, int32_t height2, int32_t r, int32_t g, int32_t b, int32_t a, int32_t xStep1, int32_t yStep1, int32_t xStep2, int32_t yStep2);
Gfx* Gfx_EnvColor(GraphicsContext* gfxCtx, int32_t r, int32_t g, int32_t b, int32_t a);
void Gfx_SetupFrame(GraphicsContext* gfxCtx, uint8_t r, uint8_t g, uint8_t b);
void func_80096FD4(PlayState* play, Room* room);
uint32_t func_80096FE8(PlayState* play, RoomContext* roomCtx);
int32_t func_8009728C(PlayState* play, RoomContext* roomCtx, int32_t roomNum);
int32_t func_800973FC(PlayState* play, RoomContext* roomCtx);
void Room_Draw(PlayState* play, Room* room, uint32_t flags);
void func_80097534(PlayState* play, RoomContext* roomCtx);
void Inventory_ChangeEquipment(int16_t equipment, uint16_t value);
uint8_t Inventory_DeleteEquipment(PlayState* play, int16_t equipment);
void Inventory_ChangeUpgrade(int16_t upgrade, int16_t value);
void Object_InitBank(PlayState* play, ObjectContext* objectCtx);
void Object_UpdateBank(ObjectContext* objectCtx);
int32_t Object_GetIndex(ObjectContext* objectCtx, int16_t objectId);
int32_t Object_IsLoaded(ObjectContext* objectCtx, int32_t bankIndex);
void Scene_SetTransitionForNextEntrance(PlayState* play);
void Scene_PrepareWater(PlayState* play);
void Scene_Draw(PlayState* play);
void SkelAnime_DrawLod(PlayState* play, void** skeleton, Vec3s* jointTable,
                       OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* arg, int32_t dListIndex);
void SkelAnime_DrawFlexLod(PlayState* play, void** skeleton, Vec3s* jointTable, int32_t dListCount,
                           OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* arg,
                           int32_t dListIndex);
void SkelAnime_DrawSkeletonOpa(PlayState* play, SkelAnime* skelAnime, OverrideLimbDrawOpa overrideLimbDraw,
                               PostLimbDrawOpa postLimbDraw, void* arg);
Gfx* SkelAnime_DrawSkeleton2(PlayState* play, SkelAnime* skelAnime, OverrideLimbDrawOpa overrideLimbDraw,
                                PostLimbDrawOpa postLimbDraw, void* arg, Gfx* gfx);
void SkelAnime_DrawOpa(PlayState* play, void** skeleton, Vec3s* jointTable,
                       OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* arg);
void SkelAnime_DrawFlexOpa(PlayState* play, void** skeleton, Vec3s* jointTable, int32_t dListCount,
                           OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* arg);
int16_t Animation_GetLength(void* animation);
int16_t Animation_GetLastFrame(void* animation);
int32_t SkelAnime_GetFrameDataLegacy(LegacyAnimationHeader* animation, int32_t frame, Vec3s* frameTable);
int16_t Animation_GetLimbCountLegacy(LegacyAnimationHeader* animation);
int16_t Animation_GetLengthLegacy(LegacyAnimationHeader* animation);
int16_t Animation_GetLastFrameLegacy(LegacyAnimationHeader* animation);
Gfx* SkelAnime_Draw(PlayState* play, void** skeleton, Vec3s* jointTable, OverrideLimbDraw overrideLimbDraw,
                    PostLimbDraw postLimbDraw, void* arg, Gfx* gfx);
Gfx* SkelAnime_DrawFlex(PlayState* play, void** skeleton, Vec3s* jointTable, int32_t dListCount,
                        OverrideLimbDraw overrideLimbDraw, PostLimbDraw postLimbDraw, void* arg, Gfx* gfx);
void SkelAnime_InterpFrameTable(int32_t limbCount, Vec3s* dst, Vec3s* start, Vec3s* target, float weight);
void AnimationContext_Reset(AnimationContext* animationCtx);
void AnimationContext_SetNextQueue(PlayState* play);
void AnimationContext_DisableQueue(PlayState* play);
void AnimationContext_SetLoadFrame(PlayState* play, LinkAnimationHeader* animation, int32_t frame, int32_t limbCount,
                                   Vec3s* frameTable);
void AnimationContext_SetCopyAll(PlayState* play, int32_t vecCount, Vec3s* dst, Vec3s* src);
void AnimationContext_SetInterp(PlayState* play, int32_t vecCount, Vec3s* base, Vec3s* mod, float weight);
void AnimationContext_SetCopyTrue(PlayState* play, int32_t vecCount, Vec3s* dst, Vec3s* src, uint8_t* copyFlag);
void AnimationContext_SetCopyFalse(PlayState* play, int32_t vecCount, Vec3s* dst, Vec3s* src, uint8_t* copyFlag);
void AnimationContext_SetMoveActor(PlayState* play, Actor* actor, SkelAnime* skelAnime, float arg3);
void AnimationContext_Update(PlayState* play, AnimationContext* animationCtx);
void SkelAnime_InitLink(PlayState* play, SkelAnime* skelAnime, FlexSkeletonHeader* skeletonHeaderSeg,
                        LinkAnimationHeader* animation, int32_t initFlags, Vec3s* jointTable, Vec3s* morphTable,
                        int32_t limbCount);
void LinkAnimation_SetUpdateFunction(SkelAnime* skelAnime);
int32_t LinkAnimation_Update(PlayState* play, SkelAnime* skelAnime);
void LinkAnimation_AnimateFrame(PlayState* play, SkelAnime* skelAnime);
void Animation_SetMorph(PlayState* play, SkelAnime* skelAnime, float morphFrames);
void LinkAnimation_Change(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation, float playSpeed,
                          float startFrame, float endFrame, uint8_t mode, float morphFrames);
void LinkAnimation_PlayOnce(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation);
void LinkAnimation_PlayOnceSetSpeed(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation,
                                    float playSpeed);
void LinkAnimation_PlayLoop(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation);
void LinkAnimation_PlayLoopSetSpeed(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation,
                                    float playSpeed);
void LinkAnimation_CopyJointToMorph(PlayState* play, SkelAnime* skelAnime);
void LinkAnimation_CopyMorphToJoint(PlayState* play, SkelAnime* skelAnime);
void LinkAnimation_LoadToMorph(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation,
                               float frame);
void LinkAnimation_LoadToJoint(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation,
                               float frame);
void LinkAnimation_InterpJointMorph(PlayState* play, SkelAnime* skelAnime, float frame);
void LinkAnimation_BlendToJoint(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation1,
                                float frame1, LinkAnimationHeader* animation2, float frame2, float weight, Vec3s* blendTable);
void LinkAnimation_BlendToMorph(PlayState* play, SkelAnime* skelAnime, LinkAnimationHeader* animation1,
                                float frame1, LinkAnimationHeader* animation2, float frame2, float weight, Vec3s* blendTable);
void LinkAnimation_EndLoop(SkelAnime* skelAnime);
int32_t LinkAnimation_OnFrame(SkelAnime* skelAnime, float frame);
void SkelAnime_Init(PlayState* play, SkelAnime* skelAnime, SkeletonHeader* skeletonHeaderSeg,
                   AnimationHeader* animation, Vec3s* jointTable, Vec3s* morphTable, int32_t limbCount);
void SkelAnime_InitFlex(PlayState* play, SkelAnime* skelAnime, FlexSkeletonHeader* skeletonHeaderSeg,
                       AnimationHeader* animation, Vec3s* jointTable, Vec3s* morphTable, int32_t limbCount);
int32_t SkelAnime_InitSkin(PlayState* play, SkelAnime* skelAnime, SkeletonHeader* skeletonHeaderSeg,
                       AnimationHeader* animation);
int32_t SkelAnime_Update(SkelAnime* skelAnime);
void Animation_ChangeImpl(SkelAnime* skelAnime, AnimationHeader* animation, float playSpeed, float startFrame, float endFrame,
                          uint8_t mode, float morphFrames, int8_t taper);
void Animation_Change(SkelAnime* skelAnime, AnimationHeader* animation, float playSpeed, float startFrame, float endFrame,
                      uint8_t mode, float morphFrames);
void Animation_PlayOnce(SkelAnime* skelAnime, AnimationHeader* animation);
void Animation_MorphToPlayOnce(SkelAnime* skelAnime, AnimationHeader* animation, float morphFrames);
void Animation_PlayOnceSetSpeed(SkelAnime* skelAnime, AnimationHeader* animation, float playSpeed);
void Animation_PlayLoop(SkelAnime* skelAnime, AnimationHeader* animation);
void Animation_MorphToLoop(SkelAnime* skelAnime, AnimationHeader* animation, float morphFrames);
void Animation_PlayLoopSetSpeed(SkelAnime* skelAnime, AnimationHeader* animation, float playSpeed);
void Animation_EndLoop(SkelAnime* skelAnime);
void Animation_Reverse(SkelAnime* skelAnime);
void SkelAnime_CopyFrameTableTrue(SkelAnime* skelAnime, Vec3s* dst, Vec3s* src, uint8_t* copyFlag);
void SkelAnime_CopyFrameTableFalse(SkelAnime* skelAnime, Vec3s* dst, Vec3s* src, uint8_t* copyFlag);
void SkelAnime_UpdateTranslation(SkelAnime* skelAnime, Vec3f* pos, int16_t angle);
int32_t Animation_OnFrame(SkelAnime* skelAnime, float frame);
void SkelAnime_Free(SkelAnime* skelAnime, PlayState* play);
void SkelAnime_CopyFrameTable(SkelAnime* skelAnime, Vec3s* dst, Vec3s* src);

void Skin_UpdateVertices(MtxF* mtx, SkinVertex* skinVertices, SkinLimbModif* modifEntry, Vtx* vtxBuf, Vec3f* pos);
void Skin_DrawAnimatedLimb(GraphicsContext* gfxCtx, Skin* skin, int32_t limbIndex, int32_t arg3, int32_t drawFlags);
void Skin_DrawLimb(GraphicsContext* gfxCtx, Skin* skin, int32_t limbIndex, Gfx* dlistOverride, int32_t drawFlags);
void func_800A6330(Actor* actor, PlayState* play, Skin* skin, SkinPostDraw postDraw, int32_t setTranslation);
void func_800A6360(Actor* actor, PlayState* play, Skin* skin, SkinPostDraw postDraw, SkinOverrideLimbDraw overrideLimbDraw, int32_t setTranslation);
void func_800A6394(Actor* actor, PlayState* play, Skin* skin, SkinPostDraw postDraw, SkinOverrideLimbDraw overrideLimbDraw, int32_t setTranslation, int32_t arg6);
void func_800A63CC(Actor* actor, PlayState* play, Skin* skin, SkinPostDraw postDraw, SkinOverrideLimbDraw overrideLimbDraw, int32_t setTranslation, int32_t arg6, int32_t drawFlags);
void Skin_GetLimbPos(Skin* skin, int32_t limbIndex, Vec3f* arg2, Vec3f* dst);
void Skin_Init(PlayState* play, Skin* skin, SkeletonHeader* skeletonHeader, AnimationHeader* animationHeader);
void Skin_Free(PlayState* play, Skin* skin);
int32_t Skin_ApplyAnimTransformations(Skin* skin, MtxF* mf, Actor* actor, int32_t setTranslation);

void SkinMatrix_Vec3fMtxFMultXYZW(MtxF* mf, Vec3f* src, Vec3f* xyzDest, float* wDest);
void SkinMatrix_Vec3fMtxFMultXYZ(MtxF* mf, Vec3f* src, Vec3f* dest);
void SkinMatrix_MtxFMtxFMult(MtxF* mfA, MtxF* mfB, MtxF* dest);
void SkinMatrix_GetClear(MtxF** mf);
void SkinMatrix_Clear(MtxF* mf);
void SkinMatrix_MtxFCopy(MtxF* src, MtxF* dest);
int32_t SkinMatrix_Invert(MtxF* src, MtxF* dest);
void SkinMatrix_SetScale(MtxF* mf, float x, float y, float z);
void SkinMatrix_SetRotateZYX(MtxF* mf, int16_t x, int16_t y, int16_t z);
void SkinMatrix_SetTranslate(MtxF* mf, float x, float y, float z);
void SkinMatrix_SetTranslateRotateYXZScale(MtxF* dest, float scaleX, float scaleY, float scaleZ, int16_t rotX, int16_t rotY, int16_t rotZ,
                                           float translateX, float translateY, float translateZ);
void SkinMatrix_SetTranslateRotateZYX(MtxF* dest, int16_t rotX, int16_t rotY, int16_t rotZ, float translateX, float translateY,
                                      float translateZ);
Mtx* SkinMatrix_MtxFToNewMtx(GraphicsContext* gfxCtx, MtxF* src);
void SkinMatrix_SetRotateAxis(MtxF* mf, int16_t angle, float axisX, float axisY, float axisZ);
void func_800A9F30(PadMgr*, int32_t);
void func_800A9F6C(float, uint8_t, uint8_t, uint8_t);
void func_800AA000(float, uint8_t, uint8_t, uint8_t);
void func_800AA0B4();
void func_800AA0F0(void);
uint32_t func_800AA148();
void func_800AA15C();
void Rumble_ClearRequests();
void func_800AA178(uint32_t);
View* View_New(GraphicsContext* gfxCtx);
void View_Free(View* view);
void View_Init(View*, GraphicsContext*);
void func_800AA358(View* view, Vec3f* eye, Vec3f* lookAt, Vec3f* up);
void func_800AA3F0(View* view, Vec3f* eye, Vec3f* lookAt, Vec3f* up);
void View_SetScale(View* view, float scale);
void View_GetScale(View* view, float* scale);
void func_800AA460(View* view, float fovy, float near, float far);
void func_800AA48C(View* view, float* fovy, float* near, float* far);
void func_800AA4A8(View* view, float fovy, float near, float far);
void func_800AA4E0(View* view, float* fovy, float* near, float* far);
void View_SetViewport(View* view, Viewport* viewport);
void View_GetViewport(View* view, Viewport* viewport);
void View_SetDistortionOrientation(View* view, float rotX, float rotY, float rotZ);
void View_SetDistortionScale(View* view, float scaleX, float scaleY, float scaleZ);
void View_SetDistortionSpeed(View* view, float speed);
void View_InitDistortion(View* view);
void View_ClearDistortion(View* view);
void View_SetDistortion(View* view, Vec3f orientation, Vec3f scale, float speed);
int32_t View_StepDistortion(View* view, Mtx* projectionMtx);
void func_800AAA50(View* view, int32_t arg1);
int32_t func_800AAA9C(View* view);
int32_t func_800AB0A8(View* view);
int32_t func_800AB2C4(View* view);
int32_t func_800AB560(View* view);
int32_t func_800AB944(View* view);
int32_t func_800AB9EC(View* view, int32_t arg1, Gfx** p);
int32_t func_800ABE74(float eyeX, float eyeY, float eyeZ);
void Skybox_Init(GameState* state, SkyboxContext* skyboxCtx, int16_t skyboxId);
Mtx* SkyboxDraw_UpdateMatrix(SkyboxContext* skyboxCtx, float x, float y, float z);
void SkyboxDraw_Draw(SkyboxContext* skyboxCtx, GraphicsContext* gfxCtx, int16_t skyboxId, int16_t blend, float x, float y, float z);
void SkyboxDraw_Update(SkyboxContext* skyboxCtx);
void PlayerCall_InitFuncPtrs(void);
void TransitionFade_Start(void* this);
void* TransitionFade_Init(void* this);
void TransitionFade_Destroy(void* this);
void TransitionFade_Update(void* this, int32_t updateRate);
void TransitionFade_Draw(void* this, Gfx** gfxP);
int32_t TransitionFade_IsDone(void* this);
void TransitionFade_SetColor(void* this, uint32_t color);
void TransitionFade_SetType(void* this, int32_t type);
void ShrinkWindow_SetVal(int32_t value);
uint32_t ShrinkWindow_GetVal(void);
void ShrinkWindow_SetCurrentVal(int32_t nowVal);
uint32_t ShrinkWindow_GetCurrentVal(void);
void ShrinkWindow_Init(void);
void ShrinkWindow_Destroy(void);
void ShrinkWindow_Update(int32_t updateRate);
void func_800BB0A0(float u, Vec3f* pos, float* roll, float* viewAngle, float* point0, float* point1, float* point2, float* point3);
int32_t func_800BB2B4(Vec3f* pos, float* roll, float* fov, CutsceneCameraPoint* point, int16_t* keyframe, float* curFrame);
void Play_SetViewpoint(PlayState* play, int16_t viewpoint);
int32_t Play_CheckViewpoint(PlayState* play, int16_t viewpoint);
void Play_SetShopBrowsingViewpoint(PlayState* play);
void Gameplay_SetupTransition(PlayState* play, int32_t arg1);
Gfx* Play_SetFog(PlayState* play, Gfx* gfx);
void Play_Destroy(GameState* thisx);
void Play_Init(GameState* thisx);
void Play_Main(GameState* thisx);
uint8_t CheckBridgeRewardCount();
uint8_t CheckLACSRewardCount();
int32_t Play_InCsMode(PlayState* play);
float func_800BFCB8(PlayState* play, MtxF* mf, Vec3f* vec);
void* Play_LoadFile(PlayState* play, RomFile* file);
void Play_SpawnScene(PlayState* play, int32_t sceneId, int32_t spawn);
void func_800C016C(PlayState* play, Vec3f* src, Vec3f* dest);
int16_t Play_CreateSubCamera(PlayState* play);
int16_t Play_GetActiveCamId(PlayState* play);
int16_t Play_ChangeCameraStatus(PlayState* play, int16_t camId, int16_t status);
void Play_ClearCamera(PlayState* play, int16_t camId);
void Play_ClearAllSubCameras(PlayState* play);
Camera* Play_GetCamera(PlayState* play, int16_t camId);
int32_t Play_CameraSetAtEye(PlayState* play, int16_t camId, Vec3f* at, Vec3f* eye);
int32_t Play_CameraSetAtEyeUp(PlayState* play, int16_t camId, Vec3f* at, Vec3f* eye, Vec3f* up);
int32_t Play_CameraSetFov(PlayState* play, int16_t camId, float fov);
int32_t Play_SetCameraRoll(PlayState* play, int16_t camId, int16_t roll);
void Play_CopyCamera(PlayState* play, int16_t camId1, int16_t camId2);
int32_t func_800C0808(PlayState* play, int16_t camId, Player* player, int16_t arg3);
int32_t Play_CameraChangeSetting(PlayState* play, int16_t camId, int16_t arg2);
void func_800C08AC(PlayState* play, int16_t camId);
void Play_SaveSceneFlags(PlayState* play);
void Play_SetupRespawnPoint(PlayState* play, int32_t respawnMode, int32_t playerParams);
void Play_TriggerVoidOut(PlayState* play);
void Play_TriggerRespawn(PlayState* play);
int32_t func_800C0CB8(PlayState* play);
int32_t FrameAdvance_IsEnabled(PlayState* play);
int32_t func_800C0DB4(PlayState* play, Vec3f* pos);
void THGA_Ct(TwoHeadGfxArena* thga, Gfx* start, size_t size);
void THGA_Dt(TwoHeadGfxArena* thga);
uint32_t THGA_IsCrash(TwoHeadGfxArena* thga);
void THGA_Init(TwoHeadGfxArena* thga);
int32_t THGA_GetSize(TwoHeadGfxArena* thga);
Gfx* THGA_GetHead(TwoHeadGfxArena* thga);
void THGA_SetHead(TwoHeadGfxArena* thga, Gfx* start);
Gfx* THGA_GetTail(TwoHeadGfxArena* thga);
Gfx* THGA_AllocStartArray8(TwoHeadGfxArena* thga, uint32_t count);
Gfx* THGA_AllocStart8(TwoHeadGfxArena* thga);
Gfx* THGA_AllocStart8Wrapper(TwoHeadGfxArena* thga);
Gfx* THGA_AllocEnd(TwoHeadGfxArena* thga, size_t size);
Gfx* THGA_AllocEndArray64(TwoHeadGfxArena* thga, uint32_t count);
Gfx* THGA_AllocEnd64(TwoHeadGfxArena* thga);
Gfx* THGA_AllocEndArray16(TwoHeadGfxArena* thga, uint32_t count);
Gfx* THGA_AllocEnd16(TwoHeadGfxArena* thga);
void* THA_GetHead(TwoHeadArena* tha);
void THA_SetHead(TwoHeadArena* tha, void* start);
void* THA_GetTail(TwoHeadArena* tha);
void* THA_AllocStart(TwoHeadArena* tha, size_t size);
void* THA_AllocStart1(TwoHeadArena* tha);
void* THA_AllocEnd(TwoHeadArena* tha, size_t size);
void* THA_AllocEndAlign16(TwoHeadArena* tha, size_t size);
void* THA_AllocEndAlign(TwoHeadArena* tha, size_t size, size_t mask);
int32_t THA_GetSize(TwoHeadArena* tha);
uint32_t THA_IsCrash(TwoHeadArena* tha);
void THA_Init(TwoHeadArena* tha);
void THA_Ct(TwoHeadArena* tha, void* ptr, size_t size);
void THA_Dt(TwoHeadArena* tha);
void func_800C3C20(void);
void func_800C3C80(AudioMgr* audioMgr);
void AudioMgr_HandleRetrace(AudioMgr* audioMgr);
void AudioMgr_HandlePRENMI(AudioMgr* audioMgr);
void AudioMgr_ThreadEntry(void* arg0);
void AudioMgr_Unlock(AudioMgr* audioMgr);
void AudioMgr_Init(AudioMgr* audioMgr, void* stack, OSPri pri, OSId id, SchedContext* sched, IrqMgr* irqMgr);
void TitleSetup_InitImpl(GameState* gameState);
void TitleSetup_Destroy(GameState* gameState);
void TitleSetup_Init(GameState* gameState);
void GameState_Draw(GameState* gameState, GraphicsContext* gfxCtx);
void GameState_SetFrameBuffer(GraphicsContext* gfxCtx);
// ? func_800C49F4(?);
void GameState_ReqPadData(GameState* gameState);
void GameState_Update(GameState* gameState);
void GameState_InitArena(GameState* gameState, size_t size);
void GameState_Realloc(GameState* gameState, size_t size);
void GameState_Init(GameState* gameState, GameStateFunc init, GraphicsContext* gfxCtx);
void GameState_Destroy(GameState* gameState);
GameStateFunc GameState_GetInit(GameState* gameState);
uint32_t GameState_IsRunning(GameState* gameState);
void* GameState_Alloc(GameState* gameState, size_t size, char* file, int32_t line);
void func_800C55D0(GameAlloc* this);
void* GameAlloc_MallocDebug(GameAlloc* this, size_t size, const char* file, int32_t line);
void* GameAlloc_Malloc(GameAlloc* this, size_t size);
void GameAlloc_Free(GameAlloc* this, void* data);
void GameAlloc_Cleanup(GameAlloc* this);
void GameAlloc_Init(GameAlloc* this);
void Graph_FaultClient();
void Graph_DisassembleUCode(Gfx* workBuf);
void Graph_UCodeFaultClient(Gfx* workBuf);
void Graph_InitTHGA(GraphicsContext* gfxCtx);
GameStateOverlay* Graph_GetNextGameState(GameState* gameState);
void Graph_Init(GraphicsContext* gfxCtx);
void Graph_Destroy(GraphicsContext* gfxCtx);
void Graph_TaskSet00(GraphicsContext* gfxCtx);
void Graph_Update(GraphicsContext* gfxCtx, GameState* gameState);
void Graph_ThreadEntry(void*);
void* Graph_Alloc(GraphicsContext* gfxCtx, size_t size);
void* Graph_Alloc2(GraphicsContext* gfxCtx, size_t size);
void Graph_OpenDisps(Gfx** dispRefs, GraphicsContext* gfxCtx, const char* file, int32_t line);
void Graph_CloseDisps(Gfx** dispRefs, GraphicsContext* gfxCtx, const char* file, int32_t line);
Gfx* Graph_GfxPlusOne(Gfx* gfx);
Gfx* Graph_BranchDlist(Gfx* gfx, Gfx* dst);
void* Graph_DlistAlloc(Gfx** gfx, size_t size);
ListAlloc* ListAlloc_Init(ListAlloc* this);
void* ListAlloc_Alloc(ListAlloc* this, size_t size);
void ListAlloc_Free(ListAlloc* this, void* data);
void ListAlloc_FreeAll(ListAlloc* this);
void Main_LogSystemHeap(void);
OSMesgQueue* PadMgr_LockSerialMesgQueue(PadMgr* padmgr);
void PadMgr_UnlockSerialMesgQueue(PadMgr* padmgr, OSMesgQueue* ctrlrqueue);
void PadMgr_LockPadData(PadMgr* padmgr);
void PadMgr_UnlockPadData(PadMgr* padmgr);
void PadMgr_RumbleControl(PadMgr* padmgr);
void PadMgr_RumbleStop(PadMgr* padmgr);
void PadMgr_RumbleReset(PadMgr* padmgr);
void PadMgr_RumbleSet(PadMgr* padmgr, uint8_t* ctrlrRumbles);
void PadMgr_ProcessInputs(PadMgr* padmgr);
void PadMgr_HandleRetraceMsg(PadMgr* padmgr);
void PadMgr_HandlePreNMI(PadMgr* padmgr);
// This function must remain commented out, because it is called incorrectly in
// fault.c (actual bug in game), and the compiler notices and won't compile it
void PadMgr_RequestPadData(PadMgr* padmgr, Input* inputs, int32_t mode);
void PadMgr_Init(PadMgr* padmgr, OSMesgQueue* siIntMsgQ, IrqMgr* irqMgr, OSId id, OSPri priority, void* stack);
void Sched_SwapFrameBuffer(CfbInfo* cfbInfo);
void func_800C84E4(SchedContext* sc, CfbInfo* cfbInfo);
void Sched_HandleReset(SchedContext* sc);
void Sched_HandleStart(SchedContext* sc);
void Sched_QueueTask(SchedContext* sc, OSScTask* task);
void Sched_Yield(SchedContext* sc);
OSScTask* func_800C89D4(SchedContext* sc, OSScTask* task);
int32_t Sched_Schedule(SchedContext* sc, OSScTask** sp, OSScTask** dp, int32_t state);
void func_800C8BC4(SchedContext* sc, OSScTask* task);
uint32_t Sched_IsComplete(SchedContext* sc, OSScTask* task);
void Sched_RunTask(SchedContext* sc, OSScTask* spTask, OSScTask* dpTask);
void Sched_HandleEntry(SchedContext* sc);
void Sched_HandleRetrace(SchedContext* sc);
void Sched_HandleRSPDone(SchedContext* sc);
void Sched_HandleRDPDone(SchedContext* sc);
void Sched_SendEntryMsg(SchedContext* sc);
void Sched_ThreadEntry(void* arg);
void Sched_Init(SchedContext* sc, void* stack, OSPri priority, UNK_TYPE arg3, UNK_TYPE arg4, IrqMgr* irqMgr);
void SysCfb_Init(int32_t n64dd);
uintptr_t SysCfb_GetFbPtr(int32_t idx);
uintptr_t SysCfb_GetFbEnd();
int32_t Math3D_PlaneVsLineSegClosestPoint(float planeAA, float planeAB, float planeAC, float planeADist, float planeBA, float planeBB,
                                      float planeBC, float planeBDist, Vec3f* linePointA, Vec3f* linePointB,
                                      Vec3f* closestPoint);
void Math3D_LineClosestToPoint(Linef* line, Vec3f* pos, Vec3f* closestPoint);
int32_t Math3D_PlaneVsPlaneVsLineClosestPoint(float planeAA, float planeAB, float planeAC, float planeADist, float planeBA,
                                          float planeBB, float planeBC, float planeBDist, Vec3f* point, Vec3f* closestPoint);
void Math3D_LineSplitRatio(Vec3f* v0, Vec3f* v1, float ratio, Vec3f* ret);
float Math3D_Cos(Vec3f* a, Vec3f* b);
int32_t Math3D_CosOut(Vec3f* a, Vec3f* b, float* dst);
void Math3D_Vec3fReflect(Vec3f* vec, Vec3f* normal, Vec3f* reflVec);
int32_t Math3D_PointInSquare2D(float upperLeftX, float lowerRightX, float upperLeftY, float lowerRightY, float x, float y);
float Math3D_Dist1DSq(float a, float b);
float Math3D_Dist2DSq(float x0, float y0, float x1, float y1);
float Math3D_Vec3fMagnitudeSq(Vec3f* vec);
float Math3D_Vec3fMagnitude(Vec3f* vec);
float Math3D_Vec3fDistSq(Vec3f* a, Vec3f* b);
void Math3D_Vec3f_Cross(Vec3f* a, Vec3f* b, Vec3f* ret);
void Math3D_SurfaceNorm(Vec3f* va, Vec3f* vb, Vec3f* vc, Vec3f* normal);
float Math3D_Vec3f_DistXYZ(Vec3f* a, Vec3f* b);
int32_t Math3D_PointRelativeToCubeFaces(Vec3f* point, Vec3f* min, Vec3f* max);
int32_t Math3D_PointRelativeToCubeEdges(Vec3f* point, Vec3f* min, Vec3f* max);
int32_t Math3D_PointRelativeToCubeVertices(Vec3f* point, Vec3f* min, Vec3f* max);
int32_t Math3D_LineVsCube(Vec3f* min, Vec3f* max, Vec3f* a, Vec3f* b);
void Math3D_RotateXZPlane(Vec3f* pointOnPlane, int16_t angle, float* a, float* c, float* d);
void Math3D_DefPlane(Vec3f* va, Vec3f* vb, Vec3f* vc, float* nx, float* ny, float* nz, float* originDist);
float Math3D_UDistPlaneToPos(float nx, float ny, float nz, float originDist, Vec3f* p);
float Math3D_DistPlaneToPos(float nx, float ny, float nz, float originDist, Vec3f* p);
int32_t Math3D_TriChkPointParaYSlopedY(Vec3f* v0, Vec3f* v1, Vec3f* v2, float z, float x);
int32_t Math3D_TriChkPointParaYIntersectDist(Vec3f* v0, Vec3f* v1, Vec3f* v2, float nx, float ny, float nz, float originDist, float z,
                                         float x, float* yIntersect, float chkDist);
int32_t Math3D_TriChkPointParaYIntersectInsideTri(Vec3f* v0, Vec3f* v1, Vec3f* v2, float nx, float ny, float nz, float originDist,
                                              float z, float x, float* yIntersect, float chkDist);
int32_t Math3D_TriChkLineSegParaYIntersect(Vec3f* v0, Vec3f* v1, Vec3f* v2, float nx, float ny, float nz, float originDist, float z,
                                       float x, float* yIntersect, float y0, float y1);
int32_t Math3D_TriChkPointParaYDist(Vec3f* v0, Vec3f* v1, Vec3f* v2, Plane* plane, float z, float x, float chkDist);
int32_t Math3D_TriChkPointParaXIntersect(Vec3f* v0, Vec3f* v1, Vec3f* v2, float nx, float ny, float nz, float originDist, float y,
                                     float z, float* xIntersect);
int32_t Math3D_TriChkLineSegParaXIntersect(Vec3f* v0, Vec3f* v1, Vec3f* v2, float nx, float ny, float nz, float originDist, float y,
                                       float z, float* xIntersect, float x0, float x1);
int32_t Math3D_TriChkPointParaXDist(Vec3f* v0, Vec3f* v1, Vec3f* v2, Plane* plane, float y, float z, float chkDist);
int32_t Math3D_TriChkPointParaZIntersect(Vec3f* v0, Vec3f* v1, Vec3f* v2, float nx, float ny, float nz, float originDist, float x,
                                     float y, float* zIntersect);
int32_t Math3D_TriChkLineSegParaZIntersect(Vec3f* v0, Vec3f* v1, Vec3f* v2, float nx, float ny, float nz, float originDist, float x,
                                       float y, float* zIntersect, float z0, float z1);
int32_t Math3D_TriChkLineSegParaZDist(Vec3f* v0, Vec3f* v1, Vec3f* v2, Plane* plane, float x, float y, float chkDist);
int32_t Math3D_LineSegVsPlane(float nx, float ny, float nz, float originDist, Vec3f* linePointA, Vec3f* linePointB,
                          Vec3f* intersect, int32_t fromFront);
void Math3D_TriNorm(TriNorm* tri, Vec3f* va, Vec3f* vb, Vec3f* vc);
int32_t Math3D_PointDistToLine2D(float x0, float y0, float x1, float y1, float x2, float y2, float* lineLenSq);
int32_t Math3D_LineVsSph(Sphere16* sphere, Linef* line);
int32_t Math3D_TriVsSphIntersect(Sphere16* sphere, TriNorm* tri, Vec3f* intersectPoint);
int32_t Math3D_CylVsLineSeg(Cylinder16* cyl, Vec3f* linePointA, Vec3f* linePointB, Vec3f* intersectA, Vec3f* intersectB);
int32_t Math3D_CylVsTri(Cylinder16* cyl, TriNorm* tri);
int32_t Math3D_CylTriVsIntersect(Cylinder16* cyl, TriNorm* tri, Vec3f* intersect);
int32_t Math3D_SphVsSph(Sphere16* sphereA, Sphere16* sphereB);
int32_t Math3D_SphVsSphOverlap(Sphere16* sphereA, Sphere16* sphereB, float* overlapSize);
int32_t Math3D_SphVsSphOverlapCenter(Sphere16* sphereA, Sphere16* sphereB, float* overlapSize, float* centerDist);
int32_t Math3D_SphVsCylOverlapDist(Sphere16* sph, Cylinder16* cyl, float* overlapSize);
int32_t Math3D_SphVsCylOverlapCenterDist(Sphere16* sph, Cylinder16* cyl, float* overlapSize, float* centerDist);
int32_t Math3D_CylOutsideCyl(Cylinder16* ca, Cylinder16* cb, float* deadSpace);
int32_t Math3D_CylOutsideCylDist(Cylinder16* ca, Cylinder16* cb, float* deadSpace, float* xzDist);
int32_t Math3D_TriVsTriIntersect(TriNorm* ta, TriNorm* tb, Vec3f* intersect);
int32_t Math3D_XZInSphere(Sphere16* sphere, float x, float z);
int32_t Math3D_XYInSphere(Sphere16* sphere, float x, float y);
int32_t Math3D_YZInSphere(Sphere16* sphere, float y, float z);
void Math3D_DrawSphere(PlayState* play, Sphere16* sph);
void Math3D_DrawCylinder(PlayState* play, Cylinder16* cyl);
void Matrix_Init(GameState* gameState);
void Matrix_Push(void);
void Matrix_Pop(void);
void Matrix_Get(MtxF* dest);
void Matrix_Put(MtxF* src);
void Matrix_Mult(MtxF* mf, uint8_t mode);
void Matrix_Translate(float x, float y, float z, uint8_t mode);
void Matrix_Scale(float x, float y, float z, uint8_t mode);
void Matrix_RotateX(float x, uint8_t mode);
void Matrix_RotateY(float y, uint8_t mode);
void Matrix_RotateZ(float z, uint8_t mode);
void Matrix_RotateZYX(int16_t x, int16_t y, int16_t z, uint8_t mode);
void Matrix_TranslateRotateZYX(Vec3f* translation, Vec3s* rotation);
void Matrix_SetTranslateRotateYXZ(float translateX, float translateY, float translateZ, Vec3s* rot);
Mtx* Matrix_MtxFToMtx(MtxF* src, Mtx* dest);
Mtx* Matrix_ToMtx(Mtx* dest, char* file, int32_t line);
Mtx* Matrix_NewMtx(GraphicsContext* gfxCtx, char* file, int32_t line);
Mtx* Matrix_MtxFToNewMtx(MtxF* src, GraphicsContext* gfxCtx);
void Matrix_MultVec3f(Vec3f* src, Vec3f* dest);
void Matrix_MtxFCopy(MtxF* dest, MtxF* src);
void Matrix_MtxToMtxF(Mtx* src, MtxF* dest);
void Matrix_MultVec3fExt(Vec3f* src, Vec3f* dest, MtxF* mf);
void Matrix_Transpose(MtxF* mf);
void Matrix_ReplaceRotation(MtxF* mf);
void Matrix_MtxFToYXZRotS(MtxF* mf, Vec3s* rotDest, int32_t flag);
void Matrix_MtxFToZYXRotS(MtxF* mf, Vec3s* rotDest, int32_t flag);
void Matrix_RotateAxis(float angle, Vec3f* axis, uint8_t mode);
MtxF* Matrix_CheckFloats(MtxF* mf, char* file, int32_t line);
void Matrix_SetTranslateScaleMtx2(Mtx* mtx, float scaleX, float scaleY, float scaleZ, float translateX, float translateY,
                                  float translateZ);
uintptr_t SysUcode_GetUCodeBoot(void);
size_t SysUcode_GetUCodeBootSize(void);
uint32_t SysUcode_GetUCode(void);
uintptr_t SysUcode_GetUCodeData(void);
void func_800D2E30(UnkRumbleStruct* arg0);
void func_800D3140(UnkRumbleStruct* arg0);
void func_800D3178(UnkRumbleStruct* arg0);
void func_800D31A0(void);
void func_800D31F0(void);
void func_800D3210(void);
void IrqMgr_AddClient(IrqMgr* this, IrqMgrClient* c, OSMesgQueue* msgQ);
void IrqMgr_RemoveClient(IrqMgr* this, IrqMgrClient* c);
void IrqMgr_SendMesgForClient(IrqMgr* this, OSMesg msg);
void IrqMgr_JamMesgForClient(IrqMgr* this, OSMesg msg);
void IrqMgr_HandlePreNMI(IrqMgr* this);
void IrqMgr_CheckStack();
void IrqMgr_HandlePRENMI450(IrqMgr* this);
void IrqMgr_HandlePRENMI480(IrqMgr* this);
void IrqMgr_HandlePRENMI500(IrqMgr* this);
void IrqMgr_HandleRetrace(IrqMgr* this);
void IrqMgr_ThreadEntry(void* arg0);
void IrqMgr_Init(IrqMgr* this, void* stack, OSPri pri, uint8_t retraceCount);
void DebugArena_CheckPointer(void* ptr, size_t size, const char* name, const char* action);
void* DebugArena_Malloc(size_t size);
void* DebugArena_MallocDebug(size_t size, const char* file, int32_t line);
void* DebugArena_MallocR(size_t size);
void* DebugArena_MallocRDebug(size_t size, const char* file, int32_t line);
void* DebugArena_Realloc(void* ptr, size_t newSize);
void* DebugArena_ReallocDebug(void* ptr, size_t newSize, const char* file, int32_t line);
void DebugArena_Free(void* ptr);
void DebugArena_FreeDebug(void* ptr, const char* file, int32_t line);
void* DebugArena_Calloc(size_t num, size_t size);
void DebugArena_Display(void);
void DebugArena_GetSizes(uint32_t* outMaxFree, uint32_t* outFree, uint32_t* outAlloc);
void DebugArena_Check(void);
void DebugArena_Init(void* start, size_t size);
void DebugArena_Cleanup(void);
uint8_t DebugArena_IsInitalized(void);
void Fault_SetFB(void*, uint16_t, uint16_t);
void Fault_AddHungupAndCrashImpl(const char*, const char*);
void Fault_AddHungupAndCrash(const char*, uint32_t);
void FaultDrawer_SetOsSyncPrintfEnabled(uint32_t);
void FaultDrawer_DrawRecImpl(int32_t, int32_t, int32_t, int32_t, uint16_t);
void FaultDrawer_DrawChar(char);
int32_t FaultDrawer_ColorToPrintColor(uint16_t);
void FaultDrawer_UpdatePrintColor();
void FaultDrawer_SetForeColor(uint16_t);
void FaultDrawer_SetBackColor(uint16_t);
void FaultDrawer_SetFontColor(uint16_t);
void FaultDrawer_SetCharPad(int8_t, int8_t);
void FaultDrawer_SetCursor(int32_t, int32_t);
void FaultDrawer_FillScreen();
void* FaultDrawer_FormatStringFunc(void*, const char*, uint32_t);
void FaultDrawer_VPrintf(const char*, va_list);
void FaultDrawer_Printf(const char*, ...);
void FaultDrawer_DrawText(int32_t, int32_t, const char*, ...);
void FaultDrawer_SetDrawerFB(void*, uint16_t, uint16_t);
void FaultDrawer_SetInputCallback(void (*)());
void FaultDrawer_SetDefault();
// ? UCodeDisas_TranslateAddr(?);
// ? UCodeDisas_ParseCombineColor(?);
// ? UCodeDisas_ParseCombineAlpha(?);
void UCodeDisas_Init(UCodeDisas*);
void UCodeDisas_Destroy(UCodeDisas*);
// ? UCodeDisas_SetCurUCodeImpl(?);
// ? UCodeDisas_ParseGeometryMode(?);
// ? UCodeDisas_ParseRenderMode(?);
// ? UCodeDisas_PrintVertices(?);
//void UCodeDisas_Disassemble(UCodeDisas*, Gfx*);
void UCodeDisas_RegisterUCode(UCodeDisas* this, int32_t count, UCodeInfo* ucodeArray);
void UCodeDisas_SetCurUCode(UCodeDisas*, void*);
Acmd* AudioSynth_Update(Acmd* cmdStart, int32_t* cmdCnt, int16_t* aiStart, int32_t aiBufLen);
void AudioHeap_DiscardFont(int32_t fontId);
void AudioHeap_DiscardSequence(int32_t seqId);
void AudioHeap_WritebackDCache(void* mem, size_t size);
void* AudioHeap_AllocZeroedAttemptExternal(AudioAllocPool* pool, size_t size);
void* AudioHeap_AllocAttemptExternal(AudioAllocPool* pool, size_t size);
void* AudioHeap_AllocDmaMemory(AudioAllocPool* pool, size_t size);
void* AudioHeap_AllocDmaMemoryZeroed(AudioAllocPool* pool, size_t size);
void* AudioHeap_AllocZeroed(AudioAllocPool* pool, size_t size);
void* AudioHeap_Alloc(AudioAllocPool* pool, size_t size);
void AudioHeap_AllocPoolInit(AudioAllocPool* pool, void* mem, size_t size);
void AudioHeap_PersistentCacheClear(AudioPersistentCache* persistent);
void AudioHeap_TemporaryCacheClear(AudioTemporaryCache* temporary);
void AudioHeap_PopCache(int32_t tableType);
void AudioHeap_InitMainPools(size_t sizeForAudioInitPool);
void* AudioHeap_AllocCached(int32_t tableType, ptrdiff_t size, int32_t cache, int32_t id);
void* AudioHeap_SearchCaches(int32_t tableType, int32_t arg1, int32_t id);
void* AudioHeap_SearchRegularCaches(int32_t tableType, int32_t cache, int32_t id);
void AudioHeap_LoadFilter(int16_t* filter, int32_t filter1, int32_t filter2);
int32_t AudioHeap_ResetStep(void);
void AudioHeap_Init(void);
void* AudioHeap_SearchPermanentCache(int32_t tableType, int32_t id);
void* AudioHeap_AllocPermanent(int32_t tableType, int32_t id, size_t size);
void* AudioHeap_AllocSampleCache(size_t size, int32_t fontId, void* sampleAddr, int8_t medium, int32_t cache);
void AudioHeap_ApplySampleBankCache(int32_t sampleBankId);
void AudioLoad_DecreaseSampleDmaTtls(void);
uintptr_t AudioLoad_DmaSampleData(uintptr_t devAddr, size_t size, int32_t arg2, uint8_t* dmaIndexRef, int32_t medium);
void AudioLoad_InitSampleDmaBuffers(int32_t arg0);
int32_t AudioLoad_IsFontLoadComplete(int32_t fontId);
int32_t AudioLoad_IsSeqLoadComplete(int32_t seqId);
void AudioLoad_SetFontLoadStatus(int32_t fontId, int32_t status);
void AudioLoad_SetSeqLoadStatus(int32_t seqId, int32_t status);
void AudioLoad_SyncLoadSeqParts(int32_t seqId, int32_t arg1);
int32_t AudioLoad_SyncLoadInstrument(int32_t fontId, int32_t instId, int32_t drumId);
void AudioLoad_AsyncLoadSeq(int32_t seqId, int32_t arg1, int32_t retData, OSMesgQueue* retQueue);
void AudioLoad_AsyncLoadSampleBank(int32_t sampleBankId, int32_t arg1, int32_t retData, OSMesgQueue* retQueue);
void AudioLoad_AsyncLoadFont(int32_t fontId, int32_t arg1, int32_t retData, OSMesgQueue* retQueue);
uint8_t* AudioLoad_GetFontsForSequence(int32_t seqId, uint32_t* arg1);
void AudioLoad_DiscardSeqFonts(int32_t seqId);
int32_t AudioLoad_SyncInitSeqPlayer(int32_t playerIdx, int32_t seqId, int32_t arg2);
int32_t AudioLoad_SyncInitSeqPlayerSkipTicks(int32_t playerIdx, int32_t seqId, int32_t arg2);
void AudioLoad_ProcessLoads(int32_t resetStatus);
void AudioLoad_SetDmaHandler(DmaHandler callback);
void AudioLoad_Init(void* heap, size_t heapSize);
void AudioLoad_InitSlowLoads(void);
int32_t AudioLoad_SlowLoadSample(int32_t arg0, int32_t arg1, int8_t* arg2);
int32_t AudioLoad_SlowLoadSeq(int32_t playerIdx, uint8_t* ramAddr, int8_t* arg2);
void AudioLoad_InitAsyncLoads(void);
void AudioLoad_ScriptLoad(int32_t tableType, int32_t arg1, int8_t* arg2);
void AudioLoad_ProcessScriptLoads(void);
void AudioLoad_InitScriptLoads(void);
AudioTask* func_800E4FE0(void);
void Audio_QueueCmdF32(uint32_t arg0, float arg1);
void Audio_QueueCmdS32(uint32_t arg0, int32_t arg1);
void Audio_QueueCmdS8(uint32_t arg0, int8_t arg1);
void Audio_QueueCmdU16(uint32_t arg0, uint16_t arg1);
int32_t Audio_ScheduleProcessCmds(void);
uint32_t func_800E5E20(uint32_t* arg0);
uint8_t* func_800E5E84(int32_t arg0, uint32_t* arg1);
int32_t func_800E5EDC(void);
int32_t func_800E5F88(int32_t arg0);
void Audio_PreNMIInternal(void);
int32_t func_800E6680(void);
uint32_t Audio_NextRandom(void);
void Audio_InitMesgQueues(void);
void Audio_InvalDCache(void* buf, size_t size);
void Audio_WritebackDCache(void* mem, size_t size);
int32_t osAiSetNextBuffer(void* buf, size_t size);
void Audio_InitNoteSub(Note* note, NoteSubEu* sub, NoteSubAttributes* attrs);
void Audio_NoteSetResamplingRate(NoteSubEu* noteSubEu, float resamplingRateInput);
void Audio_NoteInit(Note* note);
void Audio_NoteDisable(Note* note);
void Audio_ProcessNotes(void);
SoundFontSound* Audio_InstrumentGetSound(Instrument* instrument, int32_t semitone);
Instrument* Audio_GetInstrumentInner(int32_t fontId, int32_t instId);
Drum* Audio_GetDrum(int32_t fontId, int32_t drumId);
SoundFontSound* Audio_GetSfx(int32_t fontId, int32_t sfxId);
int32_t Audio_SetFontInstrument(int32_t instrumentType, int32_t fontId, int32_t index, void* value);
void Audio_SeqLayerDecayRelease(SequenceLayer* layer, int32_t target);
void Audio_SeqLayerNoteDecay(SequenceLayer* layer);
void Audio_SeqLayerNoteRelease(SequenceLayer* layer);
int32_t Audio_BuildSyntheticWave(Note* note, SequenceLayer* layer, int32_t waveId);
void Audio_InitSyntheticWave(Note* note, SequenceLayer* layer);
void Audio_InitNoteList(AudioListItem* list);
void Audio_InitNoteLists(NotePool* pool);
void Audio_InitNoteFreeList(void);
void Audio_NotePoolClear(NotePool* pool);
void Audio_NotePoolFill(NotePool* pool, int32_t count);
void Audio_AudioListPushFront(AudioListItem* list, AudioListItem* item);
void Audio_AudioListRemove(AudioListItem* item);
Note* Audio_FindNodeWithPrioLessThan(AudioListItem* list, int32_t limit);
void Audio_NoteInitForLayer(Note* note, SequenceLayer* layer);
void func_800E82C0(Note* note, SequenceLayer* layer);
void Audio_NoteReleaseAndTakeOwnership(Note* note, SequenceLayer* layer);
Note* Audio_AllocNoteFromDisabled(NotePool* pool, SequenceLayer* layer);
Note* Audio_AllocNoteFromDecaying(NotePool* pool, SequenceLayer* layer);
Note* Audio_AllocNoteFromActive(NotePool* pool, SequenceLayer* layer);
Note* Audio_AllocNote(SequenceLayer* layer);
void Audio_NoteInitAll(void);
void Audio_SequenceChannelProcessSound(SequenceChannel* channel, int32_t recalculateVolume, int32_t b);
void Audio_SequencePlayerProcessSound(SequencePlayer* seqPlayer);
float Audio_GetPortamentoFreqScale(Portamento* p);
int16_t Audio_GetVibratoPitchChange(VibratoState* vib);
float Audio_GetVibratoFreqScale(VibratoState* vib);
void Audio_NoteVibratoUpdate(Note* note);
void Audio_NoteVibratoInit(Note* note);
void Audio_NotePortamentoInit(Note* note);
void Audio_AdsrInit(AdsrState* adsr, AdsrEnvelope* envelope, int16_t* volOut);
float Audio_AdsrUpdate(AdsrState* adsr);
void AudioSeq_SequenceChannelDisable(SequenceChannel* channel);
void AudioSeq_SequencePlayerDisableAsFinished(SequencePlayer* seqPlayer);
void AudioSeq_SequencePlayerDisable(SequencePlayer* seqPlayer);
void AudioSeq_AudioListPushBack(AudioListItem* list, AudioListItem* item);
void* AudioSeq_AudioListPopBack(AudioListItem* list);
void AudioSeq_ProcessSequences(int32_t arg0);
void AudioSeq_SkipForwardSequence(SequencePlayer* seqPlayer);
void AudioSeq_ResetSequencePlayer(SequencePlayer* seqPlayer);
void AudioSeq_InitSequencePlayerChannels(int32_t playerIdx);
void AudioSeq_InitSequencePlayers(void);
void func_800ECC04(uint16_t);
void Audio_OcaSetInstrument(uint8_t);
void Audio_OcaSetSongPlayback(int8_t songIdxPlusOne, int8_t playbackState);
void Audio_OcaSetRecordingState(uint8_t);
OcarinaStaff* Audio_OcaGetRecordingStaff(void);
OcarinaStaff* Audio_OcaGetPlayingStaff(void);
OcarinaStaff* Audio_OcaGetDisplayingStaff(void);
void Audio_OcaMemoryGameStart(uint8_t minigameIdx);
int32_t Audio_OcaMemoryGameGenNote(void);
void func_800EE824(void);
void AudioDebug_Draw(GfxPrint* printer);
void AudioDebug_ScrPrt(const int8_t* str, uint16_t num);
void func_800F3054(void);
void Audio_SetSoundProperties(uint8_t bankId, uint8_t entryIdx, uint8_t channelIdx);
void func_800F3F3C(uint8_t);
void func_800F4010(Vec3f* pos, uint16_t sfxId, float);
void Audio_PlaySoundRandom(Vec3f* pos, uint16_t baseSfxId, uint8_t randLim);
void func_800F4138(Vec3f* pos, uint16_t sfxId, float);
void func_800F4190(Vec3f* pos, uint16_t sfxId);
void func_800F436C(Vec3f* pos, uint16_t sfxId, float arg2);
void func_800F4414(Vec3f* pos, uint16_t sfxId, float);
void func_800F44EC(int8_t arg0, int8_t arg1);
void func_800F4524(Vec3f* pos, uint16_t sfxId, int8_t arg2);
void func_800F4254(Vec3f* pos, uint8_t arg1);
void func_800F436C(Vec3f*, uint16_t sfxId, float arg2);
void func_800F4414(Vec3f*, uint16_t sfxId, float arg2);
void Audio_PlaySoundRiver(Vec3f* pos, float freqScale);
void Audio_PlaySoundWaterfall(Vec3f* pos, float freqScale);
void func_800F47BC(void);
void func_800F47FC(void);
void func_800F483C(uint8_t targetVol, uint8_t volFadeTimer);
void func_800F4870(uint8_t);
void func_800F4A54(uint8_t);
void Audio_PlaySoundIncreasinglyTransposed(Vec3f* pos, int16_t sfxId, uint8_t* semitones);
void Audio_ResetIncreasingTranspose(void);
void Audio_PlaySoundTransposed(Vec3f* pos, uint16_t sfxId, int8_t semitone);
void func_800F4C58(Vec3f* pos, uint16_t sfxId, uint8_t);
void func_800F4E30(Vec3f* pos, float);
void Audio_ClearSariaBgm(void);
void Audio_ClearSariaBgmAtPos(Vec3f* pos);
void Audio_PlaySariaBgm(Vec3f* pos, uint16_t seqId, uint16_t distMax);
void Audio_ClearSariaBgm2(void);
void func_800F5510(uint16_t seqId);
void func_800F5550(uint16_t seqId);
void func_800F574C(float arg0, uint8_t arg2);
void func_800F5718(void);
void func_800F5918(void);
void func_800F595C(uint16_t);
void func_800F59E8(uint16_t);
int32_t func_800F5A58(uint8_t);
void func_800F5ACC(uint16_t seqId);
void func_800F5B58(void);
void func_800F5BF0(uint8_t natureAmbienceId);
void Audio_PlayFanfare(uint16_t);
void func_800F5C2C(void);
void func_800F5E18(uint8_t playerIdx, uint16_t seqId, uint8_t fadeTimer, int8_t arg3, int8_t arg4);
void Audio_SetSequenceMode(uint8_t seqMode);
void Audio_SetBgmEnemyVolume(float dist);
void func_800F6268(float dist, uint16_t);
void func_800F64E0(uint8_t arg0);
void func_800F6584(uint8_t arg0);
void Audio_SetEnvReverb(int8_t reverb);
void Audio_SetCodeReverb(int8_t reverb);
void func_800F6700(int8_t outputMode);
void Audio_SetBaseFilter(uint8_t);
void Audio_SetExtraFilter(uint8_t);
void Audio_SetCutsceneFlag(int8_t flag);
void Audio_PlaySoundIfNotInCutscene(uint16_t sfxId);
void func_800F6964(uint16_t);
void func_800F6AB0(uint16_t);
// ? Audio_DisableAllSeq(?);
// ? func_800F6BB8(?);
void Audio_PreNMI();
// ? func_800F6C34(?);
void Audio_SetNatureAmbienceChannelIO(uint8_t channelIdxRange, uint8_t port, uint8_t val);
void Audio_PlayNatureAmbienceSequence(uint8_t natureAmbienceId);
void Audio_Init();
void Audio_InitSound();
void func_800F7170(void);
void func_800F71BC(int32_t arg0);
void Audio_SetSoundBanksMute(uint16_t muteMask);
void Audio_QueueSeqCmdMute(uint8_t channelIdx);
void Audio_ClearBGMMute(uint8_t channelIdx);
void Audio_PlaySoundGeneral(uint16_t sfxId, Vec3f* pos, uint8_t token, float* freqScale, float* a4, int8_t* reverbAdd);
void Audio_ProcessSoundRequest(void);
void Audio_ChooseActiveSounds(uint8_t bankId);
void Audio_PlayActiveSounds(uint8_t bankId);
void Audio_StopSfxByBank(uint8_t bankId);
void func_800F8884(uint8_t, Vec3f*);
void Audio_StopSfxByPosAndBank(uint8_t, Vec3f*);
void Audio_StopSfxByPos(Vec3f*);
void func_800F9280(uint8_t playerIdx, uint8_t seqId, uint8_t arg2, uint16_t fadeTimer);
void Audio_QueueSeqCmd(uint32_t bgmID);
void Audio_StopSfxByPosAndId(Vec3f* pos, uint16_t sfxId);
void Audio_StopSfxByTokenAndId(uint8_t, uint16_t);
void Audio_StopSfxById(uint32_t sfxId);
void Audio_ProcessSoundRequests(void);
void func_800F8F88(void);
uint8_t Audio_IsSfxPlaying(uint32_t sfxId);
void Audio_ResetSounds(void);
void func_800F9474(uint8_t, uint16_t);
void func_800F94FC(uint32_t);
void Audio_ProcessSeqCmd(uint32_t);
void Audio_ProcessSeqCmds(void);
uint16_t func_800FA0B4(uint8_t a0);
int32_t func_800FA11C(uint32_t arg0, uint32_t arg1);
void func_800FA174(uint8_t);
void func_800FA18C(uint8_t, uint8_t);
void Audio_SetVolScale(uint8_t playerIdx, uint8_t scaleIdx, uint8_t targetVol, uint8_t volFadeTimer);
void func_800FA3DC(void);
uint8_t func_800FAD34(void);
void Audio_ResetActiveSequences(void);
void func_800FAEB4(void);
void GfxPrint_SetColor(GfxPrint* this, uint32_t r, uint32_t g, uint32_t b, uint32_t a);
void GfxPrint_SetPosPx(GfxPrint* this, int32_t x, int32_t y);
void GfxPrint_SetPos(GfxPrint* this, int32_t x, int32_t y);
void GfxPrint_SetBasePosPx(GfxPrint* this, int32_t x, int32_t y);
void GfxPrint_Init(GfxPrint* this);
void GfxPrint_Destroy(GfxPrint* this);
void GfxPrint_Open(GfxPrint* this, Gfx* dList);
Gfx* GfxPrint_Close(GfxPrint* this);
int32_t GfxPrint_Printf(GfxPrint* this, const char* fmt, ...);
void func_800FBCE0();
void func_800FBFD8(void);
void* Overlay_AllocateAndLoad(uintptr_t vRomStart, uintptr_t vRomEnd, void* vRamStart, void* vRamEnd);
void MtxConv_F2L(Mtx* m1, MtxF* m2);
void MtxConv_L2F(MtxF* m1, Mtx* m2);
void Overlay_Relocate(void* allocatedVRamAddress, OverlayRelocationSection* overlayInfo, void* vRamAddress);
int32_t Overlay_Load(uintptr_t vRomStart, uintptr_t vRomEnd, void* vRamStart, void* vRamEnd, void* allocatedVRamAddress);
// ? func_800FC800(?);
// ? func_800FC83C(?);
// ? func_800FCAB4(?);
void SystemHeap_Init(void* start, size_t size);
void PadUtils_Init(Input* input);
void func_800FCB70(void);
void PadUtils_ResetPressRel(Input* input);
uint32_t PadUtils_CheckCurExact(Input* input, uint16_t value);
uint32_t PadUtils_CheckCur(Input* input, uint16_t key);
uint32_t PadUtils_CheckPressed(Input* input, uint16_t key);
uint32_t PadUtils_CheckReleased(Input* input, uint16_t key);
uint16_t PadUtils_GetCurButton(Input* input);
uint16_t PadUtils_GetPressButton(Input* input);
int8_t PadUtils_GetCurX(Input* input);
int8_t PadUtils_GetCurY(Input* input);
void PadUtils_SetRelXY(Input* input, int32_t x, int32_t y);
int8_t PadUtils_GetRelXImpl(Input* input);
int8_t PadUtils_GetRelYImpl(Input* input);
int8_t PadUtils_GetRelX(Input* input);
int8_t PadUtils_GetRelY(Input* input);
void PadUtils_UpdateRelXY(Input* input);
int8_t PadUtils_GetCurRX(Input* input);
int8_t PadUtils_GetCurRY(Input* input);
void PadUtils_SetRelRXY(Input* input, int32_t x, int32_t y);
int8_t PadUtils_GetRelRXImpl(Input* input);
int8_t PadUtils_GetRelRYImpl(Input* input);
int8_t PadUtils_GetRelRX(Input* input);
int8_t PadUtils_GetRelRY(Input* input);
void PadUtils_UpdateRelRXY(Input* input);
int32_t PadSetup_Init(OSMesgQueue* mq, uint8_t* outMask, OSContStatus* status);
void SystemArena_CheckPointer(void* ptr, size_t size, const char* name, const char* action);
void* SystemArena_Malloc(size_t size);
void* SystemArena_MallocDebug(size_t size, const char* file, int32_t line);
void* SystemArena_MallocR(size_t size);
void* SystemArena_MallocRDebug(size_t size, const char* file, int32_t line);
void* SystemArena_Realloc(void* ptr, size_t newSize);
void* SystemArena_ReallocDebug(void* ptr, size_t newSize, const char* file, int32_t line);
void SystemArena_Free(void* ptr);
void SystemArena_FreeDebug(void* ptr, const char* file, int32_t line);
void* SystemArena_Calloc(size_t num, size_t size);
void SystemArena_Display(void);
void SystemArena_GetSizes(uint32_t* outMaxFree, uint32_t* outFree, uint32_t* outAlloc);
void SystemArena_Check(void);
void SystemArena_Init(void* start, size_t size);
void SystemArena_Cleanup(void);
uint8_t SystemArena_IsInitalized(void);
uint32_t Rand_Next(void);
void Rand_Seed(uint32_t seed);
float Rand_ZeroOne(void);
float Rand_Centered(void);
void Rand_Seed_Variable(uint32_t* rndNum, uint32_t seed);
uint32_t Rand_Next_Variable(uint32_t* rndNum);
float Rand_ZeroOne_Variable(uint32_t* rndNum);
float Rand_Centered_Variable(uint32_t* rndNum);
uint32_t ArenaImpl_GetFillAllocBlock(Arena* arena);
uint32_t ArenaImpl_GetFillFreeBlock(Arena* arena);
uint32_t ArenaImpl_GetCheckFreeBlock(Arena* arena);
void ArenaImpl_SetFillAllocBlock(Arena* arena);
void ArenaImpl_SetFillFreeBlock(Arena* arena);
void ArenaImpl_SetCheckFreeBlock(Arena* arena);
void ArenaImpl_UnsetFillAllocBlock(Arena* arena);
void ArenaImpl_UnsetFillFreeBlock(Arena* arena);
void ArenaImpl_UnsetCheckFreeBlock(Arena* arena);
void ArenaImpl_SetDebugInfo(ArenaNode* node, const char* file, int32_t line, Arena* arena);
void ArenaImpl_LockInit(Arena* arena);
void ArenaImpl_Lock(Arena* arena);
void ArenaImpl_Unlock(Arena* arena);
ArenaNode* ArenaImpl_GetNextBlock(ArenaNode* node);
ArenaNode* ArenaImpl_GetPrevBlock(ArenaNode* node);
ArenaNode* ArenaImpl_GetLastBlock(Arena* arena);
void __osMallocInit(Arena* arena, void* start, size_t size);
void __osMallocAddBlock(Arena* arena, void* start, ptrdiff_t size);
void ArenaImpl_RemoveAllBlocks(Arena* arena);
void __osMallocCleanup(Arena* arena);
int32_t __osMallocIsInitialized(Arena* arena);
void __osMalloc_FreeBlockTest(Arena* arena, ArenaNode* node);
void* __osMalloc_NoLockDebug(Arena* arena, size_t size, const char* file, int32_t line);
void* __osMallocDebug(Arena* arena, size_t size, const char* file, int32_t line);
void* __osMallocRDebug(Arena* arena, size_t size, const char* file, int32_t line);
void* __osMalloc_NoLock(Arena* arena, size_t size);
void* __osMalloc(Arena* arena, size_t size);
void* __osMallocR(Arena* arena, size_t size);
void __osFree_NoLock(Arena* arena, void* ptr);
void __osFree(Arena* arena, void* ptr);
void __osFree_NoLockDebug(Arena* arena, void* ptr, const char* file, int32_t line);
void __osFreeDebug(Arena* arena, void* ptr, const char* file, int32_t line);
void* __osRealloc(Arena* arena, void* ptr, size_t newSize);
void* __osReallocDebug(Arena* arena, void* ptr, size_t newSize, const char* file, int32_t line);
void ArenaImpl_GetSizes(Arena* arena, uint32_t* outMaxFree, uint32_t* outFree, uint32_t* outAlloc);
void __osDisplayArena(Arena* arena);
void ArenaImpl_FaultClient(Arena* arena);
int32_t __osCheckArena(Arena* arena);
uint8_t func_800FF334(Arena* arena);
int32_t PrintUtils_VPrintf(PrintCallback* pfn, const char* fmt, va_list args);
int32_t PrintUtils_Printf(PrintCallback* pfn, const char* fmt, ...);
void Sleep_Cycles(OSTime cycles);
void Sleep_Nsec(uint32_t nsec);
void Sleep_Usec(uint32_t usec);
void Sleep_Msec(uint32_t ms);
void Sleep_Sec(uint32_t sec);
int32_t osPfsFreeBlocks(OSPfs* pfs, int32_t* leftoverBytes);
void guScale(Mtx* m, float x, float y, float z);
OSTask* _VirtualToPhysicalTask(OSTask* intp);
void osSpTaskLoad(OSTask* task);
void osSpTaskStartGo(OSTask* task);
void __osSiCreateAccessQueue(void);
void __osSiGetAccess(void);
void __osSiRelAccess(void);
int32_t osContInit(OSMesgQueue* mq, uint8_t* ctl_present_bitfield, OSContStatus* status);
void __osContGetInitData(uint8_t* ctl_present_bitfield, OSContStatus* status);
void __osPackRequestData(uint8_t poll);
int32_t osContStartReadData(OSMesgQueue* mq);
void osContGetReadData(OSContPad* pad);
void __osPackReadData(void);
void guPerspectiveF(float mf[4][4], uint16_t* perspNorm, float fovy, float aspect, float near, float far, float scale);
void guPerspective(Mtx* m, uint16_t* perspNorm, float fovy, float aspect, float near, float far, float scale);
int32_t __osSpRawStartDma(int32_t direction, void* devAddr, void* dramAddr, size_t size);
int32_t __osSiRawStartDma(int32_t dir, void* addr);
void osSpTaskYield(void);
int32_t __osPfsGetNextPage(OSPfs* pfs, uint8_t* bank, __OSInode* inode, __OSInodeUnit* page);
int32_t osPfsReadWriteFile(OSPfs* pfs, int32_t fileNo, uint8_t flag, int32_t offset, ptrdiff_t size, uint8_t* data);
int32_t __osPfsGetStatus(OSMesgQueue* queue, int32_t channel);
void __osPfsRequestOneChannel(int32_t channel, uint8_t poll);
void __osPfsGetOneChannelData(int32_t channel, OSContStatus* contData);
void guMtxIdentF(float mf[4][4]);
void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp, float yUp, float zUp);
void guLookAt(Mtx*, float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp, float yUp, float zUp);
int32_t osPfsAllocateFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName, int32_t length, int32_t* fileNo);
int32_t __osPfsDeclearPage(OSPfs* pfs, __OSInode* inode, int32_t fileSizeInPages, int32_t* startPage, uint8_t bank, int32_t* decleared,
                       int32_t* finalPage);
int32_t osStopTimer(OSTimer* timer);
uint16_t __osSumcalc(uint8_t* ptr, int32_t length);
int32_t __osIdCheckSum(uint16_t* ptr, uint16_t* csum, uint16_t* icsum);
int32_t __osRepairPackId(OSPfs* pfs, __OSPackId* badid, __OSPackId* newid);
int32_t __osCheckPackId(OSPfs* pfs, __OSPackId* check);
int32_t __osGetId(OSPfs* pfs);
int32_t __osCheckId(OSPfs* pfs);
int32_t __osPfsRWInode(OSPfs* pfs, __OSInode* inode, uint8_t flag, uint8_t bank);
int32_t osPfsFindFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName, int32_t* fileNo);
int32_t osAfterPreNMI(void);
int32_t osContStartQuery(OSMesgQueue* mq);
void osContGetQuery(OSContStatus* data);
void guLookAtHiliteF(float mf[4][4], LookAt* l, Hilite* h, float xEye, float yEye, float zEye, float xAt, float yAt, float zAt,
                     float xUp, float yUp, float zUp, float xl1, float yl1, float zl1, float xl2, float yl2, float zl2, int32_t hiliteWidth,
                     int32_t hiliteHeight);
void guLookAtHilite(Mtx* m, LookAt* l, Hilite* h, float xEye, float yEye, float zEye, float xAt, float yAt, float zAt, float xUp,
                    float yUp, float zUp, float xl1, float yl1, float zl1, float xl2, float yl2, float zl2, int32_t hiliteWidth,
                    int32_t hiliteHeight);
uint32_t __osSpDeviceBusy(void);
void guMtxIdent(Mtx*);
void guPositionF(float mf[4][4], float rot, float pitch, float yaw, float scale, float x, float y, float z);
void guPosition(Mtx*, float, float, float, float, float, float, float);
OSYieldResult osSpTaskYielded(OSTask* task);
void guRotateF(float m[4][4], float a, float x, float y, float z);
void guRotate(Mtx*, float angle, float x, float y, float z);
int32_t osAiSetFrequency(uint32_t frequency);
OSThread* __osGetActiveQueue(void);
void guNormalize(float* x, float* y, float* z);
uint32_t osDpGetStatus(void);
void osDpSetStatus(uint32_t status);
int32_t osPfsDeleteFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName);
int32_t __osPfsReleasePages(OSPfs* pfs, __OSInode* inode, uint8_t initialPage, uint8_t bank, __OSInodeUnit* finalPage);
void guOrthoF(float[4][4], float, float, float, float, float, float, float);
void guOrtho(Mtx*, float, float, float, float, float, float, float);
void osViSetEvent(OSMesgQueue* mq, OSMesg m, uint32_t retraceCount);
int32_t osPfsIsPlug(OSMesgQueue* mq, uint8_t* pattern);
void __osPfsRequestData(uint8_t poll);
void __osPfsGetInitData(uint8_t* pattern, OSContStatus* contData);
#ifndef __cplusplus
void guS2DInitBg(uObjBg* bg);
#endif
int32_t __osPfsSelectBank(OSPfs* pfs, uint8_t bank);
int32_t osContSetCh(uint8_t ch);
int32_t osPfsFileState(OSPfs* pfs, int32_t fileNo, OSPfsState* state);
int32_t osPfsInitPak(OSMesgQueue* mq, OSPfs* pfs, int32_t channel);
int32_t __osPfsCheckRamArea(OSPfs* pfs);
int32_t osPfsChecker(OSPfs* pfs);
int32_t func_80105788(OSPfs* pfs, __OSInodeCache* cache);
int32_t func_80105A60(OSPfs* pfs, __OSInodeUnit fpage, __OSInodeCache* cache);
uint32_t osAiGetLength(void);
void guTranslate(Mtx* m, float x, float y, float z);
int32_t __osContRamWrite(OSMesgQueue* mq, int32_t channel, uint16_t address, uint8_t* buffer, int32_t force);
int32_t __osContRamRead(OSMesgQueue* ctrlrqueue, int32_t channel, uint16_t addr, uint8_t* data);
uint8_t __osContAddressCrc(uint16_t addr);
uint8_t __osContDataCrc(uint8_t* data);
int32_t osSetTimer(OSTimer* timer, OSTime countdown, OSTime interval, OSMesgQueue* mq, OSMesg msg);
uint32_t __osSpGetStatus(void);
void __osSpSetStatus(uint32_t status);
void osWritebackDCacheAll(void);
OSThread* __osGetCurrFaultedThread(void);
// ? __d_to_ll(?);
// ? __f_to_ll(?);
// ? __d_to_ull(?);
// ? __f_to_ull(?);
// ? __ll_to_d(?);
// ? __ll_to_f(?);
// ? __ull_to_d(?);
// ? __ull_to_f(?);
uint32_t* osViGetCurrentFramebuffer(void);
int32_t __osSpSetPc(void* pc);
void* oot_memmove(void* dest, const void* src, size_t len);
uint8_t Message_ShouldAdvance(PlayState* play);
uint8_t Message_ShouldAdvanceSilent(PlayState* play);
void Message_CloseTextbox(PlayState*);
void Message_StartTextbox(PlayState* play, uint16_t textId, Actor* actor);
void Message_ContinueTextbox(PlayState* play, uint16_t textId);
void func_8010BD58(PlayState* play, uint16_t arg1);
uint8_t Message_GetState(MessageContext* msgCtx);
void GameOver_Init(PlayState* play);
void GameOver_Update(PlayState* play);
void func_80110990(PlayState* play);
void func_801109B0(PlayState* play);
void Message_Init(PlayState* play);
void Regs_InitData(PlayState* play);


void Heaps_Alloc(void);
void Heaps_Free(void);

CollisionHeader* BgCheck_GetCollisionHeader(CollisionContext* colCtx, int32_t bgId);

// Exposing these methods to leverage them from the file select screen to render messages

// #region SOH [General]
int32_t Actor_CalcShouldDrawAndUpdate(PlayState* play, Actor* actor, Vec3f* projectedPos, float projectedW, bool* shouldDraw,
                                 bool* shouldUpdate);

// #region SOH [Rocs Feather]
void func_80838940(Player* this, LinkAnimationHeader* anim, float arg2, PlayState* play, uint16_t sfxId);

// #endregion

#ifdef __cplusplus
#undef this
};
#undef this
#endif

#endif
