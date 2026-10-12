#pragma once

#include <cstdint>
#include <string_view>

namespace mod::formation_geometry {
    enum class Style : std::int32_t { Off, Column, Line, Wedge };

    constexpr std::string_view Name(Style style) {
        switch (style) {
            case Style::Off:
                return "Off";
            case Style::Column:
                return "Column";
            case Style::Line:
                return "Line";
            case Style::Wedge:
                return "Wedge";
        }
        return "Off";
    }
}
