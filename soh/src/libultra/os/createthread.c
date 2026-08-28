#include "global.h"

OSThread* __osThreadTail[2] = { NULL, (OSThread*)-1 };
OSThread* __osRunQueue = (OSThread*)__osThreadTail;
OSThread* __osActiveQueue = (OSThread*)__osThreadTail;
OSThread* __osRunningThread = NULL;
OSThread* __osFaultedThread = NULL;

void osCreateThread(OSThread* thread, OSId id, void (*entry)(void*), void* arg, void* sp, OSPri pri) {
    register uint32_t prevInt;

    thread->id = id;
    thread->priority = pri;
    thread->next = NULL;
    thread->queue = NULL;
    thread->context.pc = (uint32_t)entry;
    thread->context.a0 = arg;
    thread->context.sp = (uint64_t)(int32_t)sp - 16;
    thread->context.ra = __osCleanupThread;

    OSIntMask mask = OS_IM_ALL;
    thread->context.sr = (mask & OS_IM_CPU) | 2;
    thread->context.rcp = (mask & RCP_IMASK) >> 16;
    thread->context.fpcsr = FPCSR_FS | FPCSR_EV;
    thread->fp = 0;
    thread->state = OS_STATE_STOPPED;
    thread->flags = 0;

    prevInt = __osDisableInt();
    thread->tlnext = __osActiveQueue;
    __osActiveQueue = thread;
    __osRestoreInt(prevInt);
}
