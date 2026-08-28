#include <libultraship/libultra.h>
#include "sintable.c"

int16_t sins(uint16_t x) {
    int16_t value = { 0 };

    x >>= 4;

    if (x & 0x400) {
        value = sintable[0x3FF - (x & 0x3FF)];
    } else {
        value = sintable[x & 0x3FF];
    }

    if (x & 0x800) {
        return -value;
    } else {
        return value;
    }
}
