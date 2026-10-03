#pragma once

#include "Strings.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// Engine-independent checks: shared by the menu/dump and the regression tests.
namespace mod::debug_rules {
    inline bool IsFollowerScript(std::string_view name) {
        return mod::strings::EqualsIgnoreCase(name, "DialogueFollowerScript");
    }

    inline bool CurrentGame(std::uint64_t expected, std::uint64_t current, bool ready) {
        return ready && expected == current;
    }
    inline bool CurrentCommand(std::int32_t expected, std::int32_t current, bool ready) {
        return ready && current != 0 && expected == current;
    }
    inline void AddIssue(std::string& issues, const char* issue) {
        if (!issues.empty()) issues += "; ";
        issues += issue;
    }

    inline bool CanRepair(bool structure, bool script, bool bindings, bool reconcile) {
        // Reconcile can restore saved bindings, but cannot create missing quest aliases.
        return structure && script && (bindings || reconcile);
    }

    inline bool IsPartyAction(std::int32_t action) { return action >= 2 && action <= 4; }

    inline bool CanCommand(bool structure, bool script, bool bindings, std::int32_t action, bool debugEnabled) {
        return action >= 0 && action <= 8 && (IsPartyAction(action) || action == 8 || debugEnabled) &&
            CanRepair(structure, script, bindings, action == 0);
    }

    inline std::string InstallationIssues(bool structure, bool script, bool bindings, std::int32_t configuredCap, std::int32_t capacity) {
        std::string issues;
        if (!structure) AddIssue(issues, "DialogueFollower runtime aliases or required globals failed validation; state repair is blocked");
        if (!script) AddIssue(issues, "DialogueFollowerScript is not initialized; check the quest scripts and installed PEX files");
        else if (!bindings) AddIssue(issues, "saved DialogueFollowerScript properties are missing or incorrect; Reconcile counts / dead slots can restore bindings when all required aliases exist");
        if (configuredCap > capacity) AddIssue(issues, "configured follower cap exceeds available alias capacity");
        return issues;
    }

    template <class Row>
    std::int32_t Diagnose(std::vector<Row>& rows) {
        std::unordered_set<std::uint32_t> seen;
        std::int32_t live = 0;
        for (auto& row : rows) {
            row.issues.clear();
            if (!row.formID) continue;
            if (!seen.insert(row.formID).second) AddIssue(row.issues, "duplicate alias");
            else if (!row.dead) ++live;
            if (row.dead) AddIssue(row.issues, "dead reference in slot");
            if (!row.teammate) AddIssue(row.issues, "missing teammate flag");
            if (!row.currentFaction) AddIssue(row.issues, "missing CurrentFollowerFaction");
            if (row.waiting != 0 && row.waiting != 1) AddIssue(row.issues, "invalid waiting value");
        }
        return live;
    }

    inline std::string GlobalIssues(std::int32_t live, std::int32_t cap, float count, float gate) {
        std::string issues;
        if (count != static_cast<float>(live)) AddIssue(issues, "party count differs from unique live aliases");
        if (gate != (live < cap ? 1.0f : 0.0f)) AddIssue(issues, "recruitment gate differs from alias count/cap");
        if (live > cap) AddIssue(issues, "party exceeds current cap");
        return issues;
    }
}
