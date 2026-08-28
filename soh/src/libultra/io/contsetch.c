#include <libultraship/libultra.h>
#include "global.h"

/*
 * int32_t osContSetCh(uint8_t ch)
 * This function specifies the number of devices for the functions to access when those functions access to multiple
 * direct SI devices.
 */
int32_t osContSetCh(uint8_t ch) {
    __osSiGetAccess();

    if (ch > MAXCONTROLLERS) {
        __osMaxControllers = MAXCONTROLLERS;
    } else {
        __osMaxControllers = ch;
    }

    __osContLastPoll = -2;
    __osSiRelAccess();
    return 0;
}
