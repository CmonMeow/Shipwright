#pragma once
#include "message.h"

typedef struct {
    /* 0x00 */ uint32_t errStatus;
    /* 0x04 */ void* dramAddr;
    /* 0x08 */ void* C2Addr;
    /* 0x0C */ uint32_t sectorSize;
    /* 0x10 */ uint32_t C1ErrNum;
    /* 0x14 */ uint32_t C1ErrSector[4];
} __OSBlockInfo; // size = 0x24

typedef struct {
    /* 0x00 */ uint32_t cmdType;
    /* 0x04 */ uint16_t transferMode;
    /* 0x06 */ uint16_t blockNum;
    /* 0x08 */ int32_t sectorNum;
    /* 0x0C */ uint32_t devAddr;
    /* 0x10 */ uint32_t bmCtlShadow;
    /* 0x14 */ uint32_t seqCtlShadow;
    /* 0x18 */ __OSBlockInfo block[2];
} __OSTranxInfo; // size = 0x60

typedef struct OSPiHandle {
    /* 0x00 */ struct OSPiHandle* next;
    /* 0x04 */ uint8_t type;
    /* 0x05 */ uint8_t latency;
    /* 0x06 */ uint8_t pageSize;
    /* 0x07 */ uint8_t relDuration;
    /* 0x08 */ uint8_t pulse;
    /* 0x09 */ uint8_t domain;
    /* 0x0C */ uint32_t baseAddress;
    /* 0x10 */ uint32_t speed;
    /* 0x14 */ __OSTranxInfo transferInfo;
} OSPiHandle; // size = 0x74

typedef struct {
    /* 0x00 */ uint8_t type;
    /* 0x04 */ uint32_t address;
} OSPiInfo; // size = 0x08

typedef struct {
    /* 0x00 */ uint16_t type;
    /* 0x02 */ uint8_t pri;
    /* 0x03 */ uint8_t status;
    /* 0x04 */ OSMesgQueue* retQueue;
} OSIoMesgHdr; // size = 0x08

typedef struct {
    /* 0x00 */ OSIoMesgHdr hdr;
    /* 0x08 */ void* dramAddr;
    /* 0x0C */ uint32_t devAddr;
    /* 0x10 */ size_t size;
    /* 0x14 */ OSPiHandle* piHandle;
} OSIoMesg; // size = 0x18

#define OS_READ 0   // device -> RDRAM
#define OS_WRITE 1  // device <- RDRAM
#define OS_OTHERS 2 // for disk drive transfers

#define PI_DOMAIN1 0
#define PI_DOMAIN2 1

#define OS_MESG_TYPE_LOOPBACK 10
#define OS_MESG_TYPE_DMAREAD 11
#define OS_MESG_TYPE_DMAWRITE 12
#define OS_MESG_TYPE_VRETRACE 13
#define OS_MESG_TYPE_COUNTER 14
#define OS_MESG_TYPE_EDMAREAD 15
#define OS_MESG_TYPE_EDMAWRITE 16

#define OS_MESG_PRI_NORMAL 0
#define OS_MESG_PRI_HIGH 1

#define DEVICE_TYPE_CART 0 /* ROM cartridge */
#define DEVICE_TYPE_BULK 1 /* ROM bulk */
#define DEVICE_TYPE_64DD 2 /* 64 Disk Drive */
#define DEVICE_TYPE_SRAM 3 /* SRAM */
#define DEVICE_TYPE_INIT 7 /* initial value */
