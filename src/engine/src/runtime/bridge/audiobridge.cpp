#include "runtime/bridge/audiobridge.h"
#include "engine/Context.h"
#include "engine/audio/Audio.h"

extern "C" {

int32_t AudioPlayerBuffered() {
    auto audio = Engine::Context::GetInstance()->GetAudio()->GetAudioPlayer();
    if (audio == nullptr) {
        return 0;
    }

    if (!audio->IsInitialized()) {
        return 0;
    }

    return audio->Buffered();
}

int32_t AudioPlayerGetDesiredBuffered() {
    auto audio = Engine::Context::GetInstance()->GetAudio()->GetAudioPlayer();
    if (audio == nullptr) {
        return 0;
    }

    if (!audio->IsInitialized()) {
        return 0;
    }

    return audio->GetDesiredBuffered();
}

AudioChannelsSetting GetAudioChannels() {
    auto audio = Engine::Context::GetInstance()->GetAudio()->GetAudioPlayer();

    if (audio == nullptr) {
        return audioStereo;
    }

    return audio->GetAudioChannels();
}

int32_t GetNumAudioChannels() {
    auto audio = Engine::Context::GetInstance()->GetAudio()->GetAudioPlayer();

    if (audio == nullptr) {
        return 2;
    }

    return audio->GetNumOutputChannels();
}

void AudioPlayerPlayFrame(const uint8_t* buf, size_t len) {
    auto audio = Engine::Context::GetInstance()->GetAudio()->GetAudioPlayer();
    if (audio == nullptr) {
        return;
    }

    if (!audio->IsInitialized()) {
        return;
    }

    audio->Play(buf, len);
}

void SetAudioChannels(AudioChannelsSetting channels) {
    auto audio = Engine::Context::GetInstance()->GetAudio();
    if (audio == nullptr) {
        return;
    }

    audio->SetAudioChannels(channels);
}
}
