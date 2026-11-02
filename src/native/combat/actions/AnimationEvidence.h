#pragma once

#include <mutex>
#include <cstdint>

namespace mod::combat_actions {
    enum class AnimationEvent { Started, Ended, Interrupted, Conflicting };

    struct AnimationEvidence {
        bool graphStarted = false, nativeStarted = false, ended = false, interrupted = false, conflicting = false;
    };

    // Engine-free mailbox; graph callbacks never take the controller/lease mutex.
    class AnimationEvidenceMailbox {
    public:
        void Arm(std::uint32_t actor, std::uint32_t token, std::uint32_t request) {
            std::scoped_lock lock(mutex);
            owner = actor;
            generation = token;
            current = actor && token ? request : 0;
            evidence = {};
        }

        std::uint32_t Request(std::uint32_t actor, std::uint32_t token) const {
            std::scoped_lock lock(mutex);
            return actor == owner && token == generation ? current : 0;
        }

        bool Record(std::uint32_t actor, std::uint32_t token, std::uint32_t request, AnimationEvent event) {
            std::scoped_lock lock(mutex);
            if (!Matches(actor, token, request))
                return false;
            using Event = AnimationEvent;
            switch (event) {
                case Event::Started:
                    if (!evidence.ended)
                        evidence.graphStarted = true;
                    return true;
                case Event::Ended:
                    if (evidence.graphStarted || evidence.nativeStarted || evidence.conflicting)
                        evidence.ended = true;
                    return true;
                case Event::Interrupted:
                    if (!evidence.ended && (evidence.graphStarted || evidence.nativeStarted))
                        evidence.interrupted = true;
                    return true;
                case Event::Conflicting:
                    evidence.conflicting = evidence.interrupted = true;
                    return true;
            }
            return false;
        }

        void NativeStart(std::uint32_t actor, std::uint32_t token, std::uint32_t request) {
            std::scoped_lock lock(mutex);
            if (Matches(actor, token, request))
                evidence.nativeStarted = true;
        }

        void Teardown(std::uint32_t actor, std::uint32_t token, std::uint32_t request) {
            std::scoped_lock lock(mutex);
            if (Matches(actor, token, request) && !evidence.ended)
                evidence.interrupted = true;
        }

        AnimationEvidence Read(std::uint32_t actor, std::uint32_t token, std::uint32_t request) const {
            std::scoped_lock lock(mutex);
            return Matches(actor, token, request) ? evidence : AnimationEvidence{};
        }

        void Clear() {
            Arm(0, 0, 0);
        }

    private:
        bool Matches(std::uint32_t actor, std::uint32_t token, std::uint32_t request) const {
            return current && request == current && actor == owner && token == generation;
        }

        mutable std::mutex mutex;
        std::uint32_t owner = 0, generation = 0, current = 0;
        AnimationEvidence evidence;
    };
}
