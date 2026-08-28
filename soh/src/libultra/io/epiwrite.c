#include "global.h"

int32_t osEPiWriteIo(OSPiHandle* handle, uint32_t devAddr, uint32_t data) {
    register int32_t ret;

    __osPiGetAccess();
    ret = __osEPiRawWriteIo(handle, devAddr, data);
    __osPiRelAccess();

    return ret;
}
