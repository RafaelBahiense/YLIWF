#pragma once
#include "Definition.h"
#include <cstdint>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cmath>

namespace mod::combat_tests::testing {
    inline float ParticipantHealth(const ActorSpec& spec, float subjectMaximum) {
        if (!spec.subjectHealthFraction)
            return spec.health;
        return std::isfinite(subjectMaximum) && subjectMaximum > 0
                   ? std::clamp(subjectMaximum * spec.subjectHealthFraction, spec.minimumHealth, spec.maximumHealth)
                   : 0;
    }

    struct Participant {
        ActorSpec spec;
        std::uint32_t id = 0, base = 0;
        std::uint32_t incoming = 0, outgoing = 0;
        float healthLost = 0, previousHealth = 0, damageModifier = 0;
        std::uint32_t attackStarts = 0;
        bool wasAttacking = false;
    };

    class Participants {
    public:
        bool Track(const ActorSpec& spec, std::uint32_t id, std::uint32_t base) {
            if (spec.key.empty() || spec.key == "subject" || !id || !base)
                return false;
            for (const auto& entry : entries_)
                if (entry.spec.key == spec.key || entry.id == id)
                    return entry.spec == spec && entry.id == id && entry.base == base;
            if (entries_.size() >= 16)
                return false;
            entries_.push_back({spec, id, base});
            return true;
        }

        Participant* Find(std::uint32_t id) {
            for (auto& entry : entries_)
                if (entry.id == id)
                    return &entry;
            return nullptr;
        }

        const Participant* Find(std::uint32_t id) const {
            for (const auto& entry : entries_)
                if (entry.id == id)
                    return &entry;
            return nullptr;
        }

        const Participant* Find(std::string_view key) const {
            for (const auto& entry : entries_)
                if (entry.spec.key == key)
                    return &entry;
            return nullptr;
        }

        std::uint32_t First(ActorRole role) const {
            for (const auto& entry : entries_)
                if (entry.spec.role == role)
                    return entry.id;
            return 0;
        }

        std::uint32_t Target(const Participant& entry, std::uint32_t subject) const {
            if (entry.spec.target == "subject")
                return subject;
            const auto* target = Find(entry.spec.target);
            return target ? target->id : 0;
        }

        bool Opposed(std::uint32_t first, std::uint32_t second, std::uint32_t subject) const {
            const auto* a = Find(first);
            const auto* b = Find(second);
            if (first == subject)
                return b && b->spec.role == ActorRole::Opponent;
            if (second == subject)
                return a && a->spec.role == ActorRole::Opponent;
            return a && b && a->spec.role != b->spec.role;
        }

        std::vector<Participant>& Entries() {
            return entries_;
        }

        const std::vector<Participant>& Entries() const {
            return entries_;
        }

        void Clear() {
            entries_.clear();
        }

    private:
        std::vector<Participant> entries_;
    };
}
