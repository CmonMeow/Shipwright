#include <runtime/log/Log.hpp>
#include "engine/audio/NullAudioPlayer.h"
namespace Engine {

NullAudioPlayer::~NullAudioPlayer() {
    WriteLog("destruct Null audio player");
}

bool NullAudioPlayer::DoInit() {
    return true;
}

void NullAudioPlayer::DoClose() {
    // Nothing to close for null player
}

int NullAudioPlayer::Buffered() {
    return 0;
}

void NullAudioPlayer::DoPlay(const uint8_t* buf, size_t len) {
}
} // namespace Engine
