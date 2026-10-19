#pragma once

#include <optional>

namespace mod::combat_prevention_rules {
    struct Change {
        bool claim = false;
        std::optional<bool> prevent;
        bool release = false;
    };

    constexpr Change Decide(bool enable, bool owned, bool prevented) {
        if (!enable)
            return owned ? Change{false, false, true} : Change{};
        if (owned)
            return {false, true, false};
        // An existing restriction belongs to the engine or another controller.
        // Do not claim it, so releasing our order cannot clear it later.
        return prevented ? Change{} : Change{true, true, false};
    }
}
