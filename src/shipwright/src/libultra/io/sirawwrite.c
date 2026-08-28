#include "global.h"

int32_t __osSiRawWriteIo(void* devAddr, uint32_t val) {
    if (__osSiDeviceBusy()) {
        return -1;
    }
    HW_REG((uint32_t)devAddr, uint32_t) = val;
    return 0;
}
