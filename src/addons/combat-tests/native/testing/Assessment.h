#pragma once
#include <cstdint>
#include <string>

namespace mod::combat_tests::testing {
    enum class Verdict : std::uint32_t { NotAssessed, Passed, Failed, Inconclusive };

    constexpr const char* Name(Verdict verdict) {
        switch (verdict) {
            case Verdict::Passed:
                return "Passed";
            case Verdict::Failed:
                return "Failed";
            case Verdict::Inconclusive:
                return "Inconclusive";
            default:
                return "Not assessed";
        }
    }

    struct Completion {
        Verdict verdict = Verdict::NotAssessed;
        std::string reason;
    };

    struct ExecutionEvidence {
        bool completed = false, restored = false;
    };

    constexpr bool Clean(const ExecutionEvidence& evidence) {
        return evidence.completed && evidence.restored;
    }

    inline bool Defeated(bool dead, bool bleedingOut, float health) {
        return dead || bleedingOut || health <= 0;
    }
}
