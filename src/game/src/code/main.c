#include "global.h"
#include "vt.h"
#include "stdio.h"
#include "stdlib.h"

int32_t gScreenWidth = SCREEN_WIDTH;
int32_t gScreenHeight = SCREEN_HEIGHT;

PreNmiBuff* gAppNmiBufferPtr;
SchedContext gSchedContext;
IrqMgr gIrqMgr;
uintptr_t gSegments[NUM_SEGMENTS];
uint8_t sGraphStack[0x1800];
uint8_t sSchedStack[0x600];
uint8_t sAudioStack[0x800];
uint8_t sIrqMgrStack[0x500];
StackEntry sGraphStackInfo;
StackEntry sSchedStackInfo;
StackEntry sAudioStackInfo;
StackEntry sIrqMgrStackInfo;
AudioMgr gAudioMgr;

void* gAudioHeap;
static void* sSystemHeap;
static IrqMgrClient sIrqClient;
static OSMesgQueue sIrqMgrMsgQ;
static OSMesg sIrqMgrMsgBuf[60];

void Game_Initialize(void) {
    gAudioHeap = (uint8_t*)_aligned_malloc(AUDIO_HEAP_SIZE, 0x10);
    sSystemHeap = (uint8_t*)_aligned_malloc(SYSTEM_HEAP_SIZE, 0x10);

    gScreenWidth = SCREEN_WIDTH;
    gScreenHeight = SCREEN_HEIGHT;
    gAppNmiBufferPtr = (PreNmiBuff*)osAppNmiBuffer;
    PreNmiBuff_Init(gAppNmiBufferPtr);
    SysCfb_Init(0);

    __osMallocInit(&gSystemArena, sSystemHeap, SYSTEM_HEAP_SIZE);

    DebugArena_Init(SysCfb_GetFbEnd(), 1024 * 64);
    osCreateMesgQueue(&sIrqMgrMsgQ, sIrqMgrMsgBuf, 0x3C);
    StackCheck_Init(&sIrqMgrStackInfo, sIrqMgrStack, sIrqMgrStack + sizeof(sIrqMgrStack), 0, 256, "irqmgr");
    IrqMgr_Init(&gIrqMgr, &sGraphStackInfo, Z_PRIORITY_IRQMGR, 1);

    StackCheck_Init(&sSchedStackInfo, sSchedStack, sSchedStack + sizeof(sSchedStack), 0, 256, "sched");
    Sched_Init(&gSchedContext, &sAudioStack, Z_PRIORITY_SCHED, NULL, 1, &gIrqMgr);

    IrqMgr_AddClient(&gIrqMgr, &sIrqClient, &sIrqMgrMsgQ);

    StackCheck_Init(&sAudioStackInfo, sAudioStack, sAudioStack + sizeof(sAudioStack), 0, 256, "audio");
    AudioMgr_Init(&gAudioMgr, sAudioStack + sizeof(sAudioStack), Z_PRIORITY_AUDIOMGR, 0xA, &gSchedContext, &gIrqMgr);

    AudioMgr_Unlock(&gAudioMgr);

    StackCheck_Init(&sGraphStackInfo, sGraphStack, sGraphStack + sizeof(sGraphStack), 0, 256, "graph");
    osSetThreadPri(0, Z_PRIORITY_SCHED);
}

void Game_Shutdown(void) {
    osDpSetStatus(DPC_SET_FREEZE | DPC_SET_FLUSH);
    __osSpSetStatus(SP_SET_HALT | SP_SET_SIG2 | SP_CLR_INTR_BREAK);

    _aligned_free(gAudioHeap);
    gAudioHeap = NULL;
    _aligned_free(sSystemHeap);
    sSystemHeap = NULL;
}
