#include "ServerIntentAdmission.h"
#include "../SequenceNumber.h"

namespace Game::Simulation {

ServerIntentResult ServerIntentAdmission::Admit(
    int32_t playerId, uint32_t authoritativeLifeEpoch, uint32_t intentLifeEpoch,
    ServerIntentKind kind, uint32_t sequence) {
    if (playerId < 0 || authoritativeLifeEpoch == 0 ||
        intentLifeEpoch != authoritativeLifeEpoch) {
        return ServerIntentResult::Invalid;
    }

    const auto activeLife = mLifeEpochs.find(playerId);
    if (activeLife == mLifeEpochs.end() || activeLife->second != authoritativeLifeEpoch) {
        for (auto latest = mLatestSequences.begin(); latest != mLatestSequences.end();) {
            if (latest->first.playerId == playerId) {
                latest = mLatestSequences.erase(latest);
            } else {
                ++latest;
            }
        }
        for (auto accepted = mLastAcceptedTicks.begin(); accepted != mLastAcceptedTicks.end();) {
            if (accepted->first.playerId == playerId) {
                accepted = mLastAcceptedTicks.erase(accepted);
            } else {
                ++accepted;
            }
        }
        mLifeEpochs[playerId] = authoritativeLifeEpoch;
    }

    const IntentKey key{ playerId, kind };
    const auto latest = mLatestSequences.find(key);
    if (latest != mLatestSequences.end()) {
        if (sequence == latest->second) return ServerIntentResult::Duplicate;
        if (!Sequence::IsNewer(sequence, latest->second)) return ServerIntentResult::Stale;
    }
    mLatestSequences[key] = sequence;
    return ServerIntentResult::Fresh;
}

bool ServerIntentAdmission::CooldownReady(int32_t playerId, ServerIntentKind kind,
                                          uint64_t serverTick) const {
    if (playerId < 0) return false;
    const uint64_t cooldown = CooldownTicks(kind);
    if (cooldown == 0) return true;
    const auto previous = mLastAcceptedTicks.find({ playerId, kind });
    return previous == mLastAcceptedTicks.end() ||
           serverTick - previous->second >= cooldown;
}

void ServerIntentAdmission::RecordAccepted(int32_t playerId, ServerIntentKind kind,
                                           uint64_t serverTick) {
    if (playerId >= 0 && CooldownTicks(kind) != 0) {
        mLastAcceptedTicks[{ playerId, kind }] = serverTick;
    }
}

void ServerIntentAdmission::RemovePlayer(int32_t playerId) {
    for (auto sequence = mLatestSequences.begin(); sequence != mLatestSequences.end();) {
        if (sequence->first.playerId == playerId) {
            sequence = mLatestSequences.erase(sequence);
        } else {
            ++sequence;
        }
    }
    mLifeEpochs.erase(playerId);
    for (auto accepted = mLastAcceptedTicks.begin(); accepted != mLastAcceptedTicks.end();) {
        if (accepted->first.playerId == playerId) {
            accepted = mLastAcceptedTicks.erase(accepted);
        } else {
            ++accepted;
        }
    }
}

void ServerIntentAdmission::Reset() {
    mLatestSequences.clear();
    mLifeEpochs.clear();
    mLastAcceptedTicks.clear();
}

} // namespace Game::Simulation
