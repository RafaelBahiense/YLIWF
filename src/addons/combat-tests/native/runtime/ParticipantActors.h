#pragma once
#include "../testing/Participants.h"
#include <memory>
#include <atomic>

namespace RE {
    class Actor;
    class TESObjectREFR;
}

namespace mod::combat_tests {
    class ParticipantActors {
    public:
        testing::Participants participants;

        void SubjectTarget(std::string key) {
            subjectTarget_ = std::move(key);
        }

        testing::Result Spawn(testing::Context& context, const testing::ActorSpec& spec, RE::TESObjectREFR* anchor);
        testing::Result Prepare(std::uint32_t ticket);
        testing::Result Normalize(std::uint32_t ticket);
        testing::Result Start(std::uint32_t subject, std::uint32_t ticket);
        testing::Result Pending() const;
        bool Owns(RE::Actor* actor) const;
        bool Loaded(RE::TESObjectREFR* marker) const;
        std::string Validate(bool running);
        std::string ProbeProtection();
        void Position(RE::Actor* subject);
        void StopCombat();
        void Capture();
        void Reset();
        std::vector<std::uint32_t> IDs(testing::ActorRole role) const;

    private:
        enum class Operation { None, Prepare, Normalize, Start };

        static constexpr int PendingResult = -0x7FFFFFFF;

        struct Receipt {
            std::atomic_int result{PendingResult};
        };
        class Callback;
        testing::Result Poll(Operation operation, std::uint32_t subject, std::uint32_t ticket);
        std::shared_ptr<Receipt> receipt_;
        Operation operation_ = Operation::None;
        std::size_t cursor_ = 0;
        bool prepared_ = false, normalized_ = false, started_ = false;
        std::string subjectTarget_;
    };
}
