#include "global.h"
#include "ultra64/internal.h"

int32_t osEPiStartDma(OSPiHandle* handle, OSIoMesg* mb, int32_t direction) {
    int32_t ret = { 0 };

    if (!__osPiDevMgr.initialized) {
        return -1;
    }

    mb->piHandle = handle;
    if (direction == OS_READ) {
        mb->hdr.type = 0xF;
    } else {
        mb->hdr.type = 0x10;
    }

    if (mb->hdr.pri == 1) {
        ret = osJamMesg(osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
    } else {
        ret = osSendMesg(osPiGetCmdQueue(), (OSMesg)mb, OS_MESG_NOBLOCK);
    }

    return ret;
}
