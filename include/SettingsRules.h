#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include "Strings.h"

namespace mod::settings_rules {
    inline std::string_view Trim(std::string_view text) {
        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == text.npos)
            return {};
        return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
    }

    inline std::string_view WithoutComment(std::string_view text) {
        auto end = text.find_first_of(";#");
        end = std::min(end, text.find("//"));
        return Trim(text.substr(0, end));
    }

    inline std::optional<std::string_view> IniValue(std::string_view text, std::string_view section,
                                                    std::string_view key) {
        bool current = false;
        while (!text.empty()) {
            const auto newline = text.find('\n');
            const auto line = WithoutComment(text.substr(0, newline));
            text = newline == text.npos ? std::string_view{} : text.substr(newline + 1);
            if (line.starts_with('[') && line.ends_with(']'))
                current = strings::EqualsIgnoreCase(Trim(line.substr(1, line.size() - 2)), section);
            else if (current) {
                const auto equal = line.find('=');
                if (equal != line.npos && strings::EqualsIgnoreCase(Trim(line.substr(0, equal)), key))
                    return Trim(line.substr(equal + 1));
            }
        }
        return {};
    }

    inline std::optional<std::int32_t> ParseInteger(std::string_view text, std::int32_t minimum, std::int32_t maximum) {
        text = WithoutComment(text);
        if (text.empty())
            return {};
        std::int32_t value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || value < minimum || value > maximum)
            return {};
        return value;
    }

    inline std::optional<std::uint32_t> ParseFormID(std::string_view text) {
        if (text.starts_with("0x") || text.starts_with("0X"))
            text.remove_prefix(2);
        if (text.empty())
            return {};
        std::uint32_t value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
        if (error != std::errc{} || end != text.data() + text.size())
            return {};
        return value;
    }
}
