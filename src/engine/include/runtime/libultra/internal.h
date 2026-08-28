#pragma once

#include "pi.h"

typedef struct {
    /* 0x00 */ uint32_t initialized;
    /* 0x04 */ OSThread* mgrThread;
    /* 0x08 */ OSMesgQueue* cmdQueue;
    /* 0x0C */ OSMesgQueue* eventQueue;
    /* 0x10 */ OSMesgQueue* accessQueue;
    /* 0x14 */ int32_t (*piDmaCallback)(int32_t, uint32_t, void*, size_t);
    /* 0x18 */ int32_t (*epiDmaCallback)(OSPiHandle*, int32_t, uint32_t, void*, size_t);
} OSMgrArgs; // size = 0x1C

typedef struct {
    /* 0x00 */ OSMesgQueue* queue;
    /* 0x04 */ OSMesg msg;
} __OSEventState; // size = 0x08

#ifdef __cplusplus
extern "C" {
#endif
extern OSMgrArgs __osPiDevMgr;
extern __OSEventState __osEventStateTab[];
#ifdef __cplusplus
}
#endif
