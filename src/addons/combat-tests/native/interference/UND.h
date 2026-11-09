#pragma once
#include "Control.h"
#include "CombatTestHost.h"

namespace mod::combat_tests::interference::und {
    struct Snapshot {
        Support support = Support::Missing;
        std::uint32_t armed = 0, checks = 0, exclusions = 0, attempts = 0, denied = 0;
        std::uint32_t budget = 0, budgetAllowed = 0, reserveDenied = 0, cooldownDenied = 0, snapshotDenied = 0;
        float estimatedCost = 0, reserve = 0, stamina = 0;
        std::uint32_t recoveryDenied = 0;
    };

    void Install();
    bool Begin(RE::Actor* actor, bool enabled, bool budget = false, float reserveFraction = 0.2f, float cooldown = 2);
    void Sample(RE::Actor* actor, bool recovering = false, bool emergency = false);
    void End();
    void Reset();
    Snapshot Read();
    const yliwf::testing::EvadeBackend* DirectedBackend();

    constexpr const char* Status(Support value) {
        switch (value) {
            case Support::Missing:
                return "Ultimate NPC Dodging is not loaded.";
            case Support::Ready:
                return "UND comparison ready (verified 2.4.3).";
            case Support::Unsupported:
                return "UND DLL or runtime does not match the verified profile.";
            case Support::Shadowed:
                return "UND comparison call sites were changed by another hook.";
        }
        return "UND comparison unavailable.";
    }
}
