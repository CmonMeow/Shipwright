#include "resources/importer/AudioSampleFactory.h"
#include "resources/type/AudioSample.h"
#include <runtime/log/Log.hpp>

namespace Game::Resources {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryAudioSampleV2::ReadResource(std::shared_ptr<Engine::File> file,
                                                 std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    if (file->Buffer->size() < 8) {
        WriteLog("Truncated audio sample header in {}", initData->Path);
        return nullptr;
    }

    auto audioSample = std::make_shared<AudioSample>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    audioSample->sample.codec = reader->ReadUByte();
    audioSample->sample.medium = reader->ReadUByte();
    audioSample->sample.unk_bit26 = reader->ReadUByte();
    audioSample->sample.isRelocated = reader->ReadUByte();
    audioSample->sample.size = reader->ReadUInt32();

    size_t consumed = 8;
    if (audioSample->sample.size > file->Buffer->size() - consumed) {
        WriteLog("Invalid audio sample size {} for {}-byte resource {}", audioSample->sample.size,
                 file->Buffer->size(), initData->Path);
        return nullptr;
    }

    audioSample->sample.sampleAddr = new uint8_t[audioSample->sample.size];
    for (uint32_t i = 0; i < audioSample->sample.size; i++) {
        audioSample->sample.sampleAddr[i] = reader->ReadUByte();
    }
    consumed += audioSample->sample.size;

    if (file->Buffer->size() - consumed < 16) {
        WriteLog("Truncated audio loop metadata in {}", initData->Path);
        return nullptr;
    }

    audioSample->loop.start = reader->ReadUInt32();
    audioSample->loop.end = reader->ReadUInt32();
    audioSample->loop.count = reader->ReadUInt32();

    // This always seems to be 16. Can it be removed in V3?
    uint32_t loopStateCount = reader->ReadUInt32();
    consumed += 16;
    if (loopStateCount > 16 || file->Buffer->size() - consumed < loopStateCount * sizeof(int16_t) + 12) {
        WriteLog("Invalid audio loop state count {} in {}", loopStateCount, initData->Path);
        return nullptr;
    }
    for (int i = 0; i < 16; i++) {
        audioSample->loop.state[i] = 0;
    }
    for (uint32_t i = 0; i < loopStateCount; i++) {
        audioSample->loop.state[i] = reader->ReadInt16();
    }
    consumed += loopStateCount * sizeof(int16_t);
    audioSample->sample.loop = &audioSample->loop;

    audioSample->book.order = reader->ReadInt32();
    audioSample->book.npredictors = reader->ReadInt32();
    uint32_t bookDataCount = reader->ReadUInt32();
    consumed += 12;
    if (bookDataCount > (file->Buffer->size() - consumed) / sizeof(int16_t)) {
        WriteLog("Invalid audio predictor book size {} in {}", bookDataCount, initData->Path);
        return nullptr;
    }

    audioSample->book.book = new int16_t[bookDataCount];

    for (uint32_t i = 0; i < bookDataCount; i++) {
        audioSample->book.book[i] = reader->ReadInt16();
    }
    audioSample->sample.book = &audioSample->book;

    return audioSample;
}

} // namespace Game::Resources
