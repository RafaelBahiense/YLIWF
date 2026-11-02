#pragma once

#include <cstdint>

namespace RE {
    class Actor;
}

// Install/Begin require the main thread. End/Reset/Read are engine-free and thread-safe.
// Mod-specific ABI and interception belong behind this interface.
namespace mod::combat_tests::interference {
    enum class Support : std::uint32_t { Missing, Ready, Unsupported, Shadowed };

    struct Snapshot {
        Support support = Support::Missing;
        std::uint32_t bypass = 0, armed = 0, blockStops = 0, bashStops = 0, suppressed = 0;
    };

    void Install();
    bool Begin(RE::Actor* actor, bool bypass);
    void End();
    void Reset();
    Snapshot Read();

    constexpr const char* Status(Support value) {
        switch (value) {
            case Support::Missing:
                return "Fix NPC PermaBlock is not loaded.";
            case Support::Ready:
                return "PermaBlock interruption observer ready.";
            case Support::Unsupported:
                return "Observer requires Skyrim 1.6.1170 and a supported module image.";
            case Support::Shadowed:
                return "Another graph hook or actor implementation prevents verified interception.";
        }
        return "Interruption observer unavailable.";
    }
}
