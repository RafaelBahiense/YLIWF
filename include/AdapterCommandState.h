#pragma once

#include "FollowerAdapter.h"
#include <unordered_map>
#include <mutex>

namespace yliwf::sdk::papyrus {
    // Process-local request bookkeeping for owner-method callbacks. Requests
    // are never replayed and old sessions cannot complete a new actor command.
    class CommandState {
    public:
        std::uint64_t Generation() const {
            std::scoped_lock lock(mutex);
            return generation;
        }

        bool Pending(ActorID actor) const {
            std::scoped_lock lock(mutex);
            return pending.contains(actor);
        }

        bool Begin(ActorID actor, RequestID request) {
            std::scoped_lock lock(mutex);
            return actor && request && pending.try_emplace(actor, request).second;
        }

        bool Finish(ActorID actor, RequestID request, std::uint64_t epoch) {
            std::scoped_lock lock(mutex);
            const auto found = pending.find(actor);
            if (epoch != generation || found == pending.end() || found->second != request)
                return false;
            pending.erase(found);
            return true;
        }

        void Invalidate() {
            std::scoped_lock lock(mutex);
            ++generation;
            pending.clear();
        }

    private:
        mutable std::mutex mutex;
        std::uint64_t generation = 0;
        std::unordered_map<ActorID, RequestID> pending;
    };
}
