#include "global.h"
#include "vt.h"

// this should probably go elsewhere but right now viconfig.o is the only object between idle and z_std_dma
OSPiHandle* gCartHandle = 0;
volatile uint8_t gViConfigUseDefault = 1;

void ViConfig_UpdateVi(uint32_t mode) {
    if (mode != 0) {
        osSyncPrintf(VT_COL(YELLOW, BLACK) "osViSetYScale1(%f);\n" VT_RST, 1.0f);

        if (osTvType == OS_TV_PAL) {
            osViSetMode(&osViModePalLan1);
        }

        osViSetYScale(1.0f);
    }

    gViConfigUseDefault = mode;
}

void ViConfig_UpdateBlack(void) {
    if (gViConfigUseDefault != 0) {
        osViBlack(1);
    } else {
        osViBlack(0);
    }
}
