#include "global.h"
#include "vt.h"
#include <stdio.h>

void Fault_AddHungupAndCrashImpl(const char* arg0, const char* arg1) {
   
    *(uint32_t*)0x11111111 = 0; // trigger an exception
}

void Fault_AddHungupAndCrash(const char* filename, uint32_t line) {
    char msg[256];

    snprintf(msg, sizeof(msg), "HungUp %s:%d", filename, line);
    Fault_AddHungupAndCrashImpl(msg, NULL);
}
