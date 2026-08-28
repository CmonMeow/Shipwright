#include <libultraship/libultra.h>
#include "global.h"
#include "soh/mixer.h"

#define DEFAULT_LEN_1CH 0x1A0
#define DEFAULT_LEN_2CH 0x340

#define DMEM_TEMP 0x3C0
#define DMEM_UNCOMPRESSED_NOTE 0x580
#define DMEM_NOTE_PAN_TEMP 0x5C0
#define DMEM_SCRATCH2 0x760              // = DMEM_TEMP + DEFAULT_LEN_2CH + a bit more
#define DMEM_COMPRESSED_ADPCM_DATA 0x940 // = DMEM_LEFT_CH
#define DMEM_LEFT_CH 0x940
#define DMEM_RIGHT_CH 0xAE0
#define DMEM_WET_TEMP 0x3E0
#define DMEM_WET_SCRATCH 0x720 // = DMEM_WET_TEMP + DEFAULT_LEN_2CH
#define DMEM_WET_LEFT_CH 0xC80
#define DMEM_WET_RIGHT_CH 0xE20 // = DMEM_WET_LEFT_CH + DEFAULT_LEN_1CH

Acmd* AudioSynth_LoadRingBufferPart(Acmd* cmd, uint16_t dmem, uint16_t startPos, int32_t length, SynthesisReverb* reverb);
Acmd* AudioSynth_SaveBufferOffset(Acmd* cmd, uint16_t dmem, uint16_t offset, int32_t length, int16_t* buf);
Acmd* AudioSynth_SaveRingBufferPart(Acmd* cmd, uint16_t dmem, uint16_t startPos, int32_t length, SynthesisReverb* reverb);
Acmd* AudioSynth_DoOneAudioUpdate(int16_t* aiBuf, int32_t aiBufLen, Acmd* cmd, int32_t updateIndex);
Acmd* AudioSynth_ProcessNote(int32_t noteIndex, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int16_t* aiBuf,
                             int32_t aiBufLen, Acmd* cmd, int32_t updateIndex);
Acmd* AudioSynth_LoadWaveSamples(Acmd* cmd, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int32_t nSamplesToLoad);
Acmd* AudioSynth_NoteApplyHeadsetPanEffects(Acmd* cmd, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int32_t bufLen,
                                            int32_t flags, int32_t side);
Acmd* AudioSynth_ProcessEnvelope(Acmd* cmd, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int32_t aiBufLen,
                                 uint16_t inBuf, int32_t headsetPanSettings, int32_t flags);
Acmd* AudioSynth_FinalResample(Acmd* cmd, NoteSynthesisState* synthState, int32_t count, uint16_t pitch, uint16_t inpDmem,
                               int32_t resampleFlags);

uint32_t D_801304A0 = 0x13000000;
uint32_t D_801304A4 = 0x5CAEC8E2;
uint32_t D_801304A8 = 0x945CC8E2;
uint32_t D_801304AC = 0x94AEC8E2;

uint16_t D_801304B0[] = {
    0x7FFF, 0xD001, 0x3FFF, 0xF001, 0x5FFF, 0x9001, 0x7FFF, 0x8001,
};

uint8_t D_801304C0[] = { 0x40, 0x20, 0x10, 0x8 };

void AudioSynth_InitNextRingBuf(int32_t chunkLen, int32_t bufIndex, int32_t reverbIndex) {
    ReverbRingBufferItem* bufItem = { 0 };
    SynthesisReverb* reverb = &gAudioContext.synthesisReverbs[reverbIndex];
    int32_t temp_a0_2 = { 0 };
    int32_t sampleCnt = { 0 };
    int32_t extraSamples = { 0 };
    int32_t i;
    int32_t j;

    if (reverb->downsampleRate >= 2) {
        if (reverb->framesToIgnore == 0) {
            bufItem = &reverb->items[reverb->curFrame][bufIndex];
            Audio_InvalDCache(bufItem->toDownsampleLeft, DEFAULT_LEN_2CH);

            for (j = 0, i = 0; i < bufItem->lengthA / 2; j += reverb->downsampleRate, i++) {
                reverb->leftRingBuf[bufItem->startPos + i] = bufItem->toDownsampleLeft[j];
                reverb->rightRingBuf[bufItem->startPos + i] = bufItem->toDownsampleRight[j];
            }

            for (i = 0; i < bufItem->lengthB / 2; j += reverb->downsampleRate, i++) {
                reverb->leftRingBuf[i] = bufItem->toDownsampleLeft[j];
                reverb->rightRingBuf[i] = bufItem->toDownsampleRight[j];
            }
        }
    }

    bufItem = &reverb->items[reverb->curFrame][bufIndex];
    sampleCnt = chunkLen / reverb->downsampleRate;
    extraSamples = (sampleCnt + reverb->nextRingBufPos) - reverb->bufSizePerChan;
    temp_a0_2 = reverb->nextRingBufPos;
    if (extraSamples < 0) {
        bufItem->lengthA = sampleCnt * 2;
        bufItem->lengthB = 0;
        bufItem->startPos = reverb->nextRingBufPos;
        reverb->nextRingBufPos += sampleCnt;
    } else {
        bufItem->lengthA = (sampleCnt - extraSamples) * 2;
        bufItem->lengthB = extraSamples * 2;
        bufItem->startPos = reverb->nextRingBufPos;
        reverb->nextRingBufPos = extraSamples;
    }

    bufItem->numSamplesAfterDownsampling = sampleCnt;
    bufItem->chunkLen = chunkLen;

    if (reverb->unk_14 != 0) {
        int32_t temp_a0_4 = reverb->unk_14 + temp_a0_2;
        if (temp_a0_4 >= reverb->bufSizePerChan) {
            temp_a0_4 -= reverb->bufSizePerChan;
        }
        bufItem = &reverb->items2[reverb->curFrame][bufIndex];
        sampleCnt = chunkLen / reverb->downsampleRate;
        extraSamples = (temp_a0_4 + sampleCnt) - reverb->bufSizePerChan;
        if (extraSamples < 0) {
            bufItem->lengthA = sampleCnt * 2;
            bufItem->lengthB = 0;
            bufItem->startPos = temp_a0_4;
        } else {
            bufItem->lengthA = (sampleCnt - extraSamples) * 2;
            bufItem->lengthB = extraSamples * 2;
            bufItem->startPos = temp_a0_4;
        }
        bufItem->numSamplesAfterDownsampling = sampleCnt;
        bufItem->chunkLen = chunkLen;
    }
}

void func_800DB03C(int32_t arg0) {
    int32_t i;

    int32_t baseIndex = gAudioContext.numNotes * arg0;
    for (i = 0; i < gAudioContext.numNotes; i++) {
        NoteSubEu* subEu = &gAudioContext.notes[i].noteSubEu;
        NoteSubEu* subEu2 = &gAudioContext.noteSubsEu[baseIndex + i];
        if (subEu->bitField0.enabled) {
            subEu->bitField0.needsInit = false;
        } else {
            subEu2->bitField0.enabled = false;
        }

        subEu->unk_06 = 0;
    }
}

Acmd* AudioSynth_Update(Acmd* cmdStart, int32_t* cmdCnt, int16_t* aiStart, int32_t aiBufLen) {
    int32_t chunkLen = { 0 };
    int16_t* aiBufP = { 0 };
    int32_t i;
    int32_t j;

    Acmd* cmdP = cmdStart;
    for (i = gAudioContext.audioBufferParameters.updatesPerFrame; i > 0; i--) {
        AudioSeq_ProcessSequences(i - 1);
        func_800DB03C(gAudioContext.audioBufferParameters.updatesPerFrame - i);
    }

    aiBufP = aiStart;
    gAudioContext.curLoadedBook = NULL;
    for (i = gAudioContext.audioBufferParameters.updatesPerFrame; i > 0; i--) {
        if (i == 1) {
            chunkLen = aiBufLen;
        } else if ((aiBufLen / i) >= gAudioContext.audioBufferParameters.samplesPerUpdateMax) {
            chunkLen = gAudioContext.audioBufferParameters.samplesPerUpdateMax;
        } else if (gAudioContext.audioBufferParameters.samplesPerUpdateMin >= (aiBufLen / i)) {
            chunkLen = gAudioContext.audioBufferParameters.samplesPerUpdateMin;
        } else {
            chunkLen = gAudioContext.audioBufferParameters.samplesPerUpdate;
        }

        for (j = 0; j < gAudioContext.numSynthesisReverbs; j++) {
            if (gAudioContext.synthesisReverbs[j].useReverb) {
                AudioSynth_InitNextRingBuf(chunkLen, gAudioContext.audioBufferParameters.updatesPerFrame - i, j);
            }
        }

        cmdP = AudioSynth_DoOneAudioUpdate(aiBufP, chunkLen, cmdP,
                                           gAudioContext.audioBufferParameters.updatesPerFrame - i);
        aiBufLen -= chunkLen;
        aiBufP += chunkLen * 2;
    }

    for (j = 0; j < gAudioContext.numSynthesisReverbs; j++) {
        if (gAudioContext.synthesisReverbs[j].framesToIgnore != 0) {
            gAudioContext.synthesisReverbs[j].framesToIgnore--;
        }
        gAudioContext.synthesisReverbs[j].curFrame ^= 1;
    }

    *cmdCnt = cmdP - cmdStart;
    return cmdP;
}

void func_800DB2C0(int32_t updateIndexStart, int32_t noteIndex) {
    int32_t i;

    for (i = updateIndexStart + 1; i < gAudioContext.audioBufferParameters.updatesPerFrame; i++) {
        NoteSubEu* temp_v1 = &gAudioContext.noteSubsEu[(gAudioContext.numNotes * i) + noteIndex];
        if (!temp_v1->bitField0.needsInit) {
            temp_v1->bitField0.enabled = 0;
        } else {
            break;
        }
    }
}

Acmd* AudioSynth_LoadRingBuffer1AtTemp(Acmd* cmd, SynthesisReverb* reverb, int16_t bufIndex) {
    ReverbRingBufferItem* bufItem = &reverb->items[reverb->curFrame][bufIndex];

    cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_TEMP, bufItem->startPos, bufItem->lengthA, reverb);
    if (bufItem->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_TEMP + bufItem->lengthA, 0, bufItem->lengthB, reverb);
    }
    return cmd;
}

Acmd* AudioSynth_SaveRingBuffer1AtTemp(Acmd* cmd, SynthesisReverb* reverb, int16_t bufIndex) {
    ReverbRingBufferItem* bufItem = &reverb->items[reverb->curFrame][bufIndex];

    cmd = AudioSynth_SaveRingBufferPart(cmd, DMEM_WET_TEMP, bufItem->startPos, bufItem->lengthA, reverb);
    if (bufItem->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_SaveRingBufferPart(cmd, DMEM_WET_TEMP + bufItem->lengthA, 0, bufItem->lengthB, reverb);
    }
    return cmd;
}

Acmd* AudioSynth_LeakReverb(Acmd* cmd, SynthesisReverb* reverb) {
    // Leak some audio from the left reverb channel into the right reverb channel and vice versa (pan)
    aDMEMMove(cmd++, DMEM_WET_LEFT_CH, DMEM_WET_SCRATCH, DEFAULT_LEN_1CH);
    aMix(cmd++, 0x1A, reverb->leakRtl, DMEM_WET_RIGHT_CH, DMEM_WET_LEFT_CH);
    aMix(cmd++, 0x1A, reverb->leakLtr, DMEM_WET_SCRATCH, DMEM_WET_RIGHT_CH);
    return cmd;
}

Acmd* func_800DB4E4(Acmd* cmd, int32_t arg1, SynthesisReverb* reverb, int16_t arg3) {
    ReverbRingBufferItem* item = &reverb->items[reverb->curFrame][arg3];

    int16_t offsetA = (item->startPos & 7) * 2;
    int16_t offsetB = ALIGN16(offsetA + item->lengthA);
    cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_TEMP, item->startPos - (offsetA / 2), DEFAULT_LEN_1CH, reverb);
    if (item->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_TEMP + offsetB, 0, DEFAULT_LEN_1CH - offsetB, reverb);
    }
    aSetBuffer(cmd++, 0, DMEM_WET_TEMP + offsetA, DMEM_WET_LEFT_CH, arg1 * 2);
    aResample(cmd++, reverb->resampleFlags, reverb->unk_0E, reverb->unk_30);
    aSetBuffer(cmd++, 0, DMEM_WET_TEMP + DEFAULT_LEN_1CH + offsetA, DMEM_WET_RIGHT_CH, arg1 * 2);
    aResample(cmd++, reverb->resampleFlags, reverb->unk_0E, reverb->unk_34);
    return cmd;
}

Acmd* func_800DB680(Acmd* cmd, SynthesisReverb* reverb, int16_t bufIndex) {
    ReverbRingBufferItem* bufItem = &reverb->items[reverb->curFrame][bufIndex];

    aSetBuffer(cmd++, 0, DMEM_WET_LEFT_CH, DMEM_WET_SCRATCH, bufItem->unk_18 * 2);
    aResample(cmd++, reverb->resampleFlags, bufItem->unk_16, reverb->unk_38);

    cmd = AudioSynth_SaveBufferOffset(cmd, DMEM_WET_SCRATCH, bufItem->startPos, bufItem->lengthA, reverb->leftRingBuf);
    if (bufItem->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_SaveBufferOffset(cmd, DMEM_WET_SCRATCH + bufItem->lengthA, 0, bufItem->lengthB,
                                          reverb->leftRingBuf);
    }
    aSetBuffer(cmd++, 0, DMEM_WET_RIGHT_CH, DMEM_WET_SCRATCH, bufItem->unk_18 * 2);
    aResample(cmd++, reverb->resampleFlags, bufItem->unk_16, reverb->unk_3C);
    cmd = AudioSynth_SaveBufferOffset(cmd, DMEM_WET_SCRATCH, bufItem->startPos, bufItem->lengthA, reverb->rightRingBuf);

    if (bufItem->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_SaveBufferOffset(cmd, DMEM_WET_SCRATCH + bufItem->lengthA, 0, bufItem->lengthB,
                                          reverb->rightRingBuf);
    }

    return cmd;
}

Acmd* func_800DB828(Acmd* cmd, int32_t arg1, SynthesisReverb* reverb, int16_t arg3) {
    ReverbRingBufferItem* item = &reverb->items[reverb->curFrame][arg3];

    item->unk_14 = (item->unk_18 << 0xF) / arg1;
    int16_t offsetA = (item->startPos & 7) * 2;
    item->unk_16 = (arg1 << 0xF) / item->unk_18;
    int16_t offsetB = ALIGN16(offsetA + item->lengthA);
    cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_TEMP, item->startPos - (offsetA / 2), DEFAULT_LEN_1CH, reverb);
    if (item->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_TEMP + offsetB, 0, DEFAULT_LEN_1CH - offsetB, reverb);
    }
    aSetBuffer(cmd++, 0, DMEM_WET_TEMP + offsetA, DMEM_WET_LEFT_CH, arg1 * 2);
    aResample(cmd++, reverb->resampleFlags, item->unk_14, reverb->unk_30);
    aSetBuffer(cmd++, 0, DMEM_WET_TEMP + DEFAULT_LEN_1CH + offsetA, DMEM_WET_RIGHT_CH, arg1 * 2);
    aResample(cmd++, reverb->resampleFlags, item->unk_14, reverb->unk_34);
    return cmd;
}

Acmd* AudioSynth_FilterReverb(Acmd* cmd, int32_t count, SynthesisReverb* reverb) {
    // Apply a filter (convolution) to each reverb channel.
    if (reverb->filterLeft != NULL) {
        aFilter(cmd++, 2, count, reverb->filterLeft);
        aFilter(cmd++, reverb->resampleFlags, DMEM_WET_LEFT_CH, reverb->filterLeftState);
    }

    if (reverb->filterRight != NULL) {
        aFilter(cmd++, 2, count, reverb->filterRight);
        aFilter(cmd++, reverb->resampleFlags, DMEM_WET_RIGHT_CH, reverb->filterRightState);
    }
    return cmd;
}

Acmd* AudioSynth_MaybeMixRingBuffer1(Acmd* cmd, SynthesisReverb* reverb, int32_t arg2) {

    SynthesisReverb* temp_a3 = &gAudioContext.synthesisReverbs[reverb->unk_05];
    if (temp_a3->downsampleRate == 1) {
        cmd = AudioSynth_LoadRingBuffer1AtTemp(cmd, temp_a3, arg2);
        aMix(cmd++, 0x34, reverb->unk_08, DMEM_WET_LEFT_CH, DMEM_WET_TEMP);
        cmd = AudioSynth_SaveRingBuffer1AtTemp(cmd, temp_a3, arg2);
    }
    return cmd;
}

void func_800DBB94(void) {
}

void AudioSynth_ClearBuffer(Acmd* cmd, int32_t arg1, int32_t arg2) {
    aClearBuffer(cmd, arg1, arg2);
}

void func_800DBBBC(void) {
}

void func_800DBBC4(void) {
}

void func_800DBBCC(void) {
}

void AudioSynth_Mix(Acmd* cmd, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4) {
    aMix(cmd, arg1, arg2, arg3, arg4);
}

void func_800DBC08(void) {
}

void func_800DBC10(void) {
}

void func_800DBC18(void) {
}

void AudioSynth_SetBuffer(Acmd* cmd, int32_t flags, int32_t dmemIn, int32_t dmemOut, int32_t count) {
    aSetBuffer(cmd, flags, dmemIn, dmemOut, count);
}

void func_800DBC54(void) {
}

void func_800DBC5C(void) {
}

// possible fake match?
void AudioSynth_DMemMove(Acmd* cmd, int32_t dmemIn, int32_t dmemOut, int32_t count) {
    aDMEMMove(cmd, dmemIn, dmemOut, count);
}

void func_800DBC90(void) {
}

void func_800DBC98(void) {
}

void func_800DBCA0(void) {
}

void func_800DBCA8(void) {
}

void AudioSynth_InterL(Acmd* cmd, int32_t dmemIn, int32_t dmemOut, int32_t count) {
    aInterl(cmd, dmemIn, dmemOut, count);
}

void AudioSynth_EnvSetup1(Acmd* cmd, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4) {
    aEnvSetup1(cmd, arg1, arg2, arg3, arg4);
}

void func_800DBD08(void) {
}

void AudioSynth_LoadBuffer(Acmd* cmd, int32_t arg1, int32_t arg2, uintptr_t arg3) {
    aLoadBuffer(cmd, arg3, arg1, arg2);
}

void AudioSynth_SaveBuffer(Acmd* cmd, int32_t arg1, int32_t arg2, uintptr_t arg3) {
    aSaveBuffer(cmd, arg1, arg3, arg2);
}

void AudioSynth_EnvSetup2(Acmd* cmd, int32_t volLeft, int32_t volRight) {
    aEnvSetup2(cmd, volLeft, volRight);
}

void func_800DBD7C(void) {
}

void func_800DBD84(void) {
}

void func_800DBD8C(void) {
}

void AudioSynth_S8Dec(Acmd* cmd, int32_t flags, int16_t* state) {
    aS8Dec(cmd, flags, state);
}

void AudioSynth_HiLoGain(Acmd* cmd, int32_t gain, int32_t dmemIn, int32_t dmemOut, int32_t count) {
    aHiLoGain(cmd, gain, count, dmemIn, dmemOut);
}

void AudioSynth_UnkCmd19(Acmd* cmd, int32_t arg1, int32_t arg2, int32_t arg3, int32_t arg4) {
    aUnkCmd19(cmd, arg1, arg2, arg3, arg4);
}

void func_800DBE18(void) {
}

void func_800DBE20(void) {
}

void func_800DBE28(void) {
}

void func_800DBE30(void) {
}

void AudioSynth_UnkCmd3(Acmd* cmd, int32_t arg1, int32_t arg2, int32_t arg3) {
    aUnkCmd3(cmd, arg1, arg2, arg3);
}

void func_800DBE5C(void) {
}

void func_800DBE64(void) {
}

void func_800DBE6C(void) {
}

void AudioSynth_LoadFilter(Acmd* cmd, int32_t flags, int32_t countOrBuf, uintptr_t addr) {
    aFilter(cmd, flags, countOrBuf, addr);
}

void AudioSynth_LoadFilterCount(Acmd* cmd, int32_t count, uintptr_t addr) {
    aFilter(cmd, 2, count, addr);
}

Acmd* AudioSynth_LoadRingBuffer1(Acmd* cmd, int32_t arg1, SynthesisReverb* reverb, int16_t bufIndex) {
    ReverbRingBufferItem* ringBufferItem = &reverb->items[reverb->curFrame][bufIndex];

    cmd =
        AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_LEFT_CH, ringBufferItem->startPos, ringBufferItem->lengthA, reverb);
    if (ringBufferItem->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_LEFT_CH + ringBufferItem->lengthA, 0, ringBufferItem->lengthB,
                                            reverb);
    }

    return cmd;
}

Acmd* AudioSynth_LoadRingBuffer2(Acmd* cmd, int32_t arg1, SynthesisReverb* reverb, int16_t bufIndex) {
    ReverbRingBufferItem* bufItem = &reverb->items2[reverb->curFrame][bufIndex];

    cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_LEFT_CH, bufItem->startPos, bufItem->lengthA, reverb);
    if (bufItem->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_LoadRingBufferPart(cmd, DMEM_WET_LEFT_CH + bufItem->lengthA, 0, bufItem->lengthB, reverb);
    }
    return cmd;
}

Acmd* AudioSynth_LoadRingBufferPart(Acmd* cmd, uint16_t dmem, uint16_t startPos, int32_t length, SynthesisReverb* reverb) {
    aLoadBuffer(cmd++, &reverb->leftRingBuf[startPos], dmem, length);
    aLoadBuffer(cmd++, &reverb->rightRingBuf[startPos], dmem + DEFAULT_LEN_1CH, length);
    return cmd;
}

Acmd* AudioSynth_SaveRingBufferPart(Acmd* cmd, uint16_t dmem, uint16_t startPos, int32_t length, SynthesisReverb* reverb) {
    aSaveBuffer(cmd++, dmem, &reverb->leftRingBuf[startPos], length);
    aSaveBuffer(cmd++, dmem + DEFAULT_LEN_1CH, &reverb->rightRingBuf[startPos], length);
    return cmd;
}

Acmd* AudioSynth_SaveBufferOffset(Acmd* cmd, uint16_t dmem, uint16_t offset, int32_t length, int16_t* buf) {
    aSaveBuffer(cmd++, dmem, &buf[offset], length);
    return cmd;
}

Acmd* AudioSynth_MaybeLoadRingBuffer2(Acmd* cmd, int32_t arg1, SynthesisReverb* reverb, int16_t bufIndex) {
    if (reverb->downsampleRate == 1) {
        cmd = AudioSynth_LoadRingBuffer2(cmd, arg1, reverb, bufIndex);
    }

    return cmd;
}

Acmd* func_800DC164(Acmd* cmd, int32_t arg1, SynthesisReverb* reverb, int16_t arg3) {
    // Sets DMEM_WET_{LEFT,RIGHT}_CH, clobbers DMEM_TEMP
    if (reverb->downsampleRate == 1) {
        if (reverb->unk_18 != 0) {
            cmd = func_800DB828(cmd, arg1, reverb, arg3);
        } else {
            cmd = AudioSynth_LoadRingBuffer1(cmd, arg1, reverb, arg3);
        }
    } else {
        cmd = func_800DB4E4(cmd, arg1, reverb, arg3);
    }
    return cmd;
}

Acmd* AudioSynth_SaveReverbSamples(Acmd* cmd, SynthesisReverb* reverb, int16_t bufIndex) {
    ReverbRingBufferItem* bufItem = &reverb->items[reverb->curFrame][bufIndex];

    if (reverb->downsampleRate == 1) {
        if (reverb->unk_18 != 0) {
            cmd = func_800DB680(cmd, reverb, bufIndex);
        } else {
            // Put the oldest samples in the ring buffer into the wet channels
            cmd = AudioSynth_SaveRingBufferPart(cmd, DMEM_WET_LEFT_CH, bufItem->startPos, bufItem->lengthA, reverb);
            if (bufItem->lengthB != 0) {
                // Ring buffer wrapped
                cmd = AudioSynth_SaveRingBufferPart(cmd, DMEM_WET_LEFT_CH + bufItem->lengthA, 0, bufItem->lengthB,
                                                    reverb);
            }
        }
    } else {
        // Downsampling is done later by CPU when RSP is done, therefore we need to have
        // double buffering. Left and right buffers are adjacent in memory.
        AudioSynth_SaveBuffer(cmd++, DMEM_WET_LEFT_CH, DEFAULT_LEN_2CH,
                              reverb->items[reverb->curFrame][bufIndex].toDownsampleLeft);
    }

    reverb->resampleFlags = 0;
    return cmd;
}

Acmd* AudioSynth_SaveRingBuffer2(Acmd* cmd, SynthesisReverb* reverb, int16_t bufIndex) {
    ReverbRingBufferItem* bufItem = &reverb->items2[reverb->curFrame][bufIndex];

    cmd = AudioSynth_SaveRingBufferPart(cmd, DMEM_WET_LEFT_CH, bufItem->startPos, bufItem->lengthA, reverb);
    if (bufItem->lengthB != 0) {
        // Ring buffer wrapped
        cmd = AudioSynth_SaveRingBufferPart(cmd, DMEM_WET_LEFT_CH + bufItem->lengthA, 0, bufItem->lengthB, reverb);
    }
    return cmd;
}

Acmd* AudioSynth_DoOneAudioUpdate(int16_t* aiBuf, int32_t aiBufLen, Acmd* cmd, int32_t updateIndex) {
    uint8_t noteIndices[0x5C];
    int16_t reverbIndex;
    int32_t i;
    NoteSubEu* noteSubEu = { 0 };
    int32_t unk14 = { 0 };

    // if (aiBufLen == 0)
    // return;

    int32_t t = gAudioContext.numNotes * updateIndex;
    int16_t count = 0;
    if (gAudioContext.numSynthesisReverbs == 0) {
        for (i = 0; i < gAudioContext.numNotes; i++) {
            if (gAudioContext.noteSubsEu[t + i].bitField0.enabled) {
                noteIndices[count++] = i;
            }
        }
    } else {
        for (reverbIndex = 0; reverbIndex < gAudioContext.numSynthesisReverbs; reverbIndex++) {
            for (i = 0; i < gAudioContext.numNotes; i++) {
                noteSubEu = &gAudioContext.noteSubsEu[t + i];
                if (noteSubEu->bitField0.enabled && noteSubEu->bitField1.reverbIndex == reverbIndex) {
                    noteIndices[count++] = i;
                }
            }
        }

        for (i = 0; i < gAudioContext.numNotes; i++) {
            noteSubEu = &gAudioContext.noteSubsEu[t + i];
            if (noteSubEu->bitField0.enabled && noteSubEu->bitField1.reverbIndex >= gAudioContext.numSynthesisReverbs) {
                noteIndices[count++] = i;
            }
        }
    }

    aClearBuffer(cmd++, DMEM_LEFT_CH, DEFAULT_LEN_2CH);
    i = 0;
    for (reverbIndex = 0; reverbIndex < gAudioContext.numSynthesisReverbs; reverbIndex++) {
        SynthesisReverb* reverb = &gAudioContext.synthesisReverbs[reverbIndex];
        int32_t useReverb = reverb->useReverb;
        if (useReverb) {
            cmd = func_800DC164(cmd, aiBufLen, reverb, updateIndex);
            aMix(cmd++, 0x34, reverb->unk_0A, DMEM_WET_LEFT_CH, DMEM_LEFT_CH);
            unk14 = reverb->unk_14;
            if (unk14) {
                aDMEMMove(cmd++, DMEM_WET_LEFT_CH, DMEM_WET_TEMP, DEFAULT_LEN_2CH);
            }

            aMix(cmd++, 0x34, reverb->unk_0C + 0x8000, DMEM_WET_LEFT_CH, DMEM_WET_LEFT_CH);
            if (reverb->leakRtl != 0 || reverb->leakLtr != 0) {
                cmd = AudioSynth_LeakReverb(cmd, reverb);
            }

            if (unk14) {
                cmd = AudioSynth_SaveReverbSamples(cmd, reverb, updateIndex);
                if (reverb->unk_05 != -1) {
                    cmd = AudioSynth_MaybeMixRingBuffer1(cmd, reverb, updateIndex);
                }
                cmd = AudioSynth_MaybeLoadRingBuffer2(cmd, aiBufLen, reverb, updateIndex);
                aMix(cmd++, 0x34, reverb->unk_16, DMEM_WET_TEMP, DMEM_WET_LEFT_CH);
            }
        }

        while (i < count) {
            NoteSubEu* noteSubEu2 = &gAudioContext.noteSubsEu[noteIndices[i] + t];
            if (noteSubEu2->bitField1.reverbIndex == reverbIndex) {
                cmd = AudioSynth_ProcessNote(noteIndices[i], noteSubEu2,
                                             &gAudioContext.notes[noteIndices[i]].synthesisState, aiBuf, aiBufLen, cmd,
                                             updateIndex);
            } else {
                break;
            }
            i++;
        }

        if (useReverb) {
            if (reverb->filterLeft != NULL || reverb->filterRight != NULL) {
                cmd = AudioSynth_FilterReverb(cmd, aiBufLen * 2, reverb);
            }
            if (unk14) {
                cmd = AudioSynth_SaveRingBuffer2(cmd, reverb, updateIndex);
            } else {
                cmd = AudioSynth_SaveReverbSamples(cmd, reverb, updateIndex);
                if (reverb->unk_05 != -1) {
                    cmd = AudioSynth_MaybeMixRingBuffer1(cmd, reverb, updateIndex);
                }
            }
        }
    }

    while (i < count) {
        cmd = AudioSynth_ProcessNote(noteIndices[i], &gAudioContext.noteSubsEu[t + noteIndices[i]],
                                     &gAudioContext.notes[noteIndices[i]].synthesisState, aiBuf, aiBufLen, cmd,
                                     updateIndex);
        i++;
    }

    updateIndex = aiBufLen * 2;
    aInterleave(cmd++, DMEM_TEMP, DMEM_LEFT_CH, DMEM_RIGHT_CH, updateIndex);
    aSaveBuffer(cmd++, DMEM_TEMP, aiBuf, updateIndex * 2);

    return cmd;
}

Acmd* AudioSynth_ProcessNote(int32_t noteIndex, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int16_t* aiBuf,
                             int32_t aiBufLen, Acmd* cmd, int32_t updateIndex) {
    int32_t restart;
    uint16_t resamplingRateFixedPoint = { 0 };
    int32_t nTrailingSamplesToIgnore = { 0 };
    int32_t phi_a1_2 = { 0 };
    int32_t skipBytes = { 0 };
    void* buf = { 0 };
    int32_t nSamplesToDecode = { 0 };
    uint32_t samplesLenFixedPoint = { 0 };
    int32_t samplesLenAdjusted = { 0 };
    int32_t frameSize = { 0 };
    int32_t nFramesToDecode = { 0 };
    int32_t skipInitialSamples = { 0 };
    intptr_t sampleDataStart = { 0 };
    uint8_t* sampleData = { 0 };
    int32_t nParts = { 0 };
    int32_t curPart;
    int32_t side = { 0 };
    uint16_t noteSamplesDmemAddrBeforeResampling = { 0 };
    uint32_t nSamplesToLoad = { 0 };
    uint16_t unk7 = { 0 };
    uint16_t unkE = { 0 };
    int16_t* filter = { 0 };
    int16_t addr = { 0 };

    int32_t bookOffset = noteSubEu->bitField1.bookOffset;
    int32_t finished = noteSubEu->bitField0.finished;
    Note* note = &gAudioContext.notes[noteIndex];
    int32_t flags = A_CONTINUE;

    if (noteSubEu->bitField0.needsInit == true) {
        flags = A_INIT;
        synthState->restart = 0;
        synthState->samplePosInt = note->unk_BC;
        synthState->samplePosFrac = 0;
        synthState->curVolLeft = 0;
        synthState->curVolRight = 0;
        synthState->prevHeadsetPanRight = 0;
        synthState->prevHeadsetPanLeft = 0;
        synthState->reverbVol = noteSubEu->reverbVol;
        synthState->numParts = 0;
        synthState->unk_1A = 1;
        note->noteSubEu.bitField0.finished = false;
        finished = false;
    }

    resamplingRateFixedPoint = noteSubEu->resamplingRateFixedPoint;
    nParts = noteSubEu->bitField1.hasTwoParts + 1;
    samplesLenFixedPoint = (resamplingRateFixedPoint * aiBufLen * 2) + synthState->samplePosFrac;
    nSamplesToLoad = samplesLenFixedPoint >> 16;
    synthState->samplePosFrac = samplesLenFixedPoint & 0xFFFF;

    // Partially-optimized out no-op ifs required for matching. SM64 decomp
    // makes it clear that this is how it should look.
    if (synthState->numParts == 1 && nParts == 2) {
    } else if (synthState->numParts == 2 && nParts == 1) {
    } else {
    }

    synthState->numParts = nParts;

    if (noteSubEu->bitField1.isSyntheticWave) {
        cmd = AudioSynth_LoadWaveSamples(cmd, noteSubEu, synthState, nSamplesToLoad);
        noteSamplesDmemAddrBeforeResampling = DMEM_UNCOMPRESSED_NOTE + (synthState->samplePosInt * 2);
        synthState->samplePosInt += nSamplesToLoad;
    } else {
        SoundFontSample* audioFontSample = noteSubEu->sound.soundFontSound->sample;

        AdpcmLoop* loopInfo = audioFontSample->loop;
        uintptr_t loopEndPos = loopInfo->loopEnd;
        uintptr_t sampleAddr = audioFontSample->sampleAddr;
        int32_t resampledTempLen = 0;

        for (curPart = 0; curPart < nParts; curPart++) {
            int32_t nSamplesProcessed = 0;
            int32_t s5 = 0;

            if (nParts == 1) {
                samplesLenAdjusted = nSamplesToLoad;
            } else if (nSamplesToLoad & 1) {
                samplesLenAdjusted = (nSamplesToLoad & ~1) + (curPart * 2);
            } else {
                samplesLenAdjusted = nSamplesToLoad;
            }

            if (audioFontSample->codec == CODEC_ADPCM || audioFontSample->codec == CODEC_SMALL_ADPCM) {
                if (gAudioContext.curLoadedBook != audioFontSample->book->book) {
                    uint32_t nEntries = { 0 };
                    switch (bookOffset) {
                        case 1:
                            gAudioContext.curLoadedBook = &D_8012FBA8[1];
                            break;
                        case 2:
                        case 3:
                        default:
                            gAudioContext.curLoadedBook = audioFontSample->book->book;
                            break;
                    }
                    nEntries = 16 * audioFontSample->book->order * audioFontSample->book->npredictors;
                    aLoadADPCM(cmd++, nEntries, gAudioContext.curLoadedBook);
                }
            }

            while (nSamplesProcessed != samplesLenAdjusted) {
                int32_t noteFinished = false;
                restart = false;
                int32_t phi_s4 = 0;

                int32_t nFirstFrameSamplesToIgnore = synthState->samplePosInt & 0xF;
                int32_t nSamplesUntilLoopEnd = loopEndPos - synthState->samplePosInt;
                int32_t nSamplesToProcess = samplesLenAdjusted - nSamplesProcessed;

                if (nFirstFrameSamplesToIgnore == 0 && !synthState->restart) {
                    nFirstFrameSamplesToIgnore = 16;
                }
                int32_t nSamplesInFirstFrame = 16 - nFirstFrameSamplesToIgnore;

                if (nSamplesToProcess < nSamplesUntilLoopEnd) {
                    nFramesToDecode = (int32_t)(nSamplesToProcess - nSamplesInFirstFrame + 15) / 16;
                    nSamplesToDecode = nFramesToDecode * 16;
                    nTrailingSamplesToIgnore = nSamplesInFirstFrame + nSamplesToDecode - nSamplesToProcess;
                } else {
                    nSamplesToDecode = nSamplesUntilLoopEnd - nSamplesInFirstFrame;
                    nTrailingSamplesToIgnore = 0;
                    if (nSamplesToDecode <= 0) {
                        nSamplesToDecode = 0;
                        nSamplesInFirstFrame = nSamplesUntilLoopEnd;
                    }
                    nFramesToDecode = (nSamplesToDecode + 15) / 16;
                    if (loopInfo->count != 0) {
                        // Loop around and restart
                        restart = true;
                    } else {
                        noteFinished = true;
                    }
                }

                switch (audioFontSample->codec) {
                    case CODEC_ADPCM:
                        frameSize = 9;
                        skipInitialSamples = 16;
                        sampleDataStart = 0;
                        break;
                    case CODEC_SMALL_ADPCM:
                        frameSize = 5;
                        skipInitialSamples = 16;
                        sampleDataStart = 0;
                        break;
                    case CODEC_S8:
                        frameSize = 16;
                        skipInitialSamples = 16;
                        sampleDataStart = 0;
                        break;
                    case CODEC_S16_INMEMORY:
                        AudioSynth_ClearBuffer(cmd++, DMEM_UNCOMPRESSED_NOTE, (samplesLenAdjusted * 2) + 0x20);
                        flags = A_CONTINUE;
                        skipBytes = 0;
                        nSamplesProcessed = samplesLenAdjusted;
                        s5 = samplesLenAdjusted;
                        goto skip;
                    case CODEC_S16:
                    case CODEC_OPUS:
                        AudioSynth_ClearBuffer(cmd++, DMEM_UNCOMPRESSED_NOTE, (samplesLenAdjusted + 16) * 2);
                        flags = A_CONTINUE;
                        skipBytes = 0;
                        size_t bytesToRead = { 0 };
                        nSamplesProcessed += samplesLenAdjusted;

                        if (((synthState->samplePosInt * 2) + (samplesLenAdjusted)*2) < audioFontSample->size) {
                            bytesToRead = (samplesLenAdjusted)*2;
                        } else {
                            bytesToRead = audioFontSample->size - (synthState->samplePosInt * 2);
                        }
                        // 2S2H [Port] [Custom audio] Handle decoding OPUS data
                        if (audioFontSample->codec == CODEC_OPUS) {
                            aOPUSdecImpl(sampleAddr, DMEM_UNCOMPRESSED_NOTE, bytesToRead, &synthState->opusFile,
                                         synthState->samplePosInt, audioFontSample->fileSize);
                        } else {
                            aLoadBuffer(cmd++, sampleAddr + (synthState->samplePosInt * 2), DMEM_UNCOMPRESSED_NOTE,
                                        bytesToRead);
                        }

                        goto skip;
                    case CODEC_REVERB:
                        break;
                }

                if (nFramesToDecode != 0) {
                    int32_t frameIndex = (synthState->samplePosInt + skipInitialSamples - nFirstFrameSamplesToIgnore) / 16;
                    intptr_t sampleDataOffset = frameIndex * frameSize;
                    if (audioFontSample->medium == MEDIUM_RAM) {
                        sampleData = (uint8_t*)(sampleDataStart + sampleDataOffset + sampleAddr);
                    } else if (audioFontSample->medium == MEDIUM_UNK) {
                        return cmd;
                    } else {
                        sampleData = AudioLoad_DmaSampleData(sampleDataStart + sampleDataOffset + sampleAddr,
                                                             ALIGN16((nFramesToDecode * frameSize) + 0x10), flags,
                                                             &synthState->sampleDmaIndex, audioFontSample->medium);
                    }

                    if (sampleData == NULL) {
                        return cmd;
                    }

                    addr = DMEM_COMPRESSED_ADPCM_DATA - ALIGN16(nFramesToDecode * frameSize);
                    uint32_t bytesToLoad = nFramesToDecode * frameSize;
                    uint32_t bytesAvail = (uint32_t)sampleDataOffset < audioFontSample->size
                                         ? audioFontSample->size - (uint32_t)sampleDataOffset
                                         : 0;
                    if (bytesToLoad > bytesAvail) {
                        bytesToLoad = bytesAvail;
                    }
                    aLoadBuffer(cmd++, sampleData, addr, bytesToLoad);
                } else {
                    nSamplesToDecode = 0;
                }

                if (synthState->restart) {
                    aSetLoop(cmd++, audioFontSample->loop->predictorState);
                    flags = A_LOOP;
                    synthState->restart = false;
                }

                int32_t nSamplesInThisIteration = nSamplesToDecode + nSamplesInFirstFrame - nTrailingSamplesToIgnore;
                if (nSamplesProcessed == 0) {
                    skipBytes = nFirstFrameSamplesToIgnore * 2;
                } else {
                    phi_s4 = ALIGN16(s5 + 16);
                }
                switch (audioFontSample->codec) {
                    case CODEC_ADPCM:
                        addr = DMEM_COMPRESSED_ADPCM_DATA - ALIGN16(nFramesToDecode * frameSize);
                        aSetBuffer(cmd++, 0, addr, DMEM_UNCOMPRESSED_NOTE + phi_s4, nSamplesToDecode * 2);
                        aADPCMdec(cmd++, flags, synthState->synthesisBuffers->adpcmdecState);
                        break;
                    case CODEC_SMALL_ADPCM:
                        addr = DMEM_COMPRESSED_ADPCM_DATA - ALIGN16(nFramesToDecode * frameSize);
                        aSetBuffer(cmd++, 0, addr, DMEM_UNCOMPRESSED_NOTE + phi_s4, nSamplesToDecode * 2);
                        aADPCMdec(cmd++, flags | 4, synthState->synthesisBuffers->adpcmdecState);
                        break;
                    case CODEC_S8:
                        addr = DMEM_COMPRESSED_ADPCM_DATA - ALIGN16(nFramesToDecode * frameSize);
                        AudioSynth_SetBuffer(cmd++, 0, addr, DMEM_UNCOMPRESSED_NOTE + phi_s4, nSamplesToDecode * 2);
                        AudioSynth_S8Dec(cmd++, flags, synthState->synthesisBuffers->adpcmdecState);
                        break;
                }

                if (nSamplesProcessed != 0) {
                    aDMEMMove(cmd++, DMEM_UNCOMPRESSED_NOTE + phi_s4 + (nFirstFrameSamplesToIgnore * 2),
                              DMEM_UNCOMPRESSED_NOTE + s5, nSamplesInThisIteration * 2);
                }

                nSamplesProcessed += nSamplesInThisIteration;

                switch (flags) {
                    case A_INIT:
                        skipBytes = 0x20;
                        s5 = (nSamplesToDecode + 0x10) * 2;
                        break;
                    case A_LOOP:
                        s5 = nSamplesInThisIteration * 2 + s5;
                        break;
                    default:
                        if (s5 != 0) {
                            s5 = nSamplesInThisIteration * 2 + s5;
                        } else {
                            s5 = (nFirstFrameSamplesToIgnore + nSamplesInThisIteration) * 2;
                        }
                        break;
                }

                flags = A_CONTINUE;

            skip:
                if (noteFinished) {
                    AudioSynth_ClearBuffer(cmd++, DMEM_UNCOMPRESSED_NOTE + s5,
                                           (samplesLenAdjusted - nSamplesProcessed) * 2);
                    finished = true;
                    note->noteSubEu.bitField0.finished = true;
                    func_800DB2C0(updateIndex, noteIndex);
                    break;
                } else {
                    if (restart) {
                        synthState->restart = true;
                        synthState->samplePosInt = loopInfo->start;
                    } else {
                        synthState->samplePosInt += nSamplesToProcess;
                    }
                }
            }

            switch (nParts) {
                case 1:
                    noteSamplesDmemAddrBeforeResampling = DMEM_UNCOMPRESSED_NOTE + skipBytes;
                    break;
                case 2:
                    switch (curPart) {
                        case 0:
                            AudioSynth_InterL(cmd++, DMEM_UNCOMPRESSED_NOTE + skipBytes, DMEM_TEMP + 0x20,
                                              ((samplesLenAdjusted / 2) + 7) & ~7);
                            resampledTempLen = samplesLenAdjusted;
                            noteSamplesDmemAddrBeforeResampling = DMEM_TEMP + 0x20;
                            if (finished) {
                                AudioSynth_ClearBuffer(cmd++, noteSamplesDmemAddrBeforeResampling + resampledTempLen,
                                                       samplesLenAdjusted + 0x10);
                            }
                            break;
                        case 1:
                            AudioSynth_InterL(cmd++, DMEM_UNCOMPRESSED_NOTE + skipBytes,
                                              DMEM_TEMP + 0x20 + resampledTempLen, ((samplesLenAdjusted / 2) + 7) & ~7);
                            break;
                    }
            }
            if (finished) {
                break;
            }
        }
    }

    flags = A_CONTINUE;
    if (noteSubEu->bitField0.needsInit == true) {
        noteSubEu->bitField0.needsInit = false;
        flags = A_INIT;
    }

    cmd = AudioSynth_FinalResample(cmd, synthState, aiBufLen * 2, resamplingRateFixedPoint,
                                   noteSamplesDmemAddrBeforeResampling, flags);
    if (bookOffset == 3) {
        AudioSynth_UnkCmd19(cmd++, DMEM_TEMP, DMEM_TEMP, aiBufLen * 2, 0);
    }

    if (bookOffset == 2) {
        AudioSynth_UnkCmd3(cmd++, DMEM_TEMP, DMEM_TEMP, aiBufLen * 2);
    }

    phi_a1_2 = noteSubEu->unk_2;
    if (phi_a1_2 != 0) {
        if (phi_a1_2 < 0x10) {
            phi_a1_2 = 0x10;
        }
        AudioSynth_HiLoGain(cmd++, phi_a1_2, DMEM_TEMP, 0, (aiBufLen * 2) + 0x20);
    }

    filter = noteSubEu->filter;
    if (filter != 0) {
        AudioSynth_LoadFilterCount(cmd++, aiBufLen * 2, filter);
        AudioSynth_LoadFilter(cmd++, flags, DMEM_TEMP, synthState->synthesisBuffers->mixEnvelopeState);
    }

    unk7 = noteSubEu->unk_07;
    unkE = noteSubEu->unk_0E;
    buf = &synthState->synthesisBuffers->panSamplesBuffer[0x18];
    if (unk7 != 0 && noteSubEu->unk_0E != 0) {
        AudioSynth_DMemMove(cmd++, DMEM_TEMP, DMEM_SCRATCH2, aiBufLen * 2);
        int32_t thing = DMEM_SCRATCH2 - unk7;
        if (synthState->unk_1A != 0) {
            AudioSynth_ClearBuffer(cmd++, thing, unk7);
            synthState->unk_1A = 0;
        } else {
            AudioSynth_LoadBuffer(cmd++, thing, unk7, buf);
        }
        AudioSynth_SaveBuffer(cmd++, DMEM_TEMP + (aiBufLen * 2) - unk7, unk7, buf);
        AudioSynth_Mix(cmd++, (aiBufLen * 2) >> 4, unkE, DMEM_SCRATCH2, thing);
        AudioSynth_DMemMove(cmd++, thing, DMEM_TEMP, aiBufLen * 2);
    } else {
        synthState->unk_1A = 1;
    }

    if (noteSubEu->headsetPanRight != 0 || synthState->prevHeadsetPanRight != 0) {
        side = 1;
    } else if (noteSubEu->headsetPanLeft != 0 || synthState->prevHeadsetPanLeft != 0) {
        side = 2;
    } else {
        side = 0;
    }
    cmd = AudioSynth_ProcessEnvelope(cmd, noteSubEu, synthState, aiBufLen, DMEM_TEMP, side, flags);
    if (noteSubEu->bitField1.usesHeadsetPanEffects2) {
        if (!(flags & A_INIT)) {
            flags = A_CONTINUE;
        }
        cmd = AudioSynth_NoteApplyHeadsetPanEffects(cmd, noteSubEu, synthState, aiBufLen * 2, flags, side);
    }
    return cmd;
}

Acmd* AudioSynth_FinalResample(Acmd* cmd, NoteSynthesisState* synthState, int32_t count, uint16_t pitch, uint16_t inpDmem,
                               int32_t resampleFlags) {
    if (pitch == 0) {
        AudioSynth_ClearBuffer(cmd++, DMEM_TEMP, count);
    } else {
        aSetBuffer(cmd++, 0, inpDmem, DMEM_TEMP, count);
        aResample(cmd++, resampleFlags, pitch, synthState->synthesisBuffers->finalResampleState);
    }
    return cmd;
}

Acmd* AudioSynth_ProcessEnvelope(Acmd* cmd, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int32_t aiBufLen,
                                 uint16_t inBuf, int32_t headsetPanSettings, int32_t flags) {
    uint32_t phi_a1 = { 0 };
    int32_t phi_t1 = { 0 };
    int16_t rampLeft = { 0 };
    int16_t rampRight = { 0 };
    int16_t rampReverb = { 0 };
    int16_t sourceReverbVol = { 0 };

    uint16_t curVolLeft = synthState->curVolLeft;
    uint16_t targetVolLeft = noteSubEu->targetVolLeft;
    targetVolLeft <<= 4;
    int16_t reverbVol = noteSubEu->reverbVol;
    uint16_t curVolRight = synthState->curVolRight;
    uint16_t targetVolRight = noteSubEu->targetVolRight;
    targetVolRight <<= 4;

    if (targetVolLeft != curVolLeft) {
        rampLeft = (targetVolLeft - curVolLeft) / (aiBufLen >> 3);
    } else {
        rampLeft = 0;
    }
    if (targetVolRight != curVolRight) {
        rampRight = (targetVolRight - curVolRight) / (aiBufLen >> 3);
    } else {
        rampRight = 0;
    }

    sourceReverbVol = synthState->reverbVol;
    phi_t1 = sourceReverbVol & 0x7F;

    if (sourceReverbVol != reverbVol) {
        rampReverb = (((reverbVol & 0x7F) - phi_t1) << 9) / (aiBufLen >> 3);
        synthState->reverbVol = reverbVol;
    } else {
        rampReverb = 0;
    }

    synthState->curVolLeft = curVolLeft + (rampLeft * (aiBufLen >> 3));
    synthState->curVolRight = curVolRight + (rampRight * (aiBufLen >> 3));

    if (noteSubEu->bitField1.usesHeadsetPanEffects2) {
        AudioSynth_ClearBuffer(cmd++, DMEM_NOTE_PAN_TEMP, DEFAULT_LEN_1CH);
        AudioSynth_EnvSetup1(cmd++, phi_t1 * 2, rampReverb, rampLeft, rampRight);
        AudioSynth_EnvSetup2(cmd++, curVolLeft, curVolRight);
        switch (headsetPanSettings) {
            case 1:
                phi_a1 = D_801304A4;
                break;
            case 2:
                phi_a1 = D_801304A8;
                break;
            default:
                phi_a1 = D_801304AC;
                break;
        }
    } else {
        aEnvSetup1(cmd++, phi_t1 * 2, rampReverb, rampLeft, rampRight);
        aEnvSetup2(cmd++, curVolLeft, curVolRight);
        phi_a1 = D_801304AC;
    }

    aEnvMixer(cmd++, inBuf, aiBufLen, (sourceReverbVol & 0x80) >> 7, noteSubEu->bitField0.stereoHeadsetEffects,
              noteSubEu->bitField0.usesHeadsetPanEffects, noteSubEu->bitField0.stereoStrongRight,
              noteSubEu->bitField0.stereoStrongLeft, phi_a1, D_801304A0);
    return cmd;
}

Acmd* AudioSynth_LoadWaveSamples(Acmd* cmd, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int32_t nSamplesToLoad) {
    int32_t unk6 = noteSubEu->unk_06;
    int32_t samplePosInt = synthState->samplePosInt;

    if (noteSubEu->bitField1.bookOffset != 0) {
        AudioSynth_LoadBuffer(cmd++, DMEM_UNCOMPRESSED_NOTE, ALIGN16(nSamplesToLoad * 2), gWaveSamples[8]);
        gWaveSamples[8] += nSamplesToLoad * 2;
        return cmd;
    } else {
        aLoadBuffer(cmd++, noteSubEu->sound.samples, DMEM_UNCOMPRESSED_NOTE, 0x80);
        if (unk6 != 0) {
            samplePosInt = (samplePosInt * D_801304C0[unk6 >> 2]) / D_801304C0[unk6 & 3];
        }
        samplePosInt &= 0x3F;
        int32_t temp_v0 = 0x40 - samplePosInt;
        if (temp_v0 < nSamplesToLoad) {
            int32_t repeats = ((nSamplesToLoad - temp_v0 + 0x3F) / 0x40);
            if (repeats != 0) {
                aDuplicate(cmd++, repeats, DMEM_UNCOMPRESSED_NOTE, DMEM_UNCOMPRESSED_NOTE + 0x80);
            }
        }
        synthState->samplePosInt = samplePosInt;
    }
    return cmd;
}

Acmd* AudioSynth_NoteApplyHeadsetPanEffects(Acmd* cmd, NoteSubEu* noteSubEu, NoteSynthesisState* synthState, int32_t bufLen,
                                            int32_t flags, int32_t side) {
    uint16_t dest = { 0 };
    uint8_t prevPanShift = { 0 };
    uint8_t panShift = { 0 };

    switch (side) {
        case 1:
            dest = DMEM_LEFT_CH;
            panShift = noteSubEu->headsetPanRight;
            prevPanShift = synthState->prevHeadsetPanRight;
            synthState->prevHeadsetPanLeft = 0;
            synthState->prevHeadsetPanRight = panShift;
            break;
        case 2:
            dest = DMEM_RIGHT_CH;
            panShift = noteSubEu->headsetPanLeft;
            prevPanShift = synthState->prevHeadsetPanLeft;
            synthState->prevHeadsetPanLeft = panShift;
            synthState->prevHeadsetPanRight = 0;
            break;
        default:
            return cmd;
    }

    if (flags != A_INIT) {
        // Slightly adjust the sample rate in order to fit a change in pan shift
        if (panShift != prevPanShift) {
            uint16_t pitch = (((bufLen << 0xF) / 2) - 1) / ((bufLen + panShift - prevPanShift - 2) / 2);
            aSetBuffer(cmd++, 0, DMEM_NOTE_PAN_TEMP, DMEM_TEMP, bufLen + panShift - prevPanShift);
            aResampleZoh(cmd++, pitch, 0);
        } else {
            aDMEMMove(cmd++, DMEM_NOTE_PAN_TEMP, DMEM_TEMP, bufLen);
        }

        if (prevPanShift != 0) {
            aLoadBuffer(cmd++, &synthState->synthesisBuffers->panResampleState[0x8], DMEM_NOTE_PAN_TEMP,
                        ALIGN16(prevPanShift));
            aDMEMMove(cmd++, DMEM_TEMP, DMEM_NOTE_PAN_TEMP + prevPanShift, bufLen + panShift - prevPanShift);
        } else {
            aDMEMMove(cmd++, DMEM_TEMP, DMEM_NOTE_PAN_TEMP, bufLen + panShift);
        }
    } else {
        // Just shift right
        aDMEMMove(cmd++, DMEM_NOTE_PAN_TEMP, DMEM_TEMP, bufLen);
        aClearBuffer(cmd++, DMEM_NOTE_PAN_TEMP, panShift);
        aDMEMMove(cmd++, DMEM_TEMP, DMEM_NOTE_PAN_TEMP + panShift, bufLen);
    }

    if (panShift) {
        // Save excessive samples for next iteration
        aSaveBuffer(cmd++, DMEM_NOTE_PAN_TEMP + bufLen, &synthState->synthesisBuffers->panResampleState[0x8],
                    ALIGN16(panShift));
    }
    aAddMixer(cmd++, ALIGN64(bufLen), DMEM_NOTE_PAN_TEMP, dest);
    return cmd;
}
