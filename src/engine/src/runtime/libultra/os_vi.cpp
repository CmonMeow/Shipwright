#include "runtime/runtime.h"

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
void __lusViCallback() {
    __OSEventState* es = &__osEventStateTab[OS_EVENT_VI];

    if (es && es->queue) {
        osSendMesg(es->queue, es->msg, OS_MESG_NOBLOCK);
    }

}

void osCreateViManager(OSPri pri) {
#ifdef _WIN32
    static HANDLE timer = nullptr;
    if (timer == nullptr) {
        CreateTimerQueueTimer(&timer, nullptr,
                              [](PVOID, BOOLEAN) { __lusViCallback(); }, nullptr, 16, 16,
                              WT_EXECUTEDEFAULT);
    }
#endif
}

void osViSetEvent(OSMesgQueue* queue, OSMesg mesg, uint32_t c) {

    __OSEventState* es = &__osEventStateTab[OS_EVENT_VI];

    es->queue = queue;
    es->msg = mesg;
}

void osViSwapBuffer(void* a) {
}

void osViSetSpecialFeatures(uint32_t a) {
}

void osViSetMode(OSViMode* a) {
}

void osViBlack(uint8_t a) {
}

void* osViGetNextFramebuffer() {
    return nullptr;
}

void* osViGetCurrentFramebuffer() {
    return nullptr;
}

void osViSetXScale(float a) {
}

void osViSetYScale(float a) {
}
}
