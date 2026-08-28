#include <runtime/libultra.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "z64.h"
#include "OTRGlobals.h"
//#include <math.h>

uint32_t osResetType;
uint32_t osTvType = OS_TV_NTSC;
// uint32_t osTvType = OS_TV_PAL;
OSViMode osViModeNtscLan1;
OSViMode osViModeMpalLan1;
OSViMode osViModeFpalLan1;
OSViMode osViModePalLan1;
// AudioContext gAudioContext;
// unk_D_8016E750 D_8016E750[4];
DmaEntry gDmaDataTable[0x60C];
// uint8_t D_80133418;
uint16_t gAudioSEFlagSwapSource[64];
uint16_t gAudioSEFlagSwapTarget[64];
uint8_t gAudioSEFlagSwapMode[64];

// Zbuffer and Color framebuffer
uint16_t D_0E000000[SCREEN_WIDTH * SCREEN_HEIGHT];
uint16_t D_0F000000[SCREEN_WIDTH * SCREEN_HEIGHT];

uint8_t osAppNmiBuffer[2048];

// void gSPTextureRectangle(Gfx* pkt, int32_t xl, int32_t yl, int32_t xh, int32_t yh, uint32_t tile, uint32_t s, int32_t t, uint32_t dsdx, uint32_t dtdy)
//{
//	__gSPTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
// }

OSId osGetThreadId(OSThread* thread) {
    return 0;
}

OSPri osGetThreadPri(OSThread* thread) {
}

void osSetThreadPri(OSThread* thread, OSPri pri) {
}

void osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf, int32_t cmdMsgCnt) {
}

int32_t osPfsFreeBlocks(OSPfs* pfs, int32_t* leftoverBytes) {
    return 0;
}

int32_t osEPiWriteIo(OSPiHandle* handle, uint32_t devAddr, uint32_t data) {
    return 0;
}

int32_t osPfsReadWriteFile(OSPfs* pfs, int32_t fileNo, uint8_t flag, int32_t offset, ptrdiff_t size, uint8_t* data) {
    return 0;
}

int32_t osPfsDeleteFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName) {
    return 0;
}

int32_t osPfsFileState(OSPfs* pfs, int32_t fileNo, OSPfsState* state) {
    return 0;
}

int32_t osPfsInitPak(OSMesgQueue* mq, OSPfs* pfs, int32_t channel) {
    return 0;
}

int32_t __osPfsCheckRamArea(OSPfs* pfs) {
    return 0;
}

int32_t osPfsChecker(OSPfs* pfs) {
    return 0;
}

int32_t osPfsFindFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName, int32_t* fileNo) {
    return 0;
}

int32_t osPfsAllocateFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName, int32_t length, int32_t* fileNo) {
    return 0;
}

OSIntMask osSetIntMask(OSIntMask a) {
    return 0;
}

int32_t osAfterPreNMI(void) {
    return 0;
}

void osCreateThread(OSThread* thread, OSId id, void (*entry)(void*), void* arg, void* sp, OSPri pri) {
}

void osStartThread(OSThread* thread) {
}

void osStopThread(OSThread* thread) {
}

void osDestroyThread(OSThread* thread) {
}

void osWritebackDCache(void* vaddr, int32_t nbytes) {
}

void osInvalICache(void* vaddr, int32_t nbytes) {
}

int32_t osContStartQuery(OSMesgQueue* mq) {
    return 0;
}

void osContGetQuery(OSContStatus* data) {
}

uint32_t __osGetFpcCsr() {
    return 0;
}

void __osSetFpcCsr(uint32_t a0) {
}

int32_t __osDisableInt(void) {
    return 0;
}

void __osRestoreInt(int32_t a0) {
}

OSThread* __osGetActiveQueue(void) {
    return NULL;
}

OSThread* __osGetCurrFaultedThread(void) {
    return NULL;
}

uint32_t osMemSize = 1024 * 1024 * 1024;

int32_t osAiSetFrequency(uint32_t freq) {
    // this is based off the math from the original method
    /*

    int32_t osAiSetFrequency(uint32_t frequency) {
        uint8_t bitrate = { 0 };
        float dacRateF = ((float)osViClock / frequency) + 0.5f;
        uint32_t dacRate = dacRateF;

        if (dacRate < 132) {
            return -1;
        }

        bitrate = (dacRate / 66);
        if (bitrate > 16) {
            bitrate = 16;
        }

        HW_REG(AI_DACRATE_REG, uint32_t) = dacRate - 1;
        HW_REG(AI_BITRATE_REG, uint32_t) = bitrate - 1;
        return osViClock / (int32_t)dacRate;
    }

    */

    // bitrate is unused

    // osViClock comes from
    // #define VI_NTSC_CLOCK 48681812 /* Hz = 48.681812 MHz */
    // int32_t osViClock = VI_NTSC_CLOCK;

    // frequency was originally 32000

    // given all of that, dacRate is
    // (uint32_t)(((float)48681812 / 32000) + 0.5f)
    // which evaluates to 1521 (which is > 132)

    // this leaves us with a final calculation of
    // 48681812 / 1521
    // which evaluates to 32006

    return 32006;
}

void osInvalDCache(void* vaddr, int32_t nbytes) {
}

void osWritebackDCacheAll(void) {
}

int32_t osContSetCh(uint8_t ch) {
    return 0;
}

uint32_t osDpGetStatus(void) {
    return 0;
}

void osDpSetStatus(uint32_t status) {
}

uint32_t __osSpGetStatus() {
    return 0;
}

void __osSpSetStatus(uint32_t status) {
}

OSPiHandle* osDriveRomInit() {
    return NULL;
}

void __osInitialize_common(void) {
}

void __osInitialize_autodetect(void) {
}

void __osExceptionPreamble() {
}

void __osCleanupThread(void) {
}

int32_t _Printf(PrintCallback a, void* arg, const char* fmt, va_list ap) {
    unsigned char buffer[4096];

    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    a(arg, buffer, strlen(buffer));
    return 0;
}

void osSpTaskLoad(OSTask* task) {
}

void osSpTaskStartGo(OSTask* task) {
}

uint32_t osGetMemSize(void) {
    return 1024 * 1024 * 1024;
}

int32_t osEPiReadIo(OSPiHandle* handle, uint32_t devAddr, uint32_t* data) {
    return 0;
}

void osSpTaskYield(void) {
}

int32_t osStopTimer(OSTimer* timer) {
    return 0;
}

OSYieldResult osSpTaskYielded(OSTask* task) {
    return 0;
}

void osViExtendVStart(uint32_t arg0) {
}
