#pragma once

#include "diagnostics/Debug.h"
#include "settings/Settings.h"

namespace preview {
    // Shared only between matching local preview builds using the dynamic CRT.
    // No callbacks, ImGui objects or module-owned allocations survive a reload.
    struct Session {
        bool valid = false;
        mod::debug::Snapshot party;
        std::int32_t maximum = 7, gate = 0, speech = 10, distance = 1;
        bool essential = true, friendlyFire = true, crossfire = true, sandbox = true, homes = true;
        mod::formation_geometry::Style formation = mod::formation_geometry::Style::Off;
        mod::hotkey_rules::Bindings hotkeys{};
        std::array<mod::settings::PerkSpec, mod::settings::kMaxPerkSpecs> perks{};
        std::size_t perkCount = 0;
        std::string perkText, saveStatus;
        bool debug = false, logging = false, standDown = false;
        int scenario = 0;
        float scale = 1;
        std::string reloadStatus = "Watching UI sources";
    };
}
