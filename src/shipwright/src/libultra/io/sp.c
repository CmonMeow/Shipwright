#include "global.h"

uint32_t __osSpDeviceBusy(void) {
    register uint32_t status = HW_REG(SP_STATUS_REG, uint32_t);

    if (status & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL | SP_STATUS_IO_FULL)) {
        return 1;
    }
    return 0;
}
