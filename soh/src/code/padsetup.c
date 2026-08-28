#include "global.h"

int32_t PadSetup_Init(OSMesgQueue* mq, uint8_t* outMask, OSContStatus* status) {
    int32_t i;

    *outMask = 0xFF;
    int32_t ret = osContInit(mq, outMask, status);
    if (ret != 0) {
        return ret;
    }
    if (*outMask == 0xFF) {
        if (osContStartQuery(mq) != 0) {
            return 1;
        }

        osRecvMesg(mq, NULL, OS_MESG_BLOCK);
        osContGetQuery(status);

        *outMask = 0;
        for (i = 0; i < 4; i++) {
            switch (status[i].err_no) {
                case 0:
                    if (status[i].type == CONT_TYPE_NORMAL) {
                        *outMask |= 1 << i;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return 0;
}
