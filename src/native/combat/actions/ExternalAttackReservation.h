#pragma once
#include <cstdint>
#include <mutex>

namespace mod::combat_actions {
    // Engine-free mailbox. Never holds this lock while calling an engine or add-on function.
    class ExternalAttackReservation {
    public:
        void Begin(std::uint32_t actor, std::uint32_t token) {
            std::scoped_lock lock(mutex);
            owner = actor;
            generation = token;
            sequence = current = 0;
            finishing = false;
            open = actor && token;
        }

        bool Set(std::uint32_t actor, std::uint32_t token, std::uint32_t request, bool active) {
            std::scoped_lock lock(mutex);
            if (!open || actor != owner || token != generation || !request)
                return false;
            if (active) {
                if (request <= sequence)
                    return false;
                current = sequence = request;
                finishing = false;
            } else {
                if (current != request)
                    return false;
                current = 0;
                finishing = false;
            }
            return true;
        }

        bool Active() const {
            std::scoped_lock lock(mutex);
            return current != 0;
        }

        bool Finish(std::uint32_t actor, std::uint32_t token, std::uint32_t request) {
            std::scoped_lock lock(mutex);
            if (!open || actor != owner || token != generation || !request || current != request)
                return false;
            finishing = true;
            return true;
        }

        bool BlocksNewAttacks() const {
            std::scoped_lock lock(mutex);
            return open && current && finishing;
        }

        void Close() {
            std::scoped_lock lock(mutex);
            open = false;
        }

        void Clear() {
            Begin(0, 0);
        }

    private:
        mutable std::mutex mutex;
        std::uint32_t owner = 0, generation = 0, sequence = 0, current = 0;
        bool open = false, finishing = false;
    };
}
