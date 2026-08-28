#include <sysdef.h>

#include "Network/VoiceChat.h"

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

    NetworkVoicePacket opusPacket;
    if (!opus.encode(input.data(), opusPacket) || opusPacket.codec != VOICE_CODEC_OPUS || opusPacket.data.empty()) {
        Error("Voice codec self-test: Opus encode failed");
        return 2;
    }
    std::vector<__int16> opusOutput;
    if (!opus.decode(opusPacket, opusOutput) || opusOutput.size() != VOICE_SAMPLES_PER_PACKET) {
        Error("Voice codec self-test: Opus decode failed");
        return 3;
    }

    NetworkVoicePacket adpcmPacket;
    EncodeAdpcmVoicePacket(input.data(), adpcmPacket);
    std::vector<__int16> adpcmOutput;
    DecodeAdpcmVoicePacket(adpcmPacket, adpcmOutput);
    if (adpcmPacket.codec != VOICE_CODEC_ADPCM || adpcmOutput.size() != VOICE_SAMPLES_PER_PACKET) {
        Error("Voice codec self-test: ADPCM fallback failed");
        return 4;
    }

    Error("Voice codec self-test passed: Opus codec and ADPCM fallback");
    return 0;
}
