#include "global.h"

uint32_t osVirtualToPhysical(void* vaddr) {
    if ((uint32_t)vaddr >= 0x80000000 && (uint32_t)vaddr < 0xA0000000) {
        return (uint32_t)vaddr & 0x1FFFFFFF;
    }

    if ((uint32_t)vaddr >= 0xA0000000 && (uint32_t)vaddr < 0xC0000000) {
        return (uint32_t)vaddr & 0x1FFFFFFF;
    }

    return __osProbeTLB(vaddr);
}
