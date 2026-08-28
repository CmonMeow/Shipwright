#include "global.h"

//! Note that this is not the same as the original libultra
//! osAiSetNextBuffer, see comments in the function

int32_t osAiSetNextBuffer(void* buf, size_t size) {
    static uint8_t D_80130500 = false;
    uintptr_t bufAdjusted = (uintptr_t)buf;
    int32_t status = { 0 };

    if (D_80130500) {
        bufAdjusted = (uintptr_t)buf - 0x2000;
    }
    if ((((uintptr_t)buf + size) & 0x1FFF) == 0) {
        D_80130500 = true;
    } else {
        D_80130500 = false;
    }

    // Originally a call to __osAiDeviceBusy
    status = HW_REG(AI_STATUS_REG, int32_t);
    if (status & AI_STATUS_AI_FULL) {
        return -1;
    }

    // OS_K0_TO_PHYSICAL replaces osVirtualToPhysical, this replacement
    // assumes that only KSEG0 addresses are given
    HW_REG(AI_DRAM_ADDR_REG, uint32_t) = OS_K0_TO_PHYSICAL(bufAdjusted);
    HW_REG(AI_LEN_REG, uint32_t) = size;
    return 0;
}
