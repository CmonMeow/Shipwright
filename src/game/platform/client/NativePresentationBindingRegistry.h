#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>

namespace Game::Client {

using NativePresentationBindingId = uint64_t;

// Keeps native rendering objects behind non-pointer identities. Network and
// command state may retain a binding ID, but only the native presentation
// boundary can resolve it. Forget must be called from the native object's
// destruction callback before its storage can be reused.
template <typename NativeType>
class NativePresentationBindingRegistry final {
  public:
    NativePresentationBindingId Observe(NativeType* native) {
        if (!native) return 0;
        if (const auto found = mByNative.find(native); found != mByNative.end()) {
            return found->second;
        }

        const NativePresentationBindingId first = mNextId;
        do {
            const NativePresentationBindingId candidate = mNextId++;
            if (mNextId == 0) mNextId = 1;
            if (candidate != 0 && !mById.contains(candidate)) {
                mByNative.emplace(native, candidate);
                mById.emplace(candidate, native);
                return candidate;
            }
        } while (mNextId != first);
        return 0;
    }

    std::optional<NativePresentationBindingId> Find(NativeType* native) const {
        const auto found = mByNative.find(native);
        if (found == mByNative.end()) return std::nullopt;
        return found->second;
    }

    NativeType* Resolve(NativePresentationBindingId id) const {
        const auto found = mById.find(id);
        return found == mById.end() ? nullptr : found->second;
    }

    bool Forget(NativeType* native) {
        const auto found = mByNative.find(native);
        if (found == mByNative.end()) return false;
        mById.erase(found->second);
        mByNative.erase(found);
        return true;
    }

    void Reset() {
        mByNative.clear();
        mById.clear();
        // Do not recycle IDs at a session boundary. A late native callback or
        // authority result can never resolve to a new object by coincidence.
    }

    size_t Size() const { return mById.size(); }

  private:
    std::map<NativeType*, NativePresentationBindingId> mByNative;
    std::map<NativePresentationBindingId, NativeType*> mById;
    NativePresentationBindingId mNextId = 1;
};

} // namespace Game::Client
