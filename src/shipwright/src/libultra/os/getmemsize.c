#include "global.h"

#define STEP 0x100000

uint32_t osGetMemSize(void) {
    size_t size = 0x400000;

    while (size < 0x800000) {
        uint32_t* ptr = (uint32_t*)(0xA0000000 + size);

        uint32_t data0 = *ptr;
        uint32_t data1 = ptr[STEP / 4 - 1];

        *ptr ^= ~0;
        ptr[STEP / 4 - 1] ^= ~0;

        if ((*ptr != (data0 ^ ~0)) || (ptr[STEP / 4 - 1] != (data1 ^ ~0))) {
            return size;
        }

        *ptr = data0;
        ptr[STEP / 4 - 1] = data1;

        size += STEP;
    }

    return size;
}
