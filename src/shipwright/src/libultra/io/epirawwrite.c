#include "global.h"

int32_t __osEPiRawWriteIo(OSPiHandle* handle, uint32_t devAddr, uint32_t data) {
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

    HW_REG(handle->baseAddress | devAddr, uint32_t) = data;
    return 0;
}
