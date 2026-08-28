#ifndef Z64_AUDIO_H
#define Z64_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <engine/utils/binarytools/endianness.h>

#define MK_CMD(b0,b1,b2,b3) ((((b0) & 0xFF) << 0x18) | (((b1) & 0xFF) << 0x10) | (((b2) & 0xFF) << 0x8) | (((b3) & 0xFF) << 0))

#define NO_LAYER ((SequenceLayer*)(-1))

#define TATUMS_PER_BEAT 48

#define IS_SEQUENCE_CHANNEL_VALID(ptr) ((uintptr_t)(ptr) != (uintptr_t)&gAudioContext.sequenceChannelNone)

#define MAX_CHANNELS_PER_BANK 3

#define ADSR_DISABLE 0
#define ADSR_HANG -1
#define ADSR_GOTO -2
#define ADSR_RESTART -3

#define AIBUF_LEN 0x580

#define CALC_RESAMPLE_FREQ(sampleRate) ((float)sampleRate / (int32_t)gAudioContext.audioBufferParameters.frequency)

//#define MAX_SEQUENCES 0x800
extern size_t sequenceMapSize;
extern size_t fontMapSize;
extern char** fontMap;

#define MAX_AUTHENTIC_SEQID 110

typedef enum {
    /* 0 */ ADSR_STATE_DISABLED,
    /* 1 */ ADSR_STATE_INITIAL,
    /* 2 */ ADSR_STATE_START_LOOP,
    /* 3 */ ADSR_STATE_LOOP,
    /* 4 */ ADSR_STATE_FADE,
    /* 5 */ ADSR_STATE_HANG,
    /* 6 */ ADSR_STATE_DECAY,
    /* 7 */ ADSR_STATE_RELEASE,
    /* 8 */ ADSR_STATE_SUSTAIN
} AdsrStatus;

typedef enum {
    /* 0 */ MEDIUM_RAM,
    /* 1 */ MEDIUM_UNK,
    /* 2 */ MEDIUM_CART,
    /* 3 */ MEDIUM_DISK_DRIVE
} SampleMedium;

typedef enum {
    /* 0 */ CODEC_ADPCM,
    /* 1 */ CODEC_S8,
    /* 2 */ CODEC_S16_INMEMORY,
    /* 3 */ CODEC_SMALL_ADPCM,
    /* 4 */ CODEC_REVERB,
    /* 5 */ CODEC_S16,
    /* 6 */ CODEC_OPUS,
} SampleCodec;

typedef enum {
    /* 0 */ SEQUENCE_TABLE,
    /* 1 */ FONT_TABLE,
    /* 2 */ SAMPLE_TABLE
} SampleBankTableType;

typedef enum {
    /* 0 */ CACHE_TEMPORARY,
    /* 1 */ CACHE_PERSISTENT,
    /* 2 */ CACHE_EITHER,
    /* 3 */ CACHE_PERMANENT
} AudioCacheType;

typedef int32_t (*DmaHandler)(OSPiHandle* handle, OSIoMesg* mb, int32_t direction);

struct Note;
struct NotePool;
struct SequenceChannel;
struct SequenceLayer;

typedef struct AudioListItem {
    // A node in a circularly linked list. Each node is either a head or an item:
    // - Items can be either detached (prev = NULL), or attached to a list.
    //   'value' points to something of interest.
    // - List heads are always attached; if a list is empty, its head points
    //   to itself. 'count' contains the size of the list.
    // If the list holds notes, 'pool' points back to the pool where it lives.
    // Otherwise, that member is NULL.
    /* 0x00 */ struct AudioListItem* prev;
    /* 0x04 */ struct AudioListItem* next;
    /* 0x08 */ union {
                   void* value; // either Note* or SequenceLayer*
                   int32_t count;
               } u;
    /* 0x0C */ struct NotePool* pool;
} AudioListItem; // size = 0x10

typedef struct NotePool {
    /* 0x00 */ AudioListItem disabled;
    /* 0x10 */ AudioListItem decaying;
    /* 0x20 */ AudioListItem releasing;
    /* 0x30 */ AudioListItem active;
} NotePool;

// Pitch sliding by up to one octave in the positive direction. Negative
// direction is "supported" by setting extent to be negative. The code
// exterpolates exponentially in the wrong direction in that case, but that
// doesn't prevent seqplayer from doing it, AFAICT.
typedef struct {
    /* 0x00 */ uint8_t mode; // bit 0x80 denotes something; the rest are an index 0-5
    /* 0x02 */ uint16_t cur;
    /* 0x04 */ uint16_t speed;
    /* 0x08 */ float extent;
} Portamento; // size = 0xC

typedef struct {
    /* 0x0 */ int16_t delay;
    /* 0x2 */ int16_t arg;
} AdsrEnvelope; // size = 0x4

typedef struct AdpcmLoop {
    /* 0x00 */ uint32_t start;
    /* 0x04 */ uint32_t loopEnd;   // numSamples position into the sample where the loop ends
    /* 0x08 */ uint32_t count;     // The number of times the loop is played before the sound completes. Setting count to -1
    // indicates that the loop should play indefinitely.
    /* 0x0C */ uint32_t sampleEnd; // total number of int16_t-samples in the sample audio clip
    /* 0x10 */ int16_t predictorState[16]; // only exists if count != 0. 8-byte aligned
} AdpcmLoop;    // size = 0x30 (or 0x10)

typedef struct {
    /* 0x00 */ int32_t order;
    /* 0x04 */ int32_t npredictors;
    /* 0x08 */ int16_t* book; // size 8 * order * npredictors. 8-byte aligned
} AdpcmBook; // size >= 0x8

typedef struct SoundFontSample {
    union {
        struct {
            ///* 0x0 */ uint32_t unk_0 : 1;
            /* 0x0 */ uint32_t codec : 4; // The state of compression or decompression, See `SampleCodec`
            /* 0x0 */ uint32_t medium : 2; // Medium where sample is currently stored. See `SampleMedium`
            /* 0x0 */ uint32_t unk_bit26 : 1;
            /* 0x0 */ uint32_t isRelocated : 1; // Has the sample header been relocated (offsets to pointers)

        };
        uint32_t asU32;
    };
    /* 0x1 */ uint32_t size;  // Size of the sample
    uint32_t fileSize;
    /* 0x4 */ uint8_t* sampleAddr; // Raw sample data. Offset from the start of the sample bank or absolute address to either rom or ram
    /* 0x8 */ AdpcmLoop* loop; // Adpcm loop parameters used by the sample. Offset from the start of the sound font / pointer to ram
    /* 0xC */ AdpcmBook* book; // Adpcm book parameters used by the sample. Offset from the start of the sound font / pointer to ram
} SoundFontSample; // size = 0x10

typedef struct {
    /* 0x00 */ SoundFontSample* sample;
    /* 0x04 */ union {
        uint32_t tuningAsU32;
        float tuning;// frequency scale factor
    };
} SoundFontSound; // size = 0x8

typedef struct {
    /* 0x00 */ int16_t numSamplesAfterDownsampling; // never read
    /* 0x02 */ int16_t chunkLen; // never read
    /* 0x04 */ int16_t* toDownsampleLeft;
    /* 0x08 */ int16_t* toDownsampleRight; // data pointed to by left and right are adjacent in memory
    /* 0x0C */ int32_t startPos; // start pos in ring buffer
    /* 0x10 */ int16_t lengthA; // first length in ring buffer (from startPos, at most until end)
    /* 0x12 */ int16_t lengthB; // second length in ring buffer (from pos 0)
    /* 0x14 */ uint16_t unk_14;
    /* 0x16 */ uint16_t unk_16;
    /* 0x18 */ uint16_t unk_18;
} ReverbRingBufferItem; // size = 0x1C

typedef struct {
    /* 0x000 */ uint8_t resampleFlags;
    /* 0x001 */ uint8_t useReverb;
    /* 0x002 */ uint8_t framesToIgnore;
    /* 0x003 */ uint8_t curFrame;
    /* 0x004 */ uint8_t downsampleRate;
    /* 0x005 */ int8_t unk_05;
    /* 0x006 */ uint16_t windowSize;
    /* 0x008 */ int16_t unk_08;
    /* 0x00A */ int16_t unk_0A;
    /* 0x00C */ uint16_t unk_0C;
    /* 0x00E */ uint16_t unk_0E;
    /* 0x010 */ int16_t leakRtl;
    /* 0x012 */ int16_t leakLtr;
    /* 0x014 */ uint16_t unk_14;
    /* 0x016 */ int16_t unk_16;
    /* 0x018 */ uint8_t unk_18;
    /* 0x019 */ uint8_t unk_19;
    /* 0x01A */ uint8_t unk_1A;
    /* 0x01B */ uint8_t unk_1B;
    /* 0x01C */ int32_t nextRingBufPos;
    /* 0x020 */ int32_t unk_20;
    /* 0x024 */ int32_t bufSizePerChan;
    /* 0x028 */ int16_t* leftRingBuf;
    /* 0x02C */ int16_t* rightRingBuf;
    /* 0x030 */ void* unk_30;
    /* 0x034 */ void* unk_34;
    /* 0x038 */ void* unk_38;
    /* 0x03C */ void* unk_3C;
    /* 0x040 */ ReverbRingBufferItem items[2][5];
    /* 0x158 */ ReverbRingBufferItem items2[2][5];
    /* 0x270 */ int16_t* filterLeft;
    /* 0x274 */ int16_t* filterRight;
    /* 0x278 */ int16_t* filterLeftState;
    /* 0x27C */ int16_t* filterRightState;
    /* 0x280 */ SoundFontSound sound;
    /* 0x288 */ SoundFontSample sample;
    /* 0x298 */ AdpcmLoop loop;
} SynthesisReverb; // size = 0x2C8

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

typedef struct {
    /* 0x00 */ uint8_t* pc;
    /* 0x04 */ uint8_t* stack[4];
    /* 0x14 */ uint8_t remLoopIters[4];
    /* 0x18 */ uint8_t depth;
    /* 0x19 */ int8_t value;
} SeqScriptState; // size = 0x1C

// Also known as a Group, according to debug strings.
typedef struct {
    /* 0x000 */ uint8_t enabled : 1;
    /* 0x000 */ uint8_t finished : 1;
    /* 0x000 */ uint8_t muted : 1;
    /* 0x000 */ uint8_t seqDmaInProgress : 1;
    /* 0x000 */ uint8_t fontDmaInProgress : 1;
    /* 0x000 */ uint8_t recalculateVolume : 1;
    /* 0x000 */ uint8_t stopScript : 1;
    /* 0x000 */ uint8_t unk_0b1 : 1;
    /* 0x001 */ uint8_t state;
    /* 0x002 */ uint8_t noteAllocPolicy;
    /* 0x003 */ uint8_t muteBehavior;
    /* 0x004 */ uint16_t seqId;
    /* 0x005 */ uint8_t defaultFont;
    /* 0x006 */ uint8_t unk_06[1];
    /* 0x007 */ int8_t playerIdx;
    /* 0x008 */ uint16_t tempo; // tatums per minute
    /* 0x00A */ uint16_t tempoAcc;
    /* 0x00C */ uint16_t unk_0C;
    /* 0x00E */ int16_t transposition;
    /* 0x010 */ uint16_t delay;
    /* 0x012 */ uint16_t fadeTimer;
    /* 0x014 */ uint16_t fadeTimerUnkEu;
    /* 0x018 */ uint8_t* seqData;
    /* 0x01C */ float fadeVolume;
    /* 0x020 */ float fadeVelocity;
    /* 0x024 */ float volume;
    /* 0x028 */ float muteVolumeScale;
    /* 0x02C */ float fadeVolumeScale;
                float gameVolume;
    /* 0x030 */ float appliedFadeVolume;
    /* 0x034 */ float unk_34;
    /* 0x038 */ struct SequenceChannel* channels[16];
    /* 0x078 */ SeqScriptState scriptState;
    /* 0x094 */ uint8_t* shortNoteVelocityTable;
    /* 0x098 */ uint8_t* shortNoteGateTimeTable;
    /* 0x09C */ NotePool notePool;
    /* 0x0DC */ int32_t skipTicks;
    /* 0x0E0 */ uint32_t scriptCounter;
    /* 0x0E4 */ char unk_E4[0x74]; // unused struct members for sequence/sound font dma management, according to sm64 decomp
    /* 0x158 */ int8_t soundScriptIO[8];
} SequencePlayer; // size = 0x160

typedef struct {
    /* 0x0 */ uint8_t releaseRate;
    /* 0x1 */ uint8_t sustain;
    /* 0x4 */ AdsrEnvelope* envelope;
} AdsrSettings; // size = 0x8

typedef struct {
    /* 0x00 */ union {
        struct A {
            /* 0x00 */ uint8_t unk_0b80 : 1;
            /* 0x00 */ uint8_t hang : 1;
            /* 0x00 */ uint8_t decay : 1;
            /* 0x00 */ uint8_t release : 1;
            /* 0x00 */ uint8_t state : 4;
        } s;
        /* 0x00 */ uint8_t asByte;
    } action;
    /* 0x01 */ uint8_t envIndex;
    /* 0x02 */ int16_t delay;
    /* 0x04 */ float sustain;
    /* 0x08 */ float velocity;
    /* 0x0C */ float fadeOutVel;
    /* 0x10 */ float current;
    /* 0x14 */ float target;
    /* 0x18 */ char unk_18[4];
    /* 0x1C */ AdsrEnvelope* envelope;
} AdsrState;

typedef struct {
    /* 0x00 */ uint8_t unused : 2;
    /* 0x00 */ uint8_t bit2 : 2;
    /* 0x00 */ uint8_t strongRight : 1;
    /* 0x00 */ uint8_t strongLeft : 1;
    /* 0x00 */ uint8_t stereoHeadsetEffects : 1;
    /* 0x00 */ uint8_t usesHeadsetPanEffects : 1;
} StereoData;

typedef union {
    /* 0x00 */ StereoData s;
    /* 0x00 */ uint8_t asByte;
} Stereo;

typedef struct {
    /* 0x00 */ uint8_t reverb;
    /* 0x01 */ uint8_t unk_1;
    /* 0x02 */ uint8_t pan;
    /* 0x03 */ Stereo stereo;
    /* 0x04 */ uint8_t unk_4;
    /* 0x06 */ uint16_t unk_6;
    /* 0x08 */ float freqScale;
    /* 0x0C */ float velocity;
    /* 0x10 */ int16_t* filter;
    /* 0x14 */ int16_t filterBuf[8];
} NoteAttributes; // size = 0x24

// Also known as a SubTrack, according to sm64 debug strings.
typedef struct SequenceChannel {
    /* 0x00 */ uint8_t enabled : 1;
    /* 0x00 */ uint8_t finished : 1;
    /* 0x00 */ uint8_t stopScript : 1;
    /* 0x00 */ uint8_t stopSomething2 : 1; // sets SequenceLayer.stopSomething
    /* 0x00 */ uint8_t hasInstrument : 1;
    /* 0x00 */ uint8_t stereoHeadsetEffects : 1;
    /* 0x00 */ uint8_t largeNotes : 1; // notes specify duration and velocity
    /* 0x00 */ uint8_t unused : 1;
    union {
        struct {
            /* 0x01 */ uint8_t freqScale : 1;
            /* 0x01 */ uint8_t volume : 1;
            /* 0x01 */ uint8_t pan : 1;
        } s;
        /* 0x01 */ uint8_t asByte;
    } changes;
    /* 0x02 */ uint8_t noteAllocPolicy;
    /* 0x03 */ uint8_t muteBehavior;
    /* 0x04 */ uint8_t reverb;       // or dry/wet mix
    /* 0x05 */ uint8_t notePriority; // 0-3
    /* 0x06 */ uint8_t someOtherPriority;
    /* 0x07 */ uint8_t fontId;
    /* 0x08 */ uint8_t reverbIndex;
    /* 0x09 */ uint8_t bookOffset;
    /* 0x0A */ uint8_t newPan;
    /* 0x0B */ uint8_t panChannelWeight;  // proportion of pan that comes from the channel (0..128)
    /* 0x0C */ uint8_t unk_0C;
    /* 0x0D */ uint8_t velocityRandomVariance;
    /* 0x0E */ uint8_t gateTimeRandomVariance;
    /* 0x0F */ uint8_t unk_0F;
    /* 0x10 */ uint16_t vibratoRateStart;
    /* 0x12 */ uint16_t vibratoExtentStart;
    /* 0x14 */ uint16_t vibratoRateTarget;
    /* 0x16 */ uint16_t vibratoExtentTarget;
    /* 0x18 */ uint16_t vibratoRateChangeDelay;
    /* 0x1A */ uint16_t vibratoExtentChangeDelay;
    /* 0x1C */ uint16_t vibratoDelay;
    /* 0x1E */ uint16_t delay;
    /* 0x20 */ uint16_t unk_20;
    /* 0x22 */ uint16_t unk_22;
    /* 0x24 */ int16_t instOrWave; // either 0 (none), instrument index + 1, or
                             // 0x80..0x83 for sawtooth/triangle/sine/square waves.
    /* 0x26 */ int16_t transposition;
    /* 0x28 */ float volumeScale;
    /* 0x2C */ float volume;
    /* 0x30 */ int32_t pan;
    /* 0x34 */ float appliedVolume;
    /* 0x38 */ float freqScale;
    /* 0x3C */ uint8_t (*dynTable)[][2];
    /* 0x40 */ struct Note* noteUnused;
    /* 0x44 */ struct SequenceLayer* layerUnused;
    /* 0x48 */ Instrument* instrument;
    /* 0x4C */ SequencePlayer* seqPlayer;
    /* 0x50 */ struct SequenceLayer* layers[4];
    /* 0x60 */ SeqScriptState scriptState;
    /* 0x7C */ AdsrSettings adsr;
    /* 0x84 */ NotePool notePool;
    /* 0xC4 */ int8_t soundScriptIO[8]; // bridge between sound script and audio lib, "io ports"
    /* 0xCC */ int16_t* filter;
    /* 0xD0 */ Stereo stereo;
} SequenceChannel; // size = 0xD4

// Might also be known as a Track, according to sm64 debug strings (?).
typedef struct SequenceLayer {
    /* 0x00 */ uint8_t enabled : 1;
    /* 0x00 */ uint8_t finished : 1;
    /* 0x00 */ uint8_t stopSomething : 1;
    /* 0x00 */ uint8_t continuousNotes : 1; // keep the same note for consecutive notes with the same sound
    /* 0x00 */ uint8_t bit3 : 1; // "loaded"?
    /* 0x00 */ uint8_t ignoreDrumPan : 1;
    /* 0x00 */ uint8_t bit1 : 1; // "has initialized continuous notes"?
    /* 0x00 */ uint8_t notePropertiesNeedInit : 1;
    /* 0x01 */ Stereo stereo;
    /* 0x02 */ uint8_t instOrWave;
    /* 0x03 */ uint8_t gateTime;
    /* 0x04 */ uint8_t semitone;
    /* 0x05 */ uint8_t portamentoTargetNote;
    /* 0x06 */ uint8_t pan; // 0..128
    /* 0x07 */ uint8_t notePan;
    /* 0x08 */ int16_t delay;
    /* 0x0A */ int16_t gateDelay;
    /* 0x0C */ int16_t delay2;
    /* 0x0E */ uint16_t portamentoTime;
    /* 0x10 */ int16_t transposition; // #semitones added to play commands
                                  // (seq instruction encoding only allows referring to the limited range
                                  // 0..0x3F; this makes 0x40..0x7F accessible as well)
    /* 0x12 */ int16_t shortNoteDefaultDelay;
    /* 0x14 */ int16_t lastDelay;
    /* 0x18 */ AdsrSettings adsr;
    /* 0x20 */ Portamento portamento;
    /* 0x2C */ struct Note* note;
    /* 0x30 */ float freqScale;
    /* 0x34 */ float unk_34;
    /* 0x38 */ float velocitySquare2;
    /* 0x3C */ float velocitySquare; // not sure which one of those corresponds to the sm64 original
    /* 0x40 */ float noteVelocity;
    /* 0x44 */ float noteFreqScale;
    /* 0x48 */ Instrument* instrument;
    /* 0x4C */ SoundFontSound* sound;
    /* 0x50 */ SequenceChannel* channel;
    /* 0x54 */ SeqScriptState scriptState;
    /* 0x70 */ AudioListItem listItem;
} SequenceLayer; // size = 0x80

typedef struct {
    /* 0x0000 */ int16_t adpcmdecState[0x10];
    /* 0x0020 */ int16_t finalResampleState[0x10];
    /* 0x0040 */ int16_t mixEnvelopeState[0x28];
    /* 0x0090 */ int16_t panResampleState[0x10];
    /* 0x00B0 */ int16_t panSamplesBuffer[0x20];
    /* 0x00F0 */ int16_t dummyResampleState[0x10];
} NoteSynthesisBuffers; // size = 0x110

struct OggOpusFile;

typedef struct {
    /* 0x00 */ uint8_t restart;
    /* 0x01 */ uint8_t sampleDmaIndex;
    /* 0x02 */ uint8_t prevHeadsetPanRight;
    /* 0x03 */ uint8_t prevHeadsetPanLeft;
    /* 0x04 */ uint8_t reverbVol;
    /* 0x05 */ uint8_t numParts;
    /* 0x06 */ uint16_t samplePosFrac;
    /* 0x08 */ int32_t samplePosInt;
    /* 0x0C */ NoteSynthesisBuffers* synthesisBuffers;
    /* 0x10 */ int16_t curVolLeft;
    /* 0x12 */ int16_t curVolRight;
    /* 0x14 */ uint16_t unk_14;
    /* 0x16 */ uint16_t unk_16;
    /* 0x18 */ uint16_t unk_18;
    /* 0x1A */ uint8_t unk_1A;
    /* 0x1C */ uint16_t unk_1C;
    /* 0x1E */ uint16_t unk_1E;
    struct OggOpusFile* opusFile; // Only for streamed opus audio
} NoteSynthesisState; // size = 0x20

typedef struct {
    /* 0x00 */ struct SequenceChannel* channel;
    /* 0x04 */ uint32_t time;
    /* 0x08 */ int16_t* curve;
    /* 0x0C */ float extent;
    /* 0x10 */ float rate;
    /* 0x14 */ uint8_t active;
    /* 0x16 */ uint16_t rateChangeTimer;
    /* 0x18 */ uint16_t extentChangeTimer;
    /* 0x1A */ uint16_t delay;
} VibratoState; // size = 0x1C

typedef struct {
    /* 0x00 */ uint8_t priority;
    /* 0x01 */ uint8_t waveId;
    /* 0x02 */ uint8_t sampleCountIndex;
    /* 0x03 */ uint8_t fontId;
    /* 0x04 */ uint8_t unk_04;
    /* 0x05 */ uint8_t stereoHeadsetEffects;
    /* 0x06 */ int16_t adsrVolScaleUnused;
    /* 0x08 */ float portamentoFreqScale;
    /* 0x0C */ float vibratoFreqScale;
    /* 0x10 */ SequenceLayer* prevParentLayer;
    /* 0x14 */ SequenceLayer* parentLayer;
    /* 0x18 */ SequenceLayer* wantedParentLayer;
    /* 0x1C */ NoteAttributes attributes;
    /* 0x40 */ AdsrState adsr;
    // may contain portamento, vibratoState, if those are not part of Note itself
} NotePlaybackState;

typedef struct {
    struct {
        /* 0x00 */ volatile uint8_t enabled : 1;
        /* 0x00 */ uint8_t needsInit : 1;
        /* 0x00 */ uint8_t finished : 1; // ?
        /* 0x00 */ uint8_t unused : 1;
        /* 0x00 */ uint8_t stereoStrongRight : 1;
        /* 0x00 */ uint8_t stereoStrongLeft : 1;
        /* 0x00 */ uint8_t stereoHeadsetEffects : 1;
        /* 0x00 */ uint8_t usesHeadsetPanEffects : 1; // ?
    } bitField0;
    struct {
        /* 0x01 */ uint8_t reverbIndex : 3;
        /* 0x01 */ uint8_t bookOffset : 2;
        /* 0x01 */ uint8_t isSyntheticWave : 1;
        /* 0x01 */ uint8_t hasTwoParts : 1;
        /* 0x01 */ uint8_t usesHeadsetPanEffects2 : 1;
    } bitField1;
    /* 0x02 */ uint8_t unk_2;
    /* 0x03 */ uint8_t headsetPanRight;
    /* 0x04 */ uint8_t headsetPanLeft;
    /* 0x05 */ uint8_t reverbVol;
    /* 0x06 */ uint8_t unk_06;
    /* 0x07 */ uint8_t unk_07;
    /* 0x08 */ uint16_t targetVolLeft;
    /* 0x0A */ uint16_t targetVolRight;
    /* 0x0C */ uint16_t resamplingRateFixedPoint;
    /* 0x0E */ uint16_t unk_0E;
    /* 0x10 */ union {
                 SoundFontSound* soundFontSound;
                 int16_t* samples; // used for synthetic waves
             } sound;
    /* 0x14 */ int16_t* filter;
    /* 0x18 */ char pad_18[0x8];
} NoteSubEu; // size = 0x20

typedef struct Note {
    /* 0x00 */ AudioListItem listItem;
    /* 0x10 */ NoteSynthesisState synthesisState;
    /* 0x30 */ NotePlaybackState playbackState;
    /* 0x90 */ Portamento portamento;
    /* 0x9C */ VibratoState vibratoState;
    /* 0xB8 */ char unk_B8[0x4];
    /* 0xBC */ uint32_t unk_BC;
    /* 0xC0 */ NoteSubEu noteSubEu;
} Note; // size = 0xE0

typedef struct {
    /* 0x00 */ uint8_t downsampleRate;
    /* 0x02 */ uint16_t windowSize;
    /* 0x04 */ uint16_t unk_4;
    /* 0x06 */ uint16_t unk_6;
    /* 0x08 */ uint16_t unk_8;
    /* 0x0A */ uint16_t unk_A;
    /* 0x0C */ uint16_t leakRtl;
    /* 0x0E */ uint16_t leakLtr;
    /* 0x10 */ int8_t unk_10;
    /* 0x12 */ uint16_t unk_12;
    /* 0x14 */ int16_t lowPassFilterCutoffLeft;
    /* 0x16 */ int16_t lowPassFilterCutoffRight;
} ReverbSettings; // size = 0x18

typedef struct {
    /* 0x00 */ uint32_t frequency;
    /* 0x04 */ uint8_t unk_04;
    /* 0x05 */ uint8_t numNotes;
    /* 0x06 */ uint8_t numSequencePlayers;
    /* 0x07 */ uint8_t unk_07; // unused, set to zero
    /* 0x08 */ uint8_t unk_08; // unused, set to zero
    /* 0x09 */ uint8_t numReverbs;
    /* 0x0C */ ReverbSettings* reverbSettings;
    /* 0x10 */ uint16_t sampleDmaBufSize1;
    /* 0x12 */ uint16_t sampleDmaBufSize2;
    /* 0x14 */ uint16_t unk_14;
    /* 0x18 */ uint32_t persistentSeqMem;
    /* 0x1C */ uint32_t persistentFontMem;
    /* 0x20 */ uint32_t persistentSampleMem;
    /* 0x24 */ uint32_t temporarySeqMem;
    /* 0x28 */ uint32_t temporaryFontMem;
    /* 0x2C */ uint32_t temporarySampleMem;
    /* 0x30 */ int32_t persistentSampleCacheMem;
    /* 0x34 */ int32_t temporarySampleCacheMem;
} AudioSpec; // size = 0x38

typedef struct {
    /* 0x00 */ int16_t specUnk4;
    /* 0x02 */ uint16_t frequency;
    /* 0x04 */ uint16_t aiFrequency;
    /* 0x06 */ int16_t samplesPerFrameTarget;
    /* 0x08 */ int16_t maxAiBufferLength;
    /* 0x0A */ int16_t minAiBufferLength;
    /* 0x0C */ int16_t updatesPerFrame;
    /* 0x0E */ int16_t samplesPerUpdate;
    /* 0x10 */ int16_t samplesPerUpdateMax;
    /* 0x12 */ int16_t samplesPerUpdateMin;
    /* 0x14 */ int16_t numSequencePlayers;
    /* 0x18 */ float resampleRate;
    /* 0x1C */ float updatesPerFrameInv;
    /* 0x20 */ float unkUpdatesPerFrameScaled;
    /* 0x24 */ float unk_24;
} AudioBufferParameters;

typedef struct {
    /* 0x0 */ uint8_t* start;
    /* 0x4 */ uint8_t* cur;
    /* 0x8 */ ptrdiff_t size;
    /* 0xC */ int32_t count;
} AudioAllocPool; // size = 0x10

typedef struct {
    /* 0x0 */ uint8_t* ptr;
    /* 0x4 */ size_t size;
    /* 0x8 */ int16_t tableType;
    /* 0xA */ int16_t id;
} AudioCacheEntry; // size = 0xC

typedef struct {
    /* 0x00 */ int8_t inUse;
    /* 0x01 */ int8_t origMedium;
    /* 0x02 */ int8_t sampleBankId;
    /* 0x03 */ char unk_03[0x5];
    /* 0x08 */ uint8_t* allocatedAddr;
    /* 0x0C */ void* sampleAddr;
    /* 0x10 */ size_t size;
} SampleCacheEntry; // size = 0x14

typedef struct {
    /* 0x000 */ AudioAllocPool pool;
    /* 0x010 */ SampleCacheEntry entries[32];
    /* 0x290 */ ptrdiff_t size;
} AudioSampleCache; // size = 0x294

typedef struct {
    /* 0x00*/ uint32_t numEntries;
    /* 0x04*/ AudioAllocPool pool;
    /* 0x14*/ AudioCacheEntry entries[16];
} AudioPersistentCache; // size = 0xD4

typedef struct {
    /* 0x00*/ uint32_t nextSide;
    /* 0x04*/ AudioAllocPool pool;
    /* 0x14*/ AudioCacheEntry entries[2];
} AudioTemporaryCache; // size = 0x3C

typedef struct {
    /* 0x000*/ AudioPersistentCache persistent;
    /* 0x0D4*/ AudioTemporaryCache temporary;
    /* 0x100*/ uint8_t unk_100[0x10];
} AudioCache; // size = 0x110

typedef struct {
    uint32_t wantPersistent;
    uint32_t wantTemporary;
} AudioPoolSplit2; // size = 0x8

typedef struct {
    uint32_t wantSeq;
    uint32_t wantFont;
    uint32_t wantSample;
} AudioPoolSplit3; // size = 0xC

typedef struct {
    uint32_t wantSeq;
    uint32_t wantFont;
    uint32_t wantSample;
    uint32_t wantCustom;
} AudioPoolSplit4; // size = 0x10

typedef struct {
    /* 0x00 */ uint32_t endAndMediumKey;
    /* 0x04 */ SoundFontSample* sample;
    /* 0x08 */ uint8_t* ramAddr;
    /* 0x0C */ uint32_t encodedInfo;
    /* 0x10 */ int32_t isFree;
} AudioPreloadReq; // size = 0x14

typedef struct {
#ifdef IS_BIGENDIAN
    union{
        uint32_t opArgs;
        struct {
            uint8_t op;
            uint8_t arg0;
            uint8_t arg1;
            uint8_t arg2;
        };
    };
    union {
        void* data;
        float asFloat;
        int32_t asInt;
        struct {
            uint16_t asUShort;
            uint8_t pad2[2];
        };
        struct {
            int8_t asSbyte;
            uint8_t pad1[3];
        };
        struct {
            uint8_t asUbyte;
            uint8_t pad0[3];
        };
        uint32_t asUInt;
    };
#else
    union{
        uint32_t opArgs;
        struct {
            uint8_t arg2;
            uint8_t arg1;
            uint8_t arg0;
            uint8_t op;
        };
    };
    union {
        uint32_t data;
        float asFloat;
        int32_t asInt;
        struct {
            uint8_t pad2[2];
            uint16_t asUShort;
        };
        struct {
            uint8_t pad1[3];
            int8_t asSbyte;
        };
        struct {
            uint8_t pad0[3];
            uint8_t asUbyte;
        };
        uint32_t asUInt;
    };
#endif
} AudioCmd;

typedef struct {
    /* 0x00 */ int8_t status;
    /* 0x01 */ int8_t delay;
    /* 0x02 */ int8_t medium;
    /* 0x04 */ uint8_t* ramAddr;
    /* 0x08 */ uint8_t* curDevAddr;
    /* 0x0C */ uint8_t* curRamAddr;
    /* 0x10 */ size_t bytesRemaining;
    /* 0x14 */ size_t chunkSize;
    /* 0x18 */ int32_t unkMediumParam;
    /* 0x1C */ uint32_t retMsg;
    /* 0x20 */ OSMesgQueue* retQueue;
    /* 0x24 */ OSMesgQueue msgQueue;
    /* 0x3C */ OSMesg msg;
    /* 0x40 */ OSIoMesg ioMesg;
} AudioAsyncLoad; // size = 0x58

typedef struct {
    /* 0x00 */ uint8_t medium;
    /* 0x01 */ uint8_t seqOrFontId;
    /* 0x02 */ uint16_t instId;
    /* 0x04 */ int32_t unkMediumParam;
    /* 0x08 */ uint8_t* curDevAddr;
    /* 0x0C */ uint8_t* curRamAddr;
    /* 0x10 */ uint8_t* ramAddr;
    /* 0x14 */ int32_t status;
    /* 0x18 */ int32_t bytesRemaining;
    /* 0x1C */ int8_t* isDone;
    /* 0x20 */ SoundFontSample sample;
    /* 0x30 */ OSMesgQueue msgqueue;
    /* 0x48 */ OSMesg msg;
    /* 0x4C */ OSIoMesg ioMesg;
} AudioSlowLoad; // size = 0x64

typedef struct {
    /* 0x00 */ uintptr_t romAddr;
    /* 0x04 */ size_t size;
    /* 0x08 */ int8_t medium;
    /* 0x09 */ int8_t cachePolicy;
    /* 0x0A */ int16_t shortData1;
    /* 0x0C */ int16_t shortData2;
    /* 0x0E */ int16_t shortData3;
} AudioTableEntry; // size = 0x10

typedef struct {
    /* 0x00 */ int16_t numEntries;
    /* 0x02 */ int16_t unkMediumParam;
    /* 0x04 */ uintptr_t romAddr;
    /* 0x08 */ char pad[0x8];
    /* 0x10 */ AudioTableEntry entries[512]; // (dynamic size)
} AudioTable; // size >= 0x20

typedef struct {
    /* 0x00 */ OSTask task;
    /* 0x40 */ OSMesgQueue* taskQueue;
    /* 0x44 */ void* unk_44; // probably a message that gets unused.
    /* 0x48 */ char unk_48[0x8];
} AudioTask; // size = 0x50

typedef struct {
    /* 0x00 */ uint8_t* ramAddr;
    /* 0x04 */ uint32_t devAddr;
    /* 0x08 */ uint16_t sizeUnused;
    /* 0x0A */ uint16_t size;
    /* 0x0C */ uint8_t unused;
    /* 0x0D */ uint8_t reuseIndex; // position in sSampleDmaReuseQueue1/2, if ttl == 0
    /* 0x0E */ uint8_t ttl;        // duration after which the DMA can be discarded
} SampleDma; // size = 0x10

#include <runtime/libultra/abi.h>

typedef struct {
    /* 0x0000 */ char unk_0000;
    /* 0x0001 */ int8_t numSynthesisReverbs;
    /* 0x0002 */ uint16_t unk_2;
    /* 0x0004 */ uint16_t unk_4;
    /* 0x0006 */ char unk_0006[0x0A];
    /* 0x0010 */ int16_t* curLoadedBook;
    /* 0x0014 */ NoteSubEu* noteSubsEu;
    /* 0x0018 */ SynthesisReverb synthesisReverbs[4];
    /* 0x0B38 */ char unk_0B38[0x30];
    /* 0x0B68 */ SoundFontSample* usedSamples[128];
    /* 0x0D68 */ AudioPreloadReq preloadSampleStack[128];
    /* 0x1768 */ int32_t numUsedSamples;
    /* 0x176C */ int32_t preloadSampleStackTop;
    /* 0x1770 */ AudioAsyncLoad asyncLoads[0x10];
    /* 0x1CF0 */ OSMesgQueue asyncLoadUnkMediumQueue;
    /* 0x1D08 */ char unk_1D08[0x40];
    /* 0x1D48 */ AudioAsyncLoad* curUnkMediumLoad;
    /* 0x1D4C */ uint32_t slowLoadPos;
    /* 0x1D50 */ AudioSlowLoad slowLoads[2];
    /* 0x1E18 */ OSPiHandle* cartHandle;
    /* 0x1E1C */ OSPiHandle* driveHandle;
    /* 0x1E20 */ OSMesgQueue externalLoadQueue;
    /* 0x1E38 */ OSMesg externalLoadMesgBuf[0x10];
    /* 0x1E78 */ OSMesgQueue preloadSampleQueue;
    /* 0x1E90 */ OSMesg preloadSampleMesgBuf[0x10];
    /* 0x1ED0 */ OSMesgQueue currAudioFrameDmaQueue;
    /* 0x1EE8 */ OSMesg currAudioFrameDmaMesgBuf[0x40];
    /* 0x1FE8 */ OSIoMesg currAudioFrameDmaIoMesgBuf[0x40];
    /* 0x25E8 */ OSMesgQueue syncDmaQueue;
    /* 0x2600 */ OSMesg syncDmaMesg;
    /* 0x2604 */ OSIoMesg syncDmaIoMesg;
    /* 0x261C */ SampleDma* sampleDmas;
    /* 0x2620 */ uint32_t sampleDmaCount;
    /* 0x2624 */ uint32_t sampleDmaListSize1;
    /* 0x2628 */ int32_t unused2628;
    /* 0x262C */ uint8_t sampleDmaReuseQueue1[0x100]; // read pos <= write pos, wrapping mod 256
    /* 0x272C */ uint8_t sampleDmaReuseQueue2[0x100];
    /* 0x282C */ uint8_t sampleDmaReuseQueue1RdPos;
    /* 0x282D */ uint8_t sampleDmaReuseQueue2RdPos;
    /* 0x282E */ uint8_t sampleDmaReuseQueue1WrPos;
    /* 0x282F */ uint8_t sampleDmaReuseQueue2WrPos;
    /* 0x2830 */ AudioTable* sequenceTable;
    /* 0x2834 */ AudioTable* soundFontTable;
    /* 0x2838 */ AudioTable* sampleBankTable;
    /* 0x283C */ uint8_t* sequenceFontTable;
    /* 0x2840 */ uint16_t numSequences;
    /* 0x2844 */ SoundFont* soundFonts;
    /* 0x2848 */ AudioBufferParameters audioBufferParameters;
    /* 0x2870 */ float unk_2870;
    /* 0x2874 */ int32_t sampleDmaBufSize1;
    /* 0x2874 */ int32_t sampleDmaBufSize2;
    /* 0x287C */ char unk_287C[0x10];
    /* 0x288C */ int32_t sampleDmaBufSize;
    /* 0x2890 */ int32_t maxAudioCmds;
    /* 0x2894 */ int32_t numNotes;
    /* 0x2898 */ int16_t tempoInternalToExternal;
    /* 0x289A */ int8_t soundMode;
    /* 0x289C */ int32_t totalTaskCnt;
    /* 0x28A0 */ int32_t curAudioFrameDmaCount;
    /* 0x28A4 */ int32_t rspTaskIdx;
    /* 0x28A8 */ int32_t curAIBufIdx;
    /* 0x28AC */ Acmd* abiCmdBufs[2];
    /* 0x28B4 */ Acmd* curAbiCmdBuf;
    /* 0x28B8 */ AudioTask* currTask;
    /* 0x28BC */ char unk_28BC[0x4];
    /* 0x28C0 */ AudioTask rspTask[2];
    /* 0x2960 */ float unk_2960;
    /* 0x2964 */ int32_t refreshRate;
    /* 0x2968 */ int16_t* aiBuffers[3];
    /* 0x2974 */ int16_t aiBufLengths[3];
    /* 0x297C */ uint32_t audioRandom;
    /* 0x2980 */ int32_t audioErrorFlags;
    /* 0x2984 */ volatile uint32_t resetTimer;
    /* 0x2988 */ char unk_2988[0x8];
    /* 0x2990 */ AudioAllocPool audioSessionPool;
    /* 0x29A0 */ AudioAllocPool externalPool;
    /* 0x29B0 */ AudioAllocPool audioInitPool;
    /* 0x29C0 */ AudioAllocPool notesAndBuffersPool;
    /* 0x29D0 */ char unk_29D0[0x20]; // probably two unused pools
    /* 0x29F0 */ AudioAllocPool cachePool;
    /* 0x2A00 */ AudioAllocPool persistentCommonPool;
    /* 0x2A10 */ AudioAllocPool temporaryCommonPool;
    /* 0x2A20 */ AudioCache seqCache;
    /* 0x2B30 */ AudioCache fontCache;
    /* 0x2C40 */ AudioCache sampleBankCache;
    /* 0x2D50 */ AudioAllocPool permanentPool;
    /* 0x2D60 */ AudioCacheEntry permanentCache[32];
    /* 0x2EE0 */ AudioSampleCache persistentSampleCache;
    /* 0x3174 */ AudioSampleCache temporarySampleCache;
    /* 0x3408 */ AudioPoolSplit4 sessionPoolSplit;
    /* 0x3418 */ AudioPoolSplit2 cachePoolSplit;
    /* 0x3420 */ AudioPoolSplit3 persistentCommonPoolSplit;
    /* 0x342C */ AudioPoolSplit3 temporaryCommonPoolSplit;
    /* 0x3438 */ uint8_t sampleFontLoadStatus[0x30];
    /* 0x3468 */ uint8_t* fontLoadStatus;
    /* 0x3498 */ uint8_t* seqLoadStatus;
    /* 0x3518 */ volatile uint8_t resetStatus;
    /* 0x3519 */ uint8_t audioResetSpecIdToLoad;
    /* 0x351C */ int32_t audioResetFadeOutFramesLeft;
    /* 0x3520 */ float* unk_3520;
    /* 0x3524 */ uint8_t* audioHeap;
    /* 0x3528 */ size_t audioHeapSize;
    /* 0x352C */ Note* notes;
    /* 0x3530 */ SequencePlayer seqPlayers[4];
    /* 0x3AB0 */ SequenceLayer sequenceLayers[64];
    /* 0x5AB0 */ SequenceChannel sequenceChannelNone;
    /* 0x5B84 */ int32_t noteSubEuOffset;
    /* 0x5B88 */ AudioListItem layerFreeList;
    /* 0x5B98 */ NotePool noteFreeLists;
    /* 0x5BD8 */ uint8_t cmdWrPos;
    /* 0x5BD9 */ uint8_t cmdRdPos;
    /* 0x5BDA */ uint8_t cmdQueueFinished;
    /* 0x5BDC */ uint16_t unk_5BDC[4];
    /* 0x5BE4 */ OSMesgQueue* audioResetQueueP;
    /* 0x5BE8 */ OSMesgQueue* taskStartQueueP;
    /* 0x5BEC */ OSMesgQueue* cmdProcQueueP;
    /* 0x5BF0 */ OSMesgQueue taskStartQueue;
    /* 0x5C08 */ OSMesgQueue cmdProcQueue;
    /* 0x5C20 */ OSMesgQueue audioResetQueue;
    /* 0x5C38 */ OSMesg taskStartMsgs[1];
    /* 0x5C3C */ OSMesg audioResetMesgs[1];
    /* 0x5C40 */ OSMesg cmdProcMsgs[4];
    /* 0x5C50 */ AudioCmd cmdBuf[0x100];
} AudioContext; // size = 0x6450

typedef struct {
    /* 0x00 */ uint8_t reverbVol;
    /* 0x01 */ uint8_t unk_1;
    /* 0x02 */ uint8_t pan;
    /* 0x03 */ Stereo stereo;
    /* 0x04 */ float frequency;
    /* 0x08 */ float velocity;
    /* 0x0C */ char unk_0C[0x4];
    /* 0x10 */ int16_t* filter;
    /* 0x14 */ uint8_t unk_14;
    /* 0x16 */ uint16_t unk_16;
} NoteSubAttributes; // size = 0x18

typedef struct {
    /* 0x00 */ size_t heapSize;
    /* 0x04 */ size_t initPoolSize;
    /* 0x08 */ size_t permanentPoolSize;
} AudioContextInitSizes; // size = 0xC

typedef struct {
    /* 0x00 */ float volCur;
    /* 0x04 */ float volTarget;
    /* 0x08 */ float volStep;
    /* 0x0C */ uint16_t volTimer;
    /* 0x10 */ float freqScaleCur;
    /* 0x14 */ float freqScaleTarget;
    /* 0x18 */ float freqScaleStep;
    /* 0x1C */ uint16_t freqScaleTimer;
} ActiveSequenceChannelData; // size = 0x20

typedef struct {
    /* 0x000 */ float volCur;
    /* 0x004 */ float volTarget;
    /* 0x008 */ float volStep;
    /* 0x00C */ uint16_t volTimer;
    /* 0x00E */ uint8_t volScales[4];
    /* 0x012 */ uint8_t volFadeTimer;
    /* 0x013 */ uint8_t fadeVolUpdate;
    /* 0x014 */ uint32_t tempoCmd;
    /* 0x018 */ uint16_t tempoOriginal; // stores the original tempo before modifying it (to reset back to)
    /* 0x01C */ float tempoCur;
    /* 0x020 */ float tempoTarget;
    /* 0x024 */ float tempoStep;
    /* 0x028 */ uint16_t tempoTimer;
    /* 0x02C */ uint32_t setupCmd[8]; // a queue of cmds to execute once the player is disabled
    /* 0x04C */ uint8_t setupCmdTimer; // only execute setup commands when the timer is at 0.
    /* 0x04D */ uint8_t setupCmdNum; // number of setup commands requested once the player is disabled
    /* 0x04E */ uint8_t setupFadeTimer;
    /* 0x050 */ ActiveSequenceChannelData channelData[16];
    /* 0x250 */ uint16_t freqScaleChannelFlags;
    /* 0x252 */ uint16_t volChannelFlags;
    /* 0x254 */ uint16_t seqId; // active seqId currently playing. Resets when sequence stops
    /* 0x256 */ uint16_t prevSeqId; // last seqId played on a player. Does not reset when sequence stops
    /* 0x258 */ uint16_t channelPortMask;
    /* 0x25C */ uint32_t startSeqCmd; // This name comes from MM
    /* 0x260 */ uint8_t isWaitingForFonts; // This name comes from MM
} ActiveSequence; // size = 0x264

typedef enum {
    /* 0 */ BANK_PLAYER,
    /* 1 */ BANK_ITEM,
    /* 2 */ BANK_ENV,
    /* 3 */ BANK_ENEMY,
    /* 4 */ BANK_SYSTEM,
    /* 5 */ BANK_OCARINA,
    /* 6 */ BANK_VOICE
} SoundBankTypes;

typedef enum {
    /* 0 */ SFX_STATE_EMPTY,
    /* 1 */ SFX_STATE_QUEUED,
    /* 2 */ SFX_STATE_READY,
    /* 3 */ SFX_STATE_PLAYING_REFRESH,
    /* 4 */ SFX_STATE_PLAYING_1,
    /* 5 */ SFX_STATE_PLAYING_2
} SfxState;

typedef struct {
    /* 0x00 */ float*     posX;
    /* 0x04 */ float*     posY;
    /* 0x08 */ float*     posZ;
    /* 0x0C */ uint8_t       token;
    /* 0x10 */ float*     freqScale;
    /* 0x14 */ float*     vol;
    /* 0x18 */ int8_t*      reverbAdd;
    /* 0x1C */ float      dist;
    /* 0x20 */ uint32_t      priority; // lower is more prioritized
    /* 0x24 */ uint8_t       sfxImportance;
    /* 0x26 */ uint16_t      sfxParams;
    /* 0x28 */ uint16_t      sfxId;
    /* 0x2A */ uint8_t       state; // uses SfxState enum
    /* 0x2B */ uint8_t       freshness;
    /* 0x2C */ uint8_t       prev;
    /* 0x2D */ uint8_t       next;
    /* 0x2E */ uint8_t       channelIdx;
    /* 0x2F */ uint8_t       unk_2F;
} SoundBankEntry; // size = 0x30

/*
 * SFX IDs
 *
 * index    0000000111111111    observed in audio code
 * & 200    0000001000000000    single bit
 * & 400    0000010000000000    single bit
 * & 800    0000100000000000    single bit, what we currently call SFX_FLAG
 * & 600    0000011000000000    2 bits
 * & A00    0000101000000000    2 bits
 * & C00    0000110000000000    2 bits, observed in audio code
 * & E00    0000111000000000    all 3 bits
 * bank     1111000000000000    observed in audio code
 */

#define SFX_BANK_SHIFT(sfxId)   (((sfxId) >> 12) & 0xFF)

#define SFX_BANK_MASK(sfxId)    ((sfxId) & 0xF000)

#define SFX_INDEX(sfxId)    ((sfxId) & 0x01FF)
#define SFX_BANK(sfxId)     SFX_BANK_SHIFT(SFX_BANK_MASK(sfxId))

typedef struct {
    uint32_t priority; // lower is more prioritized
    uint8_t entryIndex;
} ActiveSound;

typedef struct {
    uint8_t importance;
    uint16_t params;
} SoundParams;

typedef struct {
    /* 0x0000 */ uint8_t noteIdx;
    /* 0x0001 */ uint8_t unk_01;
    /* 0x0002 */ uint16_t unk_02;
    /* 0x0004 */ uint8_t volume;
    /* 0x0005 */ uint8_t vibrato;
    /* 0x0006 */ int8_t tone;
    /* 0x0007 */ uint8_t semitone;
} OcarinaNote;  // size = 0x8

typedef struct {
    uint8_t len;
    uint8_t notesIdx[8];
} OcarinaSongInfo;

typedef struct {
    uint8_t noteIdx;
    uint8_t state;   // original name: "status"
    uint8_t pos;     // original name: "locate"
} OcarinaStaff;

typedef enum {
    /*  0 */ OCARINA_NOTE_D4,
    /*  1 */ OCARINA_NOTE_F4,
    /*  2 */ OCARINA_NOTE_A4,
    /*  3 */ OCARINA_NOTE_B4,
    /*  4 */ OCARINA_NOTE_D5,
    /* -1 */ OCARINA_NOTE_INVALID = 0xFF
} OcarinaNoteIdx;

typedef struct {
    char* seqData;
    int32_t seqDataSize;
    uint16_t seqNumber;
    uint8_t medium;
    uint8_t cachePolicy;
    int32_t numFonts;
    uint8_t fonts[16];
} SequenceData;

void Audio_SetGameVolume(int player_id, float volume);
float Audio_GetGameVolume(int player_id);

#ifdef __cplusplus
}
#endif
#endif
