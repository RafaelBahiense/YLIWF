#pragma once
#include <cstdint>

namespace mod::combat_tests::scene_rules {
    class Scope {
    public:
        bool Begin(std::uint32_t actor, std::uint32_t token, std::uint32_t setup, std::uint32_t combat) {
            if (actor_ || !actor || !token || !setup || !combat || setup == combat)
                return false;
            actor_ = actor;
            token_ = token;
            setup_ = setup;
            combat_ = combat;
            return true;
        }

        bool StartCombat(std::uint32_t actor, std::uint32_t token) {
            if (!Matches(actor, token))
                return false;
            fighting_ = true;
            return true;
        }

        bool End(std::uint32_t actor, std::uint32_t token) {
            if (!Matches(actor, token))
                return false;
            *this = {};
            return true;
        }

        bool Matches(std::uint32_t actor, std::uint32_t token) const {
            return actor && actor == actor_ && token && token == token_;
        }

        bool Includes(std::uint32_t actor) const {
            return actor && actor == actor_;
        }

        std::uint32_t Actor() const {
            return actor_;
        }

        std::uint32_t Preferred(std::uint32_t rolePackage) const {
            return !actor_ ? 0 : !fighting_ ? setup_ : rolePackage ? rolePackage : combat_;
        }

    private:
        std::uint32_t actor_ = 0, token_ = 0, setup_ = 0, combat_ = 0;
        bool fighting_ = false;
    };
}
