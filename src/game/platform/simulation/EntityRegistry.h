#pragma once

#include "EntityId.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Game::Simulation {

template <typename Entity>
class EntityRegistry final {
  public:
    EntityId Create(Entity entity = {}) {
        uint32_t index = 0;
        if (mFreeSlots.empty()) {
            index = static_cast<uint32_t>(mSlots.size());
            mSlots.emplace_back();
        } else {
            index = mFreeSlots.back();
            mFreeSlots.pop_back();
        }
        mSlots[index].entity.emplace(std::move(entity));
        ++mSize;
        return { index, mSlots[index].generation };
    }

    bool Destroy(EntityId id) {
        if (!Contains(id)) {
            return false;
        }
        Slot& slot = mSlots[id.index];
        slot.entity.reset();
        ++slot.generation;
        if (slot.generation == 0) {
            slot.generation = 1;
        }
        mFreeSlots.push_back(id.index);
        --mSize;
        return true;
    }

    Entity* Get(EntityId id) {
        return Contains(id) ? &*mSlots[id.index].entity : nullptr;
    }

    const Entity* Get(EntityId id) const {
        return Contains(id) ? &*mSlots[id.index].entity : nullptr;
    }

    template <typename Predicate>
    Entity* FindIf(Predicate&& predicate) {
        for (Slot& slot : mSlots) {
            if (slot.entity && predicate(*slot.entity)) {
                return &*slot.entity;
            }
        }
        return nullptr;
    }

    template <typename Predicate>
    const Entity* FindIf(Predicate&& predicate) const {
        for (const Slot& slot : mSlots) {
            if (slot.entity && predicate(*slot.entity)) {
                return &*slot.entity;
            }
        }
        return nullptr;
    }

    template <typename Function>
    void ForEach(Function&& function) {
        for (Slot& slot : mSlots) {
            if (slot.entity) {
                function(*slot.entity);
            }
        }
    }

    template <typename Function>
    void ForEach(Function&& function) const {
        for (const Slot& slot : mSlots) {
            if (slot.entity) {
                function(*slot.entity);
            }
        }
    }

    void Clear() {
        mSlots.clear();
        mFreeSlots.clear();
        mSize = 0;
    }

    size_t Size() const {
        return mSize;
    }

  private:
    struct Slot {
        uint32_t generation = 1;
        std::optional<Entity> entity;
    };

    bool Contains(EntityId id) const {
        return id.Valid() && id.index < mSlots.size() && mSlots[id.index].generation == id.generation &&
               mSlots[id.index].entity.has_value();
    }

    std::vector<Slot> mSlots;
    std::vector<uint32_t> mFreeSlots;
    size_t mSize = 0;
};

} // namespace Game::Simulation
