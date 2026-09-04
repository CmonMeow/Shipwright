#include <runtime/log/Log.hpp>
#include "engine/audio/Audio.h"
#include "engine/audio/AudioPlayer.h"
#include "engine/audio/NullAudioPlayer.h"
#include "engine/audio/WasapiAudioPlayer.h"

namespace Engine {

Audio::~Audio() {
    WriteLog("destruct audio");
}

void Audio::InitAudioPlayer() {
    mAudioPlayer = std::make_shared<WasapiAudioPlayer>(mAudioSettings);

    if (mAudioPlayer && !mAudioPlayer->Init()) {
        // Keep the requested native backend in configuration so a later launch can retry after a device change.
        WriteLog("Native audio initialization failed; using silent fallback for this session");
        mAudioPlayer = std::make_shared<NullAudioPlayer>(this->mAudioSettings);
        mAudioPlayer->Init();
    }
}

void Audio::Init() {
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
