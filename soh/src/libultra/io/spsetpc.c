#include "global.h"

int32_t __osSpSetPc(void* pc) {
    register uint32_t spStatus = HW_REG(SP_STATUS_REG, uint32_t);

    if (!(spStatus & SP_STATUS_HALT)) {
        return -1;
    } else {
        HW_REG(SP_PC_REG, void*) = pc;
    }

    return 0;
}
