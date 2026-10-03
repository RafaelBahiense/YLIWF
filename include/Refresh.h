#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace mod::refresh {
    // Call under the refresh mutex. Events only dirty the cache; the visible page
    // drives Queue. Tickets also reject work from a closed page or an old save.
    class State {
    public:
        using Clock = std::chrono::steady_clock;
        struct Request {
            std::uint64_t revision;
            std::uint32_t inspectID;
        };

        void SetVisible(bool value) {
            if (visible == value) return;
            visible = value;
            if (visible) MarkDirty();
            else { queued = 0; inspectID = 0; }
        }
        void MarkDirty() { ++revision; }
        void Inspect(std::uint32_t id) { inspectID = id; MarkDirty(); }
        void Reset() {
            visible = false;
            queued = 0;
            inspectID = 0;
            lastQueued = {};
            MarkDirty();
        }
        std::uint64_t Queue(Clock::time_point now) {
            if (!visible || queued || (revision == captured && now - lastQueued < std::chrono::seconds(1))) return 0;
            lastQueued = now;
            return queued = ++sequence;
        }
        std::optional<Request> Start(std::uint64_t ticket) {
            if (!visible || !ticket || ticket != queued) return std::nullopt;
            const Request request{revision, inspectID};
            inspectID = 0;
            return request;
        }
        void Finish(std::uint64_t ticket, std::uint64_t capturedRevision) {
            if (ticket != queued) return;
            captured = capturedRevision;
            queued = 0;
        }

    private:
        bool visible = false;
        std::uint64_t revision = 1, captured = 0, sequence = 0, queued = 0;
        std::uint32_t inspectID = 0;
        Clock::time_point lastQueued{};
    };
}
