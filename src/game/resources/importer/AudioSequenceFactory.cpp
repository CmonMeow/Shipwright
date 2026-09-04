#include "resources/importer/AudioSequenceFactory.h"
#include "resources/type/AudioSequence.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryAudioSequenceV2::ReadResource(std::shared_ptr<Engine::File> file,
                                                   std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto audioSequence = std::make_shared<AudioSequence>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    audioSequence->sequence.seqDataSize = reader->ReadUInt32();
    audioSequence->sequence.seqData = new char[audioSequence->sequence.seqDataSize];
    for (int32_t i = 0; i < audioSequence->sequence.seqDataSize; i++) {
        audioSequence->sequence.seqData[i] = reader->ReadChar();
    }

    audioSequence->sequence.seqNumber = reader->ReadUByte();
    audioSequence->sequence.medium = reader->ReadUByte();
    audioSequence->sequence.cachePolicy = reader->ReadUByte();

    audioSequence->sequence.numFonts = reader->ReadUInt32();
    for (int32_t i = 0; i < 16; i++) {
        audioSequence->sequence.fonts[i] = 0;
    }
    for (int32_t i = 0; i < audioSequence->sequence.numFonts; i++) {
        audioSequence->sequence.fonts[i] = reader->ReadUByte();
    }

    return audioSequence;
}

} // namespace Game::Resources
