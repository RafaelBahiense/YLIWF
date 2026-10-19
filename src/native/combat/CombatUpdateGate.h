#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace mod::combat_update {

    class Gate {
    public:
        void Acquire(std::uint32_t actor) {
            std::lock_guard lock(mutex);
            actors.try_emplace(actor, 0);
        }

        void Release(std::uint32_t actor) {
            std::lock_guard lock(mutex);
            actors.erase(actor);
        }

        bool Suppress(std::uint32_t actor, bool scene, bool foreignInterrupt) {
            std::lock_guard lock(mutex);
            const auto found = actors.find(actor);
            if (found == actors.end() || scene || foreignInterrupt)
                return false;
            ++found->second;
            return true;
        }

        std::uint64_t TakeSuppressed(std::uint32_t actor) {
            std::lock_guard lock(mutex);
            const auto found = actors.find(actor);
            return found == actors.end() ? 0 : std::exchange(found->second, 0);
        }

        void Clear() {
            std::lock_guard lock(mutex);
            actors.clear();
        }

    private:
        std::mutex mutex;
        std::unordered_map<std::uint32_t, std::uint64_t> actors;
    };
}
