#include "global.h"

int32_t __osEPiRawStartDma(OSPiHandle* handle, int32_t direction, uint32_t cartAddr, void* dramAddr, size_t size) {
    int32_t status;

    while (status = HW_REG(PI_STATUS_REG, uint32_t), status & (PI_STATUS_BUSY | PI_STATUS_IOBUSY)) {
        ;
    }

    if (__osCurrentHandle[handle->domain]->type != handle->type) {
        OSPiHandle* curHandle = __osCurrentHandle[handle->domain];

        if (handle->domain == 0) {
            if (curHandle->latency != handle->latency) {
                HW_REG(PI_BSD_DOM1_LAT_REG, uint32_t) = handle->latency;
            }

            if (curHandle->pageSize != handle->pageSize) {
                HW_REG(PI_BSD_DOM1_PGS_REG, uint32_t) = handle->pageSize;
            }

            if (curHandle->relDuration != handle->relDuration) {
                HW_REG(PI_BSD_DOM1_RLS_REG, uint32_t) = handle->relDuration;
            }

            if (curHandle->pulse != handle->pulse) {
                HW_REG(PI_BSD_DOM1_PWD_REG, uint32_t) = handle->pulse;
            }
        } else {
            if (curHandle->latency != handle->latency) {
                HW_REG(PI_BSD_DOM2_LAT_REG, uint32_t) = handle->latency;
            }

            if (curHandle->pageSize != handle->pageSize) {
                HW_REG(PI_BSD_DOM2_PGS_REG, uint32_t) = handle->pageSize;
            }

            if (curHandle->relDuration != handle->relDuration) {
                HW_REG(PI_BSD_DOM2_RLS_REG, uint32_t) = handle->relDuration;
            }

            if (curHandle->pulse != handle->pulse) {
                HW_REG(PI_BSD_DOM2_PWD_REG, uint32_t) = handle->pulse;
            }
        }

        curHandle->type = handle->type;
        curHandle->latency = handle->latency;
        curHandle->pageSize = handle->pageSize;
        curHandle->relDuration = handle->relDuration;
        curHandle->pulse = handle->pulse;
    }

    HW_REG(PI_DRAM_ADDR_REG, void*) = (void*)osVirtualToPhysical(dramAddr);
    HW_REG(PI_CART_ADDR_REG, void*) = (void*)((handle->baseAddress | cartAddr) & 0x1FFFFFFF);

    switch (direction) {
        case OS_READ:
            HW_REG(PI_WR_LEN_REG, uint32_t) = size - 1;
            break;
        case OS_WRITE:
            HW_REG(PI_RD_LEN_REG, uint32_t) = size - 1;
            break;
        default:
            return -1;
            break;
    }

    return 0;
}
