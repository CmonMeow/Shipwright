#include <runtime/log/Log.hpp>
#include "engine/audio/Audio.h"
#include "engine/config/Config.h"
#ifdef __APPLE__
#include "engine/audio/CoreAudioAudioPlayer.h"
#endif

#include "engine/Context.h"

namespace Engine {

Audio::~Audio() {
    WriteLog("destruct audio");
}

void Audio::InitAudioPlayer() {
    switch (mAudioBackend) {
#ifdef _WIN32
        case AudioBackend::WASAPI:
            mAudioPlayer = std::make_shared<WasapiAudioPlayer>(this->mAudioSettings);
            break;
#endif
#ifdef __APPLE__
        case AudioBackend::COREAUDIO:
            mAudioPlayer = std::make_shared<CoreAudioAudioPlayer>(this->mAudioSettings);
            break;
#endif
        default:
            mAudioPlayer = std::make_shared<NullAudioPlayer>(this->mAudioSettings);
            break;
    }

    if (mAudioPlayer && !mAudioPlayer->Init()) {
        // Keep the requested native backend in configuration so a later launch can retry after a device change.
        WriteLog("Native audio initialization failed; using silent fallback for this session");
        mAudioPlayer = std::make_shared<NullAudioPlayer>(this->mAudioSettings);
        mAudioPlayer->Init();
    }
}

void Audio::Init() {
#ifdef _WIN32
    mAudioBackend = AudioBackend::WASAPI;
#elif defined(__APPLE__)
    mAudioBackend = AudioBackend::COREAUDIO;
#else
    mAudioBackend = AudioBackend::NUL;
#endif
    InitAudioPlayer();
}

std::shared_ptr<AudioPlayer> Audio::GetAudioPlayer() {
    return mAudioPlayer;
}

void Audio::SetAudioChannels(AudioChannelsSetting channels) {
    if (mAudioSettings.ChannelSetting != channels) {
        mAudioSettings.ChannelSetting = channels;
        // Reinitialize the existing audio player with the new channel configuration
        if (mAudioPlayer) {
            mAudioPlayer->SetAudioChannels(channels);
        }
    }
}

AudioChannelsSetting Audio::GetAudioChannels() const {
    return mAudioSettings.ChannelSetting;
}

} // namespace Engine
