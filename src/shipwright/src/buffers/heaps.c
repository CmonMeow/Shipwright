#include "z64.h"
#include <assert.h>
#if !defined(__APPLE__) && !defined(__OpenBSD__)
#include <malloc.h>
#endif
#include <stdlib.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

uint8_t* gAudioHeap;
uint8_t* gSystemHeap;

void Heaps_Alloc(void) {
#ifdef _MSC_VER
    gAudioHeap = (uint8_t*)_aligned_malloc(AUDIO_HEAP_SIZE, 0x10);
    gSystemHeap = (uint8_t*)_aligned_malloc(SYSTEM_HEAP_SIZE, 0x10);
#elif defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L)
    if (posix_memalign((void**)&gAudioHeap, 0x10, AUDIO_HEAP_SIZE) != 0)
        gAudioHeap = NULL;
    if (posix_memalign((void**)&gSystemHeap, 0x10, SYSTEM_HEAP_SIZE) != 0)
        gSystemHeap = NULL;
#else
    gAudioHeap = (uint8_t*)memalign(0x10, AUDIO_HEAP_SIZE);
    gSystemHeap = (uint8_t*)memalign(0x10, SYSTEM_HEAP_SIZE);
#endif

    assert(gAudioHeap != NULL);
    assert(gSystemHeap != NULL);
}

void Heaps_Free(void) {
#ifdef _MSC_VER
    _aligned_free(gAudioHeap);
    _aligned_free(gSystemHeap);
#else
    free(gAudioHeap);
    free(gSystemHeap);
#endif
}
