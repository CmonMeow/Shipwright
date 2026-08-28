#include "global.h"

uint32_t __osSpGetStatus(void) {
    return HW_REG(SP_STATUS_REG, uint32_t);
}
