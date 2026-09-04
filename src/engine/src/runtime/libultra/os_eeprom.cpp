#include "runtime/libultra/eeprom.h"

#include <cstdio>
#include <cstring>

extern "C" {

int32_t osEepromProbe(OSMesgQueue* mq) {
    return EEPROM_TYPE_4K;
}

int32_t osEepromLongRead(OSMesgQueue* mq, uint8_t address, uint8_t* buffer, int32_t length) {
    uint8_t content[512];
    int32_t ret = -1;

    FILE* fp = fopen("default.sav", "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fread(content, 1, 512, fp) == 512) {
        memcpy(buffer, content + address * 8, length);
        ret = 0;
    }
    fclose(fp);

    return ret;
}

int32_t osEepromRead(OSMesgQueue* mq, uint8_t address, uint8_t* buffer) {
    return osEepromLongRead(mq, address, buffer, 8);
}

int32_t osEepromLongWrite(OSMesgQueue* mq, uint8_t address, uint8_t* buffer, int32_t length) {
    uint8_t content[512] = { 0 };
    if (address != 0 || length != 512) {
        osEepromLongRead(mq, 0, content, 512);
    }
    memcpy(content + address * 8, buffer, length);

    FILE* fp = fopen("default.sav", "wb");
    if (fp == NULL) {
        return -1;
    }
    int32_t ret = fwrite(content, 1, 512, fp) == 512 ? 0 : -1;
    fclose(fp);
    return ret;
}

int32_t osEepromWrite(OSMesgQueue* mq, uint8_t address, uint8_t* buffer) {
    return osEepromLongWrite(mq, address, buffer, 8);
}
}
