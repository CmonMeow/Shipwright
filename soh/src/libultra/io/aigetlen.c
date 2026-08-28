#include "global.h"

uint32_t osAiGetLength(void) {
    return HW_REG(AI_LEN_REG, uint32_t);
}
