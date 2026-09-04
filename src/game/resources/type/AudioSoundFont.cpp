#include "AudioSoundFont.h"
#include <engine/resource/ResourceManager.h>
#include <runtime/log/Log.hpp>

#include <mutex>
#include <unordered_set>

namespace Game::Resources {

Sample* AudioSoundFont::ResolveSample(const std::string& path) {
    static std::mutex sampleLogMutex;
    static std::unordered_set<std::string> usedSamples;
    static std::unordered_set<std::string> missingSamples;

    if (path.empty()) {
        return nullptr;
    }
    const auto initData = GetInitData();
    if (initData == nullptr || initData->Manager == nullptr) {
        return nullptr;
    }
    auto resource = initData->Manager->LoadResourceProcess(path);
    Sample* sample = static_cast<Sample*>(resource ? resource->GetRawPointer() : nullptr);
    {
        std::scoped_lock lock(sampleLogMutex);
        if (sample != nullptr) {
            if (usedSamples.insert(path).second) {
                Error("Used audio sample: %s", path.c_str());
            }
            missingSamples.erase(path);
        } else {
            if (missingSamples.insert(path).second) {
                Error("Missing optional audio sample: %s", path.c_str());
            }
        }
    }
    return sample;
}

AudioSoundFont::~AudioSoundFont() {
    for (auto i : instrumentAddresses) {
        if (i != nullptr) {
            delete[] i->envelope;
            delete i;
        }
    }

    for (auto d : drumAddresses) {
        if (d != nullptr) {
            delete[] d->envelope;
            delete d;
        }
    }
}

SoundFont* AudioSoundFont::GetPointer() {
    return &soundFont;
}

size_t AudioSoundFont::GetPointerSize() {
    return sizeof(SoundFont);
}

Drum* AudioSoundFont::ResolveDrumSample(size_t index) {
    if (index >= drumAddresses.size()) {
        return nullptr;
    }
    Drum* drum = drumAddresses[index];
    if (drum != nullptr && drum->sound.sample == nullptr && index < drumSamplePaths.size()) {
        drum->sound.sample = ResolveSample(drumSamplePaths[index]);
    }
    return drum != nullptr && drum->sound.sample != nullptr ? drum : nullptr;
}

Instrument* AudioSoundFont::ResolveInstrumentSamples(size_t index) {
    if (index >= instrumentAddresses.size()) {
        return nullptr;
    }
    Instrument* instrument = instrumentAddresses[index];
    if (instrument == nullptr || index >= instrumentSamplePaths.size()) {
        return instrument;
    }

    const InstrumentSamplePaths& paths = instrumentSamplePaths[index];
    if (instrument->lowNotesSound.sample == nullptr) {
        instrument->lowNotesSound.sample = ResolveSample(paths.low);
    }
    if (instrument->normalNotesSound.sample == nullptr) {
        instrument->normalNotesSound.sample = ResolveSample(paths.normal);
    }
    if (instrument->highNotesSound.sample == nullptr) {
        instrument->highNotesSound.sample = ResolveSample(paths.high);
    }
    return instrument;
}

SoundFontSound* AudioSoundFont::ResolveSoundEffectSample(size_t index) {
    if (index >= soundEffects.size()) {
        return nullptr;
    }

    SoundFontSound& sound = soundEffects[index];
    if (sound.sample != nullptr || index >= soundEffectSamplePaths.size() || soundEffectSamplePaths[index].empty()) {
        return &sound;
    }

    sound.sample = ResolveSample(soundEffectSamplePaths[index]);
    return &sound;
}
} // namespace Game::Resources
