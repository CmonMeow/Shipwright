#include "port/resource/importer/AudioSoundFontFactory.h"
#include "port/resource/type/AudioSoundFont.h"
#include <runtime/libultra.h>
#include "z64audio.h"

namespace SOH {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryAudioSoundFontV2::ReadResource(std::shared_ptr<Engine::File> file,
                                                    std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto audioSoundFont = std::make_shared<AudioSoundFont>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    audioSoundFont->soundFont.fntIndex = reader->ReadInt32();
    audioSoundFont->medium = reader->ReadInt8();
    audioSoundFont->cachePolicy = reader->ReadInt8();

    audioSoundFont->data1 = reader->ReadUInt16();
    audioSoundFont->soundFont.sampleBankId1 = audioSoundFont->data1 >> 8;
    audioSoundFont->soundFont.sampleBankId2 = audioSoundFont->data1 & 0xFF;

    audioSoundFont->data2 = reader->ReadUInt16();
    audioSoundFont->data3 = reader->ReadUInt16();

    uint32_t drumCount = reader->ReadUInt32();
    audioSoundFont->soundFont.numDrums = drumCount;

    uint32_t instrumentCount = reader->ReadUInt32();
    audioSoundFont->soundFont.numInstruments = instrumentCount;

    uint32_t soundEffectCount = reader->ReadUInt32();
    audioSoundFont->soundFont.numSfx = soundEffectCount;

    // 🥁 DRUMS 🥁
    // audioSoundFont->drums.reserve(audioSoundFont->soundFont.numDrums);
    audioSoundFont->drumAddresses.reserve(audioSoundFont->soundFont.numDrums);
    audioSoundFont->drumSamplePaths.reserve(audioSoundFont->soundFont.numDrums);
    for (uint32_t i = 0; i < audioSoundFont->soundFont.numDrums; i++) {
        Drum* drum = new Drum{};
        drum->releaseRate = reader->ReadUByte();
        drum->pan = reader->ReadUByte();
        drum->loaded = reader->ReadUByte();
        drum->loaded = 0; // this was always getting set to zero in ResourceMgr_LoadAudioSoundFontByName

        uint32_t envelopeCount = reader->ReadUInt32();
        drum->envelope = new AdsrEnvelope[envelopeCount];
        for (uint32_t j = 0; j < envelopeCount; j++) {
            int16_t delay = reader->ReadInt16();
            int16_t arg = reader->ReadInt16();

            drum->envelope[j].delay = BE16SWAP(delay);
            drum->envelope[j].arg = BE16SWAP(arg);
        }

        bool hasSample = reader->ReadInt8() != 0;
        std::string sampleFileName = reader->ReadString();
        drum->sound.tuning = reader->ReadFloat();

        // audioSoundFont->drums.push_back(drum);
        //  BENTODO clean this up in V3.
        if (!hasSample || sampleFileName.empty()) {
            delete[] drum->envelope;
            delete drum;
            audioSoundFont->drumAddresses.push_back(nullptr);
            audioSoundFont->drumSamplePaths.emplace_back();
        } else {
            audioSoundFont->drumAddresses.push_back(drum);
            audioSoundFont->drumSamplePaths.push_back(std::move(sampleFileName));
        }
    }
    audioSoundFont->soundFont.drums = audioSoundFont->drumAddresses.data();

    // 🎺🎻🎷🎸🎹 INSTRUMENTS 🎹🎸🎷🎻🎺
    for (uint32_t i = 0; i < audioSoundFont->soundFont.numInstruments; i++) {
        Instrument* instrument = new Instrument{};
        AudioSoundFont::InstrumentSamplePaths samplePaths;

        uint8_t isValidEntry = reader->ReadUByte();
        instrument->loaded = reader->ReadUByte();
        instrument->loaded = 0; // this was always getting set to zero in ResourceMgr_LoadAudioSoundFontByName

        instrument->normalRangeLo = reader->ReadUByte();
        instrument->normalRangeHi = reader->ReadUByte();
        instrument->releaseRate = reader->ReadUByte();

        uint32_t envelopeCount = reader->ReadInt32();
        instrument->envelope = new AdsrEnvelope[envelopeCount];

        for (uint32_t j = 0; j < envelopeCount; j++) {
            int16_t delay = reader->ReadInt16();
            int16_t arg = reader->ReadInt16();

            instrument->envelope[j].delay = BE16SWAP(delay);
            instrument->envelope[j].arg = BE16SWAP(arg);
        }

        bool hasLowNoteSoundFontEntry = reader->ReadInt8();
        if (hasLowNoteSoundFontEntry) {
            reader->ReadInt8();
            samplePaths.low = reader->ReadString();
            instrument->lowNotesSound.tuning = reader->ReadFloat();
        } else {
            instrument->lowNotesSound.tuning = 0;
        }

        bool hasNormalNoteSoundFontEntry = reader->ReadInt8();
        if (hasNormalNoteSoundFontEntry) {
            // BENTODO remove in V3
            reader->ReadInt8();
            samplePaths.normal = reader->ReadString();
            instrument->normalNotesSound.tuning = reader->ReadFloat();
        } else {
            instrument->normalNotesSound.tuning = 0;
        }

        bool hasHighNoteSoundFontEntry = reader->ReadInt8();
        if (hasHighNoteSoundFontEntry) {
            reader->ReadInt8();
            samplePaths.high = reader->ReadString();
            instrument->highNotesSound.tuning = reader->ReadFloat();
        } else {
            instrument->highNotesSound.tuning = 0;
        }

        if (isValidEntry) {
            audioSoundFont->instrumentAddresses.push_back(instrument);
            audioSoundFont->instrumentSamplePaths.push_back(std::move(samplePaths));
        } else {
            delete[] instrument->envelope;
            delete instrument;
            audioSoundFont->instrumentAddresses.push_back(nullptr);
            audioSoundFont->instrumentSamplePaths.emplace_back();
        }
    }
    audioSoundFont->soundFont.instruments = audioSoundFont->instrumentAddresses.data();

    // 🔊 SOUND EFFECTS 🔊
    audioSoundFont->soundEffects.reserve(audioSoundFont->soundFont.numSfx);
    audioSoundFont->soundEffectSamplePaths.reserve(audioSoundFont->soundFont.numSfx);
    for (uint32_t i = 0; i < audioSoundFont->soundFont.numSfx; i++) {
        SoundFontSound soundEffect = {};
        std::string sampleFileName;

        bool hasSFEntry = reader->ReadInt8();
        if (hasSFEntry) {
            reader->ReadInt8();
            sampleFileName = reader->ReadString();
            soundEffect.tuning = reader->ReadFloat();
        }

        audioSoundFont->soundEffects.push_back(soundEffect);
        audioSoundFont->soundEffectSamplePaths.push_back(std::move(sampleFileName));
    }
    audioSoundFont->soundFont.soundEffects = audioSoundFont->soundEffects.data();

    return audioSoundFont;
}

} // namespace SOH
