#include <libultraship/libultra.h>
#include "global.h"

int32_t osPfsFindFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName, int32_t* fileNo) {
    int32_t j;
    int32_t i;
    __OSDir dir;
    int32_t ret = 0;

    if (!(pfs->status & PFS_INITIALIZED)) {
        return PFS_ERR_INVALID;
    }

    if ((ret = __osCheckId(pfs)) != 0) {
        return ret;
    }

    for (j = 0; j < pfs->dir_size; j++) {
        if ((ret = __osContRamRead(pfs->queue, pfs->channel, pfs->dir_table + j, (uint8_t*)&dir)) != 0) {
            return ret;
        }
        if ((ret = __osPfsGetStatus(pfs->queue, pfs->channel)) != 0) {
            return ret;
        }

        if ((dir.company_code == companyCode) && (dir.game_code == gameCode)) {
            int32_t err = 0;
            if (gameName != 0) {
                for (i = 0; i < PFS_FILE_NAME_LEN; i++) {
                    if (dir.game_name[i] != gameName[i]) {
                        err = 1;
                        break;
                    }
                }
            }
            if ((extName != 0) && (err == 0)) {
                for (i = 0; i < PFS_FILE_EXT_LEN; i++) {
                    if (dir.ext_name[i] != extName[i]) {
                        err = 1;
                        break;
                    }
                }
            }
            if (err == 0) {
                *fileNo = j;
                return ret;
            }
        }
    }
    *fileNo = -1;
    return PFS_ERR_INVALID;
}
