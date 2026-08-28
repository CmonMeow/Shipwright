#include "global.h"

void osDpSetStatus(uint32_t status) {
    DPC_STATUS_REG = status;
}
