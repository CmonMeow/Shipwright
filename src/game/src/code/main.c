#ifdef _WIN32
#include <Windows.h>
#include <locale.h>
#endif

#include "global.h"
#include "vt.h"
#include "stdio.h"
#include "stdlib.h"
#include "port/OTRGlobals.h"

#include <runtime/bridge.h>

int32_t gScreenWidth = SCREEN_WIDTH;
int32_t gScreenHeight = SCREEN_HEIGHT;

PreNmiBuff* gAppNmiBufferPtr;
SchedContext gSchedContext;
PadMgr gPadMgr;
IrqMgr gIrqMgr;
uintptr_t gSegments[NUM_SEGMENTS];
OSThread sGraphThread;
uint8_t sGraphStack[0x1800];
uint8_t sSchedStack[0x600];
uint8_t sAudioStack[0x800];
uint8_t sPadMgrStack[0x500];
uint8_t sIrqMgrStack[0x500];
StackEntry sGraphStackInfo;
StackEntry sSchedStackInfo;
StackEntry sAudioStackInfo;
StackEntry sPadMgrStackInfo;
StackEntry sIrqMgrStackInfo;
AudioMgr gAudioMgr;
OSMesgQueue sSiIntMsgQ;
OSMesg sSiIntMsgBuf[1];

void* gAudioHeap;

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
   
    ClearLog();
    InitOTR(hInstance, showCommand);
    
    gAudioHeap = (uint8_t*)_aligned_malloc(AUDIO_HEAP_SIZE, 0x10);
    void* gSystemHeap = (uint8_t*)_aligned_malloc(SYSTEM_HEAP_SIZE, 0x10);
    
    {
        IrqMgrClient irqClient;
        OSMesgQueue irqMgrMsgQ;
        OSMesg irqMgrMsgBuf[60];

        gScreenWidth = SCREEN_WIDTH;
        gScreenHeight = SCREEN_HEIGHT;
        gAppNmiBufferPtr = (PreNmiBuff*)osAppNmiBuffer;
        PreNmiBuff_Init(gAppNmiBufferPtr);
        SysCfb_Init(0);
        
        __osMallocInit(&gSystemArena, gSystemHeap, SYSTEM_HEAP_SIZE);

        DebugArena_Init(SysCfb_GetFbEnd(), 1024 * 64);
        osCreateMesgQueue(&sSiIntMsgQ, sSiIntMsgBuf, 1);
        osSetEventMesg(5, &sSiIntMsgQ, OS_MESG_PTR(NULL));

        osCreateMesgQueue(&irqMgrMsgQ, irqMgrMsgBuf, 0x3C);
        StackCheck_Init(&sIrqMgrStackInfo, sIrqMgrStack, sIrqMgrStack + sizeof(sIrqMgrStack), 0, 256, "irqmgr");
        IrqMgr_Init(&gIrqMgr, &sGraphStackInfo, Z_PRIORITY_IRQMGR, 1);

        StackCheck_Init(&sSchedStackInfo, sSchedStack, sSchedStack + sizeof(sSchedStack), 0, 256, "sched");
        Sched_Init(&gSchedContext, &sAudioStack, Z_PRIORITY_SCHED, NULL, 1, &gIrqMgr);

        IrqMgr_AddClient(&gIrqMgr, &irqClient, &irqMgrMsgQ);

        StackCheck_Init(&sAudioStackInfo, sAudioStack, sAudioStack + sizeof(sAudioStack), 0, 256, "audio");
        AudioMgr_Init(&gAudioMgr, sAudioStack + sizeof(sAudioStack), Z_PRIORITY_AUDIOMGR, 0xA, &gSchedContext, &gIrqMgr);

        StackCheck_Init(&sPadMgrStackInfo, sPadMgrStack, sPadMgrStack + sizeof(sPadMgrStack), 0, 256, "padmgr");
        PadMgr_Init(&gPadMgr, &sSiIntMsgQ, &gIrqMgr, 7, Z_PRIORITY_PADMGR, &sIrqMgrStack);

        AudioMgr_Unlock(&gAudioMgr);

        StackCheck_Init(&sGraphStackInfo, sGraphStack, sGraphStack + sizeof(sGraphStack), 0, 256, "graph");
        osCreateThread(&sGraphThread, 4, Graph_ThreadEntry, 0, sGraphStack + sizeof(sGraphStack), Z_PRIORITY_GRAPH);
        osStartThread(&sGraphThread);
        osSetThreadPri(0, Z_PRIORITY_SCHED);

        Graph_ThreadEntry(0);

        while (true) {
            int16_t* msg = NULL;
            osRecvMesg(&irqMgrMsgQ, (OSMesg*)&msg, OS_MESG_BLOCK);
            if (msg == NULL) break;
            if (*msg == OS_SC_PRE_NMI_MSG) PreNmiBuff_SetReset(gAppNmiBufferPtr);
        }

        osDestroyThread(&sGraphThread);
        osDpSetStatus(DPC_SET_FREEZE | DPC_SET_FLUSH);
        __osSpSetStatus(SP_SET_HALT | SP_SET_SIG2 | SP_CLR_INTR_BREAK);
    }

    DeinitOTR();

    _aligned_free(gAudioHeap);
    _aligned_free(gSystemHeap);

    return 0;
}
