#include "global.h"

int16_t coss(uint16_t angle) {
    return sins(angle + 0x4000);
}
