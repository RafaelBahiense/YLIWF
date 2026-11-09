#pragma once
#include <utility>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace mod::combat_tests::testing {
    enum class State { Ready, Pending, Failed };

    struct Result {
        State state = State::Ready;
        std::string reason;

        static Result Pending(std::string reason = {}) {
            return {State::Pending, std::move(reason)};
        }

        static Result Failed(std::string reason) {
            return {State::Failed, std::move(reason)};
        }
    };
    enum class Scope { Suite, Case, Phase };

    // Callbacks poll cleanup on the game thread. Destruction never invokes engine
    // work. A pending/failed resource stays registered until verified or reset by
    // the host's explicit load/revert recovery path.
    class Resources {
    public:
        using Cleanup = std::function<Result()>;

        bool Own(Scope scope, std::string key, std::uint32_t id, Cleanup cleanup) {
            if (!id || !cleanup || key.empty() || closing_ || entries_.size() >= 32)
                return false;
            for (const auto& entry : entries_)
                if (entry.scope == scope && entry.key == key)
                    return entry.id == id;
            entries_.push_back({scope, std::move(key), id, std::move(cleanup), false});
            return true;
        }

        Result Close(Scope scope) {
            closing_ = true;
            for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
                if (it->closed || static_cast<int>(it->scope) < static_cast<int>(scope))
                    continue;
                const auto result = it->cleanup();
                if (result.state != State::Ready)
                    return result;
                it->closed = true;
            }
            std::erase_if(entries_, [scope](const Entry& entry) {
                return entry.closed && static_cast<int>(entry.scope) >= static_cast<int>(scope);
            });
            closing_ = false;
            return {};
        }

        std::uint32_t Find(Scope scope, const std::string& key) const {
            for (const auto& entry : entries_)
                if (entry.scope == scope && entry.key == key && !entry.closed)
                    return entry.id;
            return 0;
        }

        bool Empty() const {
            return entries_.empty();
        }

        void ResetForLoad() {
            entries_.clear();
            closing_ = false;
        }

    private:
        struct Entry {
            Scope scope;
            std::string key;
            std::uint32_t id;
            Cleanup cleanup;
            bool closed;
        };

        std::vector<Entry> entries_;
        bool closing_ = false;
    };
}
