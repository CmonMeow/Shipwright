#include "global.h"

void __osSpSetStatus(uint32_t status) {
    HW_REG(SP_STATUS_REG, uint32_t) = status;
}
