#pragma once

#include "MenuTabs.h"
#include <array>
#include <algorithm>
#include <string_view>

namespace mod::menu_tabs {
    inline bool Valid(const yliwf::menu::Tab& tab) {
        const auto terminated = [](const auto& text) {
            return text[0] && std::ranges::find(text, '\0') != std::end(text);
        };
        return tab.size >= sizeof(tab) && tab.version == yliwf::menu::Version && tab.render && terminated(tab.id) &&
               terminated(tab.title);
    }

    class Registry {
    public:
        struct Result {
            std::uint32_t handle = 0;
            bool inserted = false;
        };

        Result Add(const yliwf::menu::Tab& tab) {
            if (!Valid(tab))
                return {};
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto& existing = entries[i];
                if (std::string_view(existing.id) != tab.id)
                    continue;
                if (existing.render == tab.render && existing.context == tab.context &&
                    std::string_view(existing.title) == tab.title)
                    return {i + 1, false};
                return {};
            }
            if (count == entries.size())
                return {};
            entries[count++] = tab;
            return {count, true};
        }

        const yliwf::menu::Tab* At(std::uint32_t index) const {
            return index < count ? &entries[index] : nullptr;
        }

    private:
        std::array<yliwf::menu::Tab, yliwf::menu::MaxTabs> entries{};
        std::uint32_t count = 0;
    };
}
