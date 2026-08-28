#ifndef GAME_UTILS_H
#define GAME_UTILS_H

#include <runtime/runtime.h>

#ifdef __cplusplus

#include <algorithm>
#include <array>
#include <iterator>
#include <set>
#include <vector>

namespace GameUtils {
void RandInit(uint64_t seed, uint64_t* state = nullptr);
uint32_t next32(uint64_t* state = nullptr);
uint32_t Random(uint32_t min, uint32_t max, uint64_t* state = nullptr);
double RandomDouble(uint64_t* state = nullptr);

// Get a random element from a vector or array
template <typename T> T RandomElement(std::vector<T>& vector, bool erase, uint64_t* state = nullptr) {
    const auto idx = Random(0, static_cast<uint32_t>(vector.size()), state);
    const T selected = vector[idx];
    if (erase) {
        vector.erase(vector.begin() + idx);
    }
    return selected;
}
template <typename Container> auto& RandomElement(Container& container, uint64_t* state = nullptr) {
    return container[Random(0, static_cast<uint32_t>(std::size(container)), state)];
}
template <typename Container> const auto& RandomElement(const Container& container, uint64_t* state = nullptr) {
    return container[Random(0, static_cast<uint32_t>(std::size(container)), state)];
}

template <typename T> const T RandomElementFromSet(const std::set<T>& set, uint64_t* state = nullptr) {
    if (set.size() == 1) {
        return *set.begin();
    }
    uint32_t rand = Random(0, static_cast<uint32_t>(set.size()), state);
    auto it = set.begin();
    for (uint32_t i = 0; i < rand; i++) {
        it++;
    }
    auto test = *it;
    return *it;
}

template <typename T> void Shuffle(std::vector<T>& vector, uint64_t* state = nullptr) {
    for (size_t i = 0; i + 1 < vector.size(); i++) {
        std::swap(vector[i], vector[Random(static_cast<uint32_t>(i), static_cast<uint32_t>(vector.size()), state)]);
    }
}
template <typename T, size_t size> void Shuffle(std::array<T, size>& arr, uint64_t* state = nullptr) {
    for (size_t i = 0; i + 1 < arr.size(); i++) {
        std::swap(arr[i], arr[Random(static_cast<uint32_t>(i), static_cast<uint32_t>(arr.size()), state)]);
    }
}
} // namespace GameUtils
#endif

#endif // GAME_UTILS_H
