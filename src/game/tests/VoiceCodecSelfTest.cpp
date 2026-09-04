#include "multiplayer/Win32NetworkPlatform.h"

#include "multiplayer/VoiceChat.h"

#include <cmath>
#include <vector>

int main() {
    std::vector<__int16> input(VOICE_SAMPLES_PER_PACKET);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<__int16>(std::sin(static_cast<double>(i) * 0.11) * 12000.0);
    }

    cOpusCodec opus;
    if (!opus.available()) {
        Error("Voice codec self-test: Opus is unavailable");
        return 1;
    }

    std::vector<unsigned char> encoded;
    if (!opus.encode(input.data(), encoded) || encoded.empty()) {
        Error("Voice codec self-test: Opus encode failed");
        return 2;
    }
    NetworkVoicePacket opusPacket{};
    opusPacket.codec = VOICE_CODEC_OPUS;
    opusPacket.sampleRate = VOICE_SAMPLE_RATE;
    opusPacket.frameSamples = VOICE_SAMPLES_PER_PACKET;
    opusPacket.data = std::move(encoded);
    std::vector<__int16> opusOutput;
    if (!opus.decode(opusPacket, opusOutput) || opusOutput.size() != VOICE_SAMPLES_PER_PACKET) {
        Error("Voice codec self-test: Opus decode failed");
        return 3;
    }

    Error("Voice codec self-test passed: Opus encode/decode");
    return 0;
}
