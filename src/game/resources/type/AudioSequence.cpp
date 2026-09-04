#include "AudioSequence.h"

namespace Game::Resources {

Sequence* AudioSequence::GetPointer() {
    return &sequence;
}

size_t AudioSequence::GetPointerSize() {
    return sizeof(Sequence);
}

AudioSequence::~AudioSequence() {
    delete[] sequence.seqData;
    sequence.seqData = nullptr;
}

} // namespace Game::Resources