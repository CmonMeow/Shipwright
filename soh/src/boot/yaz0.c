#include "global.h"
#include <string.h>

uint8_t sYaz0DataBuffer[0x400];
uintptr_t sYaz0CurDataEnd;
uintptr_t sYaz0CurRomStart;
uint32_t sYaz0CurSize;
uintptr_t sYaz0MaxPtr;

void* Yaz0_FirstDMA(void) {

    sYaz0MaxPtr = sYaz0CurDataEnd - 0x19;

    uint32_t curSize = sYaz0CurDataEnd - (uintptr_t)sYaz0DataBuffer;
    uint32_t dmaSize = (curSize > sYaz0CurSize) ? sYaz0CurSize : curSize;

    DmaMgr_DmaRomToRam(sYaz0CurRomStart, sYaz0DataBuffer, dmaSize);
    sYaz0CurRomStart += dmaSize;
    sYaz0CurSize -= dmaSize;
    return sYaz0DataBuffer;
}

void* Yaz0_NextDMA(void* curSrcPos) {
    uint32_t dmaSize = { 0 };

    uint32_t restSize = sYaz0CurDataEnd - (uintptr_t)curSrcPos;
    uint8_t* dst = (restSize & 7) ? (sYaz0DataBuffer - (restSize & 7)) + 8 : sYaz0DataBuffer;

    memcpy(dst, curSrcPos, restSize);
    dmaSize = (sYaz0CurDataEnd - (uintptr_t)dst) - restSize;
    if (sYaz0CurSize < dmaSize) {
        dmaSize = sYaz0CurSize;
    }

    if (dmaSize != 0) {
        DmaMgr_DmaRomToRam(sYaz0CurRomStart, (uintptr_t)dst + restSize, dmaSize);
        sYaz0CurRomStart += dmaSize;
        sYaz0CurSize -= dmaSize;
        if (!sYaz0CurSize) {
            sYaz0MaxPtr = (uintptr_t)dst + restSize + dmaSize;
        }
    }

    return dst;
}

void Yaz0_DecompressImpl(Yaz0Header* hdr, uint8_t* dst) {
    uint32_t bitIdx = 0;
    uint8_t* src = (uint8_t*)hdr->data;
    uint8_t* dstEnd = dst + hdr->decSize;
    uint32_t chunkHeader = { 0 };
    uint32_t chunkSize;

    do {
        if (bitIdx == 0) {
            if ((sYaz0MaxPtr < (uintptr_t)src) && (sYaz0CurSize != 0)) {
                src = Yaz0_NextDMA(src);
            }

            chunkHeader = *src++;
            bitIdx = 8;
        }

        if (chunkHeader & (1 << 7)) { // uncompressed
            *dst = *src;
            dst++;
            src++;
        } else { // compressed
            uint32_t off = ((*src & 0xF) << 8 | *(src + 1));
            uint32_t nibble = *src >> 4;
            uint8_t* backPtr = dst - off;
            src += 2;

            chunkSize = (nibble == 0)              // N = chunkSize; B = back offset
                            ? (uint32_t)(*src++ + 0x12) // 3 bytes 0B BB NN
                            : nibble + 2;          // 2 bytes NB BB

            do {
                *dst++ = *(backPtr++ - 1);
                chunkSize--;
            } while (chunkSize != 0);
        }
        chunkHeader <<= 1;
        bitIdx--;
    } while (dst != dstEnd);
}

void Yaz0_Decompress(uintptr_t romStart, void* dst, size_t size) {
    sYaz0CurRomStart = romStart;
    sYaz0CurSize = size;
    sYaz0CurDataEnd = sYaz0DataBuffer + sizeof(sYaz0DataBuffer);
    Yaz0_DecompressImpl(Yaz0_FirstDMA(), dst);
}
