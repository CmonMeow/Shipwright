#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace Engine {
class Audio;
}

namespace Game::Client {

class NativeAudioWorker final {
  public:
    explicit NativeAudioWorker(Engine::Audio& audio);
    ~NativeAudioWorker();

    NativeAudioWorker(const NativeAudioWorker&) = delete;
    NativeAudioWorker& operator=(const NativeAudioWorker&) = delete;

    void Start();
    void Stop();
    void BeginFrame();
    void WaitForFrame();

  private:
    void Run();

    Engine::Audio& mAudio;
    std::thread mThread;
    std::condition_variable mWorkAvailable;
    std::condition_variable mWorkComplete;
    std::mutex mMutex;
    bool mRunning = false;
    bool mProcessing = false;
};

} // namespace Game::Client
