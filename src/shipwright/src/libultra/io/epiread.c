#include "global.h"

int32_t osEPiReadIo(OSPiHandle* handle, uint32_t devAddr, uint32_t* data) {
    register int32_t ret;

    __osPiGetAccess();
    ret = __osEPiRawReadIo(handle, devAddr, data);
    __osPiRelAccess();

    return ret;
}
