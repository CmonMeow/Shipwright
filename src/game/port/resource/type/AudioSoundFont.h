#pragma once

#include <cstdint>
#include <vector>
#include <engine/resource/Resource.h>
#include "port/resource/type/AudioSample.h"
#include <runtime/libultra/types.h>

namespace SOH {

typedef struct {
    /* 0x0 */ int16_t delay;
    /* 0x2 */ int16_t arg;
} AdsrEnvelope; // size = 0x4

typedef struct {
    /* 0x00 */ Sample* sample;
    /* 0x04 */ union {
        uint32_t tuningAsU32;
        float tuning; // frequency scale factor
    };
} SoundFontSound; // size = 0x8

typedef struct {
    /* 0x00 */ uint8_t loaded;
    /* 0x01 */ uint8_t normalRangeLo;
    /* 0x02 */ uint8_t normalRangeHi;
    /* 0x03 */ uint8_t releaseRate;
    /* 0x04 */ AdsrEnvelope* envelope;
    /* 0x08 */ SoundFontSound lowNotesSound;
    /* 0x10 */ SoundFontSound normalNotesSound;
    /* 0x18 */ SoundFontSound highNotesSound;
} Instrument; // size = 0x20

typedef struct {
    /* 0x00 */ uint8_t releaseRate;
    /* 0x01 */ uint8_t pan;
    /* 0x02 */ uint8_t loaded;
    /* 0x04 */ SoundFontSound sound;
    /* 0x14 */ AdsrEnvelope* envelope;
} Drum; // size = 0x14

typedef struct {
    /* 0x00 */ uint8_t numInstruments;
    /* 0x01 */ uint8_t numDrums;
    /* 0x02 */ uint8_t sampleBankId1;
    /* 0x03 */ uint8_t sampleBankId2;
    /* 0x04 */ uint16_t numSfx;
    /* 0x08 */ Instrument** instruments;
    /* 0x0C */ Drum** drums;
    /* 0x10 */ SoundFontSound* soundEffects;
    int32_t fntIndex;
} SoundFont; // size = 0x14

class AudioSoundFont : public Engine::Resource<SoundFont> {
  public:
    using Resource::Resource;

    AudioSoundFont() : Resource(std::shared_ptr<Engine::ResourceInitData>()) {
    }
    ~AudioSoundFont();

    SoundFont* GetPointer();
    size_t GetPointerSize();

    int8_t medium;
    int8_t cachePolicy;
    uint16_t data1;
    uint16_t data2;
    uint16_t data3;

    std::vector<Drum*> drumAddresses;
    std::vector<std::vector<AdsrEnvelope>> drumEnvelopeArrays;

    std::vector<Instrument*> instrumentAddresses;

    std::vector<SoundFontSound> soundEffects;

    SoundFont soundFont;
};
}; // namespace SOH
