#include "global.h"

int32_t __osSiDeviceBusy(void) {
    register uint32_t status = HW_REG(SI_STATUS_REG, uint32_t);

    if (status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_READ_BUSY)) {
        return true;
    } else {
        return false;
    }
}
