#include "platform/client/NativeAudioWorker.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include <engine/audio/Audio.h>
#include <engine/audio/AudioPlayer.h>

#include "variables.h"

extern "C" void AudioMgr_CreateNextAudioBuffer(int16_t* samples, uint32_t sampleCount);

namespace Game::Client {
namespace {

constexpr uint32_t SamplesHigh = 560;
constexpr uint32_t SamplesLow = 528;
constexpr uint32_t AudioChannels = 2;
constexpr uint32_t MaximumFramesPerUpdate = 3;

} // namespace

NativeAudioWorker::NativeAudioWorker(Engine::Audio& audio) : mAudio(audio) {
}

NativeAudioWorker::~NativeAudioWorker() {
    Stop();
}

void NativeAudioWorker::Start() {
    std::lock_guard lock(mMutex);
    if (mRunning) {
        return;
    }

    mRunning = true;
    mThread = std::thread(&NativeAudioWorker::Run, this);
}

void NativeAudioWorker::Stop() {
    {
        std::lock_guard lock(mMutex);
        if (!mThread.joinable()) {
            return;
        }
        mRunning = false;
    }
    mWorkAvailable.notify_all();
    mThread.join();
}

void NativeAudioWorker::BeginFrame() {
    {
        std::lock_guard lock(mMutex);
        if (!mRunning) {
            return;
        }
        mProcessing = true;
    }
    mWorkAvailable.notify_one();
}

void NativeAudioWorker::WaitForFrame() {
    std::unique_lock lock(mMutex);
    mWorkComplete.wait(lock, [this] { return !mProcessing || !mRunning; });
}

void NativeAudioWorker::Run() {
    std::unique_lock lock(mMutex);
    while (mRunning) {
        mWorkAvailable.wait(lock, [this] { return mProcessing || !mRunning; });
        if (!mRunning) {
            break;
        }

        lock.unlock();

        auto audioPlayer = mAudio.GetAudioPlayer();
        const int bufferedSamples = audioPlayer && audioPlayer->IsInitialized() ? audioPlayer->Buffered() : 0;
        const int desiredBuffered = audioPlayer && audioPlayer->IsInitialized() ? audioPlayer->GetDesiredBuffered() : 0;
        const uint32_t sampleCount = bufferedSamples < desiredBuffered ? SamplesHigh : SamplesLow;
        const uint32_t requestedFrames = R_UPDATE_RATE > 0 ? static_cast<uint32_t>(R_UPDATE_RATE) : 1;
        const uint32_t framesPerUpdate = std::min(requestedFrames, MaximumFramesPerUpdate);
        std::array<int16_t, SamplesHigh * AudioChannels * MaximumFramesPerUpdate> buffer{};

        for (uint32_t frame = 0; frame < framesPerUpdate; ++frame) {
            AudioMgr_CreateNextAudioBuffer(buffer.data() + frame * sampleCount * AudioChannels, sampleCount);
        }

        if (audioPlayer && audioPlayer->IsInitialized()) {
            audioPlayer->Play(reinterpret_cast<const uint8_t*>(buffer.data()),
                              sampleCount * sizeof(int16_t) * AudioChannels * framesPerUpdate);
        }

        lock.lock();
        mProcessing = false;
        mWorkComplete.notify_all();
    }

    mProcessing = false;
    mWorkComplete.notify_all();
}

} // namespace Game::Client
