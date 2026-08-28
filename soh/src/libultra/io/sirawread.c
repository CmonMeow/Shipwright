#include "global.h"

int32_t __osSiRawReadIo(void* devAddr, uint32_t* dst) {
    if (__osSiDeviceBusy()) {
        return -1;
    }
    *dst = HW_REG((uintptr_t)devAddr, uint32_t);
    return 0;
}
