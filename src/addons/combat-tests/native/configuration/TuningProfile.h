#pragma once
#include "DefenderTuning.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace mod::combat_tests::tuning {
    inline std::string Serialize(const yliwf::testing::DefenderTuning& profile) {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::setprecision(std::numeric_limits<float>::max_digits10);
        text << "schema=1\nname=" << profile.name << '\n';
        text << "challenge_enabled=" << profile.challengeEnabled << '\n';
        for (const auto& field : yliwf::testing::TuningFields)
            text << field.key << '=' << profile.*field.member << '\n';
        return text.str();
    }

    inline std::string_view Trim(std::string_view text) {
        const auto first = text.find_first_not_of(" \t\r");
        if (first == std::string_view::npos)
            return {};
        return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
    }

    inline std::optional<yliwf::testing::DefenderTuning> Parse(std::string_view text) {
        if (text.empty() || text.size() > 8192)
            return std::nullopt;
        if (text.starts_with("\xEF\xBB\xBF"))
            text.remove_prefix(3);
        yliwf::testing::DefenderTuning result;
        bool schema = false, name = false;
        bool challenge = false;
        std::array<bool, yliwf::testing::TuningFields.size()> fields{};
        while (!text.empty()) {
            const auto newline = text.find('\n');
            const auto line = Trim(text.substr(0, newline));
            text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
            if (line.empty() || line.starts_with('#'))
                continue;
            const auto equal = line.find('=');
            if (equal == std::string_view::npos)
                return std::nullopt;
            const auto key = Trim(line.substr(0, equal)), value = Trim(line.substr(equal + 1));
            if (key == "schema") {
                if (schema || value != "1")
                    return std::nullopt;
                schema = true;
            } else if (key == "challenge_enabled") {
                if (challenge || (value != "0" && value != "1"))
                    return std::nullopt;
                result.challengeEnabled = value == "1" ? 1u : 0u;
                challenge = true;
            } else if (key == "name") {
                if (name || value.empty() || value.size() >= sizeof(result.name))
                    return std::nullopt;
                std::memset(result.name, 0, sizeof(result.name));
                std::memcpy(result.name, value.data(), value.size());
                name = true;
            } else {
                bool recognized = false;
                for (std::size_t i = 0; i < fields.size(); ++i) {
                    const auto& field = yliwf::testing::TuningFields[i];
                    if (key != field.key)
                        continue;
                    float number = 0;
                    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
                    if (fields[i] || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                        return std::nullopt;
                    result.*field.member = number;
                    fields[i] = recognized = true;
                    break;
                }
                if (!recognized)
                    return std::nullopt;
            }
        }
        if (!schema || !name || !std::ranges::all_of(fields, [](bool present) { return present; }) ||
            yliwf::testing::TuningError(result))
            return std::nullopt;
        return result;
    }
}
