#include "LocalStructureActionStream.h"

namespace Game::Client {

LocalStructureActionStream::LocalStructureActionStream(uint32_t nextSequence)
    : mNextSequence(nextSequence == 0 ? 1 : nextSequence) {
}

std::optional<LocalStructureAction> LocalStructureActionStream::Issue(
    const LocalStructureActionRequest& request) {
    if (!IsSane(request)) return std::nullopt;
    return LocalStructureAction{ TakeSequence(), request };
}

void LocalStructureActionStream::BeginLife() {
    mNextSequence = 1;
}

void LocalStructureActionStream::Reset() {
    BeginLife();
}

bool LocalStructureActionStream::IsSane(
    const LocalStructureActionRequest& request) {
    return request.structureKey >= 0 &&
           (request.kind == LocalStructureActionKind::Build ||
            request.kind == LocalStructureActionKind::Repair);
}

uint32_t LocalStructureActionStream::TakeSequence() {
    const uint32_t sequence = mNextSequence++;
    if (mNextSequence == 0) mNextSequence = 1;
    return sequence;
}

} // namespace Game::Client
