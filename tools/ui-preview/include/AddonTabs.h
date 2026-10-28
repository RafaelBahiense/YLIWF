#pragma once

#include <vector>

namespace preview {
    inline auto& TabFactories() {
        static std::vector<void (*)()> factories;
        return factories;
    }

    struct AddonTab {
        explicit AddonTab(void (*registerTab)()) {
            TabFactories().push_back(registerTab);
        }
    };

    inline void RegisterAddonTabs() {
        for (const auto registerTab : TabFactories())
            registerTab();
    }
}
