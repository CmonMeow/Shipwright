#include "global.h"

int32_t __osSpRawStartDma(int32_t direction, void* devAddr, void* dramAddr, size_t size) {
    if (__osSpDeviceBusy()) {
        return -1;
    }
    HW_REG(SP_MEM_ADDR_REG, uint32_t) = (uint32_t)devAddr;
    HW_REG(SP_DRAM_ADDR_REG, uint32_t) = osVirtualToPhysical(dramAddr);
    if (direction == OS_READ) {
        HW_REG(SP_WR_LEN_REG, uint32_t) = size - 1;
    } else {
        HW_REG(SP_RD_LEN_REG, uint32_t) = size - 1;
    }
    return 0;
}
