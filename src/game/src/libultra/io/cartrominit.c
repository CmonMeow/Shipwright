#include "global.h"

OSPiHandle __CartRomHandle;

OSPiHandle* osCartRomInit(void) {
    register uint32_t a;
    register int32_t status;
    register uint32_t prevInt;
    register uint32_t lastLatency;
    register uint32_t lastPageSize;
    register uint32_t lastRelDuration;
    register uint32_t lastPulse;

    static uint32_t D_8000AF10 = 1;

    __osPiGetAccess();

    if (!D_8000AF10) {
        __osPiRelAccess();
        return &__CartRomHandle;
    }

    D_8000AF10 = 0;
    __CartRomHandle.type = DEVICE_TYPE_CART;
    __CartRomHandle.baseAddress = 0xB0000000;
    __CartRomHandle.domain = PI_DOMAIN1;
    __CartRomHandle.speed = 0;
    bzero(&__CartRomHandle.transferInfo, sizeof(__OSTranxInfo));

    while (status = HW_REG(PI_STATUS_REG, uint32_t), status & (PI_STATUS_BUSY | PI_STATUS_IOBUSY)) {
        ;
    }

    lastLatency = HW_REG(PI_BSD_DOM1_LAT_REG, uint32_t);
    lastPageSize = HW_REG(PI_BSD_DOM1_PGS_REG, uint32_t);
    lastRelDuration = HW_REG(PI_BSD_DOM1_RLS_REG, uint32_t);
    lastPulse = HW_REG(PI_BSD_DOM1_PWD_REG, uint32_t);

    HW_REG(PI_BSD_DOM1_LAT_REG, uint32_t) = 0xFF;
    HW_REG(PI_BSD_DOM1_PGS_REG, uint32_t) = 0;
    HW_REG(PI_BSD_DOM1_RLS_REG, uint32_t) = 3;
    HW_REG(PI_BSD_DOM1_PWD_REG, uint32_t) = 0xFF;

    a = HW_REG(__CartRomHandle.baseAddress, uint32_t);
    __CartRomHandle.latency = a & 0xFF;
    __CartRomHandle.pageSize = (a >> 0x10) & 0xF;
    __CartRomHandle.relDuration = (a >> 0x14) & 0xF;
    __CartRomHandle.pulse = (a >> 8) & 0xFF;

    HW_REG(PI_BSD_DOM1_LAT_REG, uint32_t) = lastLatency;
    HW_REG(PI_BSD_DOM1_PGS_REG, uint32_t) = lastPageSize;
    HW_REG(PI_BSD_DOM1_RLS_REG, uint32_t) = lastRelDuration;
    HW_REG(PI_BSD_DOM1_PWD_REG, uint32_t) = lastPulse;

    prevInt = __osDisableInt();
    __CartRomHandle.next = __osPiTable;
    __osPiTable = &__CartRomHandle;
    __osRestoreInt(prevInt);
    __osPiRelAccess();

    return &__CartRomHandle;
}
