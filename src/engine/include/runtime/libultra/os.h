#pragma once

#ifndef OS_H
#define OS_H

#include <stdint.h>
#include "controller.h"
#include "message.h"
#include "time.h"
#include "pi.h"
#include "vi.h"

// EEPROM

#define EEPROM_TYPE_4K 0x01
#define EEPROM_TYPE_16K 0x02

#define EEPROM_MAXBLOCKS 64
#define EEP16K_MAXBLOCKS 256
#define EEPROM_BLOCK_SIZE 8

#ifdef __cplusplus
extern "C" {
#endif

void osWritebackDCacheAll();
void osInvalDCache(void* p, int32_t l);
void osInvalICache(void* p, int32_t x);
void osWritebackDCache(void* p, int32_t x);

int32_t osPiStartDma(OSIoMesg* mb, int32_t priority, int32_t direction, uintptr_t devAddr, void* vAddr, size_t nbytes,
                 OSMesgQueue* mq);
void osViSwapBuffer(void*);
void osViBlack(uint8_t active);
void osViFade(uint8_t, uint16_t);
void osViRepeatLine(uint8_t);
void osViSetXScale(float);
void osViSetYScale(float);
void osViSetMode(OSViMode*);
void osViSetEvent(OSMesgQueue*, OSMesg, uint32_t);
void osCreateViManager(OSPri);
void osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf, int32_t cmdMsgCnt);

void osSetTime(OSTime time);
uint64_t osGetTime(void);
uint32_t osGetCount(void);
int32_t osEepromProbe(OSMesgQueue*);
int32_t osEepromRead(OSMesgQueue*, uint8_t, uint8_t*);
int32_t osEepromWrite(OSMesgQueue*, uint8_t, uint8_t*);
int32_t osEepromLongRead(OSMesgQueue*, uint8_t, uint8_t*, int);
int32_t osEepromLongWrite(OSMesgQueue*, uint8_t, uint8_t*, int);

int osSetTimer(OSTimer* t, OSTime countdown, OSTime interval, OSMesgQueue* mq, OSMesg msg);

int32_t osAiSetFrequency(uint32_t freq);
OSPiHandle* osCartRomInit(void);
int32_t osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, int32_t direction);

int32_t osAiSetFrequency(uint32_t);
int32_t osAiSetNextBuffer(void*, size_t);
uint32_t osAiGetLength(void);

#ifdef __cplusplus
};
#endif

#endif
