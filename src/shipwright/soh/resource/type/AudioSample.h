#pragma once

#include <cstdint>
#include <ship/resource/Resource.h>
#include <libultraship/libultra/types.h>

namespace SOH {
typedef struct {
    /* 0x00 */ uint32_t start;
    /* 0x04 */ uint32_t end;
    /* 0x08 */ uint32_t count;
    /* 0x0C */ char unk_0C[0x4];
    /* 0x10 */ int16_t state[16]; // only exists if count != 0. 8-byte aligned
} AdpcmLoop;                  // size = 0x30 (or 0x10)

typedef struct {
    /* 0x00 */ int32_t order;
    /* 0x04 */ int32_t npredictors;
    /* 0x08 */ int16_t* book; // size 8 * order * npredictors. 8-byte aligned
} AdpcmBook;              // s

typedef struct Sample {
    union {
        struct {
            ///* 0x0 */ uint32_t unk_0 : 1;
            /* 0x0 */ uint32_t codec : 4;  // The state of compression or decompression, See `SampleCodec`
            /* 0x0 */ uint32_t medium : 2; // Medium where sample is currently stored. See `SampleMedium`
            /* 0x0 */ uint32_t unk_bit26 : 1;
            /* 0x0 */ uint32_t isRelocated : 1; // Has the sample header been relocated (offsets to pointers)
        };
        uint32_t asU32;
    };
    /* 0x1 */ uint32_t size; // Size of the sample
    uint32_t fileSize;
    /* 0x4 */ uint8_t* sampleAddr; // Raw sample data. Offset from the start of the sample bank or absolute address to either
                              // rom or ram
    /* 0x8 */ AdpcmLoop*
        loop; // Adpcm loop parameters used by the sample. Offset from the start of the sound font / pointer to ram
    /* 0xC */ AdpcmBook*
        book; // Adpcm book parameters used by the sample. Offset from the start of the sound font / pointer to ram
} Sample;     // size = 0x10

class AudioSample : public Ship::Resource<Sample> {
  public:
    using Resource::Resource;

    AudioSample() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }
    ~AudioSample();

    Sample* GetPointer();
    size_t GetPointerSize();

    Sample sample;
    AdpcmLoop loop;
    AdpcmBook book;
    // Only applies to streamed audio
    float tuning = -1.0f;
};
}; // namespace SOH