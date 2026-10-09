// Assertions are the test runner; keep them active in every configuration.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "DebugRules.h"
#include "Debug.h"
#include "Refresh.h"
#include "PartyRules.h"
#include "SettingsRules.h"
#include "SettingsDefaults.h"
#include "SettingsFile.h"
#include "FollowerView.h"
#include "FollowDistanceRules.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <fstream>

struct Row {
    std::uint32_t formID = 0;
    bool dead = false, teammate = true, currentFaction = true;
    float waiting = 0;
    std::string issues{};
};

namespace {
    int traceWrites = 0;
    std::string traceEvent, traceDetail;
}

// Capture the public helpers' output without starting Skyrim or writing a log.
namespace mod::debug {
    void Trace(std::string_view event, RE::Actor*, std::string_view detail) {
        ++traceWrites;
        traceEvent = event;
        traceDetail = detail;
    }
}

struct TraceValue {
    int* formatCalls;
};

template <> struct fmt::formatter<TraceValue> : fmt::formatter<int> {
    auto format(const TraceValue& value, fmt::format_context& context) const {
        ++*value.formatCalls;
        return fmt::formatter<int>::format(42, context);
    }
};

int main() {
    {
        using namespace mod::follow_distance;
        for (int preset = 0; preset < 3; ++preset) {
            const auto individual = Decode(Encode(preset, true), 1);
            const auto party = Decode(Encode(preset, false), 1);
            assert(individual.preset == preset && individual.individual);
            assert(party.preset == preset && !party.individual);
        }
        assert(Decode(-1, 2).preset == 2 && !Decode(-1, 2).individual);
        assert(Decode(42, -1).preset == 1);
        // Global Far replaces a Close override; changing one actor afterward
        // leaves the other actor's saved party-applied Far choice intact.
        int first = Encode(0, true), second = Encode(1, true);
        first = second = Encode(2, false);
        assert(Decode(first, 1).preset == 2 && !Decode(first, 1).individual);
        first = Encode(0, true);
        assert(Decode(first, 1).preset == 0 && Decode(second, 1).preset == 2);
        assert(Decode(second, 0).preset == 2); // Save choice wins over a different INI default.
    }
    {
        using namespace mod::settings_rules;
        assert(IniValue("[General]\r\n Value = 2 ; note\r\n[Debug]\r\nValue=1\r\n", "general", "value") == "2");
        assert(!IniValue("[Debug]\nValue=1\n", "General", "Value"));
        assert(IniValue(mod::settings_defaults::Ini, "General", "sPerkForms") == "");
        assert(ParseInteger(*IniValue(mod::settings_defaults::Ini, "General", "iMaxFollowers"), 1, 8) == 8);
        assert(ParseInteger(*IniValue(mod::settings_defaults::Ini, "General", "bFollowerHomes"), 0, 1) == 0);
        assert(ParseInteger(*IniValue(mod::settings_defaults::Ini, "General", "iFollowDistance"), 0, 2) == 1);
        assert(ParseInteger(" 2 // comment", 0, 2) == 2);
        for (auto text : {"", "invalid", "1junk", "-1", "3", "9999999999999"}) assert(!ParseInteger(text, 0, 2));
        assert(!ParseInteger("2", 0, 1)); // Boolean options accept only zero or one.
        assert(!ParseInteger("0", 1, 8));

        using namespace mod::settings_file;
        const auto directory = std::filesystem::current_path() / ("settings-test-" + std::to_string(GetCurrentProcessId()));
        const auto path = directory / "Plugins" / "settings.ini";
        assert(CreateMissing(path, mod::settings_defaults::Ini).result == Result::Created);
        auto read = [&] { std::ifstream file(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(file), {}); };
        assert(read() == mod::settings_defaults::Ini);
        const std::string custom = "[General]\r\niMaxFollowers=invalid\r\nUnknownUserSetting=keep\r\n";
        { std::ofstream file(path, std::ios::binary | std::ios::trunc); file << custom; }
        assert(CreateMissing(path, mod::settings_defaults::Ini).result == Result::Existing && read() == custom);
        assert(!ParseInteger(*IniValue(read(), "General", "iMaxFollowers"), 1, 8));
        assert(CreateMissing(path.parent_path(), mod::settings_defaults::Ini).result == Result::Failed);
        assert(read() == custom); // A creation failure cannot reset the user file.
        std::filesystem::remove(path); std::filesystem::remove(path.parent_path()); std::filesystem::remove(directory);
    }
    {
        using namespace mod::follower_view;
        const Space room{1, 0, true, true}, otherRoom{2, 0, true, true};
        const Space outdoors{3, 10, false, true}, nearby{4, 10, false, true}, otherWorld{5, 11, false, true};
        assert(Comparable(room, room) && !Comparable(room, otherRoom));
        assert(Comparable(outdoors, nearby) && !Comparable(outdoors, otherWorld));
        assert(!Comparable(room, outdoors));
        assert(!Comparable({0, 10, false, true}, outdoors));
        assert(!Comparable({3, 10, false, false}, outdoors));
        assert(!Comparable({3, 0, false, true}, {4, 0, false, true}));
        assert(DistanceMeters(room, room, {0, 0, 0}, {210, 280, 0}) == 5.0);
        assert(DistanceMeters(room, room, {0, 0, 0}, {0, 0, 0}) == 0.0);
        assert(!DistanceMeters(room, otherRoom, {0, 0, 0}, {0, 0, 0}));
        assert(!DistanceMeters(room, room, {std::numeric_limits<float>::infinity(), 0, 0}, {0, 0, 0}));
        assert(!DistanceMeters(room, room, {0, 0, 0}, {0, std::numeric_limits<float>::quiet_NaN(), 0}));
    }
    using mod::settings_rules::ParseFormID;
    assert(ParseFormID("00058F75") == 0x58F75);
    assert(ParseFormID("0x00058f75") == 0x58F75);
    assert(ParseFormID("0XFFFFFFFF") == 0xFFFFFFFF);
    for (const auto* invalid : {"", "0x", "58F75junk", "-1", "+1", " 1", "1 ", "100000000"}) assert(!ParseFormID(invalid));
    using namespace mod::debug_rules;
    {
        using mod::party_rules::Slot;
        assert(mod::party_rules::ValidBinding(true, 0, "Follower", true));
        assert(mod::party_rules::ValidBinding(true, 0, "follower", true));
        assert(mod::party_rules::ValidBinding(true, 0, "FoLlOwEr", true));
        assert(!mod::party_rules::ValidBinding(true, 0, "FollowerExtra", true));
        assert(!mod::party_rules::ValidBinding(true, 0, "Follower", false));
        assert(!mod::party_rules::ValidBinding(false, 0, "Follower", true));
        assert(!mod::party_rules::ValidBinding(true, 1, "Animal", false));
        for (std::uint32_t aliasID = 2; aliasID <= 8; ++aliasID) {
            const auto name = fmt::format("ExtraFollower0{}", aliasID - 1);
            assert(mod::party_rules::ValidBinding(true, aliasID, name, false));
            assert(mod::party_rules::ValidBinding(true, aliasID, fmt::format("extrafollower0{}", aliasID - 1), false));
            assert(mod::party_rules::ValidBinding(true, aliasID, fmt::format("EXTRAFOLLOWER0{}", aliasID - 1), false));
            assert(mod::party_rules::ValidBinding(true, aliasID, fmt::format("eXtRaFoLlOwEr0{}", aliasID - 1), false));
            assert(!mod::party_rules::ValidBinding(true, aliasID, name, true));
            assert(!mod::party_rules::ValidBinding(false, aliasID, name, false));
            assert(!mod::party_rules::ValidBinding(true, aliasID, "ExtraFollower99", false));
        }
        assert(!mod::party_rules::ValidBinding(true, 99, "ExtraFollower01", false));
        // Null slots and foreign/animal bindings are invalid, not free. A filled
        // non-actor alias must also stay occupied so recruitment cannot overwrite it.
        std::vector<Slot> slots{{}, {true, true, false, 0}, {true, false},
            {true, true, false, 0x123}, {true, true, false, 0x123},
            {true, true, true, 0x456}, {true, true, false, 0x789}};
        assert(mod::party_rules::FindActor(slots, 0) == -1);
        assert(mod::party_rules::FindActor(slots, 0x456) == 5);  // Dead actors remain managed until explicit cleanup.
        assert(mod::party_rules::FindActor(slots, 0x123) == 3);  // Prefer the first duplicate.
        assert(mod::party_rules::FindActor(slots, 0x999) == -1);
        assert(mod::party_rules::FindFree(slots) == 2);
        assert(mod::party_rules::FindFirstLiving(slots) == 3);
        assert(mod::party_rules::CountLive(slots) == 2);
        assert(slots[5].actorID == 0x456 && slots[5].occupied);  // Counting never clears a dead alias.
        slots[2].occupied = true;
        assert(mod::party_rules::FindFree(slots) == -1);
        slots.push_back({true, true, false, 0xABC});
        assert(mod::party_rules::CountLive(slots) == 3);
        slots.back() = {};  // Load/clear reads actual slots, not a permanent native registry.
        assert(mod::party_rules::CountLive(slots) == 2);
        assert(mod::party_rules::CountLive({}) == 0);
        assert(mod::party_rules::FindFree({}) == -1);
        assert(mod::party_rules::FindFirstLiving({}) == -1);
        for (auto& slot : slots) if (slot.actorID) slot.dead = true;
        assert(mod::party_rules::CountLive(slots) == 0);
        assert(mod::party_rules::FindFirstLiving(slots) == -1);
        const auto empty = mod::party_rules::CountGlobals(0, 4);
        assert(empty.vanillaCount == 0 && empty.partyCount == 0 && empty.recruitGate == 1);
        const auto partial = mod::party_rules::CountGlobals(3, 4);
        assert(partial.vanillaCount == 1 && partial.partyCount == 3 && partial.recruitGate == 1);
        const auto full = mod::party_rules::CountGlobals(4, 4);
        assert(full.vanillaCount == 1 && full.recruitGate == 0);
        assert(mod::party_rules::CountGlobals(5, 4).recruitGate == 0);  // Do not silently dismiss an over-cap party.
        assert(mod::party_rules::CountGlobals(-1, 0).partyCount == 0);
    }
    {
        mod::refresh::State refresh;
        const auto now = mod::refresh::State::Clock::time_point{} + std::chrono::seconds(10);
        refresh.MarkDirty();
        assert(refresh.Queue(now) == 0);  // Events and load completion do no work while hidden.
        refresh.SetVisible(true);
        const auto first = refresh.Queue(now);
        assert(first && refresh.Queue(now) == 0);  // Coalesce render frames.
        refresh.Inspect(0x123);  // Inspection arriving after polling queued must not be lost.
        const auto request = refresh.Start(first);
        assert(request && request->inspectID == 0x123);
        refresh.MarkDirty();  // A gameplay change during capture still needs a fresh read.
        refresh.Finish(first, request->revision);
        const auto changed = refresh.Queue(now);
        assert(changed);
        const auto changedRequest = refresh.Start(changed);
        assert(changedRequest && changedRequest->inspectID == 0);
        refresh.Finish(changed, changedRequest->revision);
        assert(refresh.Queue(now + std::chrono::milliseconds(999)) == 0);
        const auto poll = refresh.Queue(now + std::chrono::seconds(1));
        assert(poll);
        refresh.SetVisible(false);
        assert(!refresh.Start(poll));  // Closing cancels queued captures, not gameplay commands.
        refresh.MarkDirty();
        assert(refresh.Queue(now + std::chrono::seconds(5)) == 0);
        refresh.SetVisible(true);
        const auto reopened = refresh.Queue(now + std::chrono::seconds(1));
        assert(reopened && reopened != poll);  // Reopen immediately, before the polling interval.
        refresh.Finish(poll, request->revision);
        assert(refresh.Queue(now + std::chrono::seconds(1)) == 0);  // Old callback cannot free the new task.
        assert(refresh.Start(reopened));
        refresh.Reset();
        assert(!refresh.Start(reopened));
        assert(refresh.Queue(now + std::chrono::seconds(5)) == 0);
        refresh.SetVisible(true);
        const auto newSave = refresh.Queue(now + std::chrono::seconds(1));
        assert(newSave && newSave != reopened);
        const auto newRequest = refresh.Start(newSave);
        assert(newRequest);
        refresh.Finish(newSave, newRequest->revision);
        refresh.Inspect(0x456);
        const auto inspecting = refresh.Queue(now + std::chrono::seconds(1));
        const auto inspectRequest = refresh.Start(inspecting);
        assert(inspectRequest && inspectRequest->inspectID == 0x456);
        refresh.Inspect(0x789);  // A later inspection while capturing gets its own refresh.
        refresh.Finish(inspecting, inspectRequest->revision);
        const auto nextInspect = refresh.Queue(now + std::chrono::seconds(1));
        assert(refresh.Start(nextInspect)->inspectID == 0x789);
    }
    int formatCalls = 0, getterCalls = 0;
    auto diagnostic = [&] {
        ++getterCalls;
        return std::string("cap=4");
    };
    mod::debug::Logging = false;
    mod::debug::Trace("Native.Formatted", nullptr, "value={}", TraceValue{&formatCalls});
    mod::debug::TraceLazy("Native.Lazy", nullptr, diagnostic);
    assert(formatCalls == 0 && getterCalls == 0 && traceWrites == 0);

    mod::debug::Logging = true;
    mod::debug::Trace("Native.Formatted", nullptr, "value={}", TraceValue{&formatCalls});
    assert(formatCalls == 1 && traceWrites == 1);
    assert(traceEvent == "Native.Formatted" && traceDetail == "value=42");
    mod::debug::TraceLazy("Native.Lazy", nullptr, diagnostic);
    assert(getterCalls == 1 && traceWrites == 2);
    assert(traceEvent == "Native.Lazy" && traceDetail == "cap=4");
    mod::debug::Trace("Native.Fields", nullptr, "count={} base={:08X} recruit={}", -1, 0x123U, true);
    assert(traceDetail == "count=-1 base=00000123 recruit=true");
    mod::debug::Logging = false;
    mod::debug::Trace("Native.Formatted", nullptr, "value={}", TraceValue{&formatCalls});
    mod::debug::TraceLazy("Native.Lazy", nullptr, diagnostic);
    assert(formatCalls == 1 && getterCalls == 1 && traceWrites == 3);

    // Saved script names may differ in case from the installed PEX declaration.
    assert(IsFollowerScript("DialogueFollowerScript"));
    assert(IsFollowerScript("dialoguefollowerscript"));
    assert(IsFollowerScript("DIALOGUEFOLLOWERSCRIPT"));
    assert(IsFollowerScript("dIaLoGuEfOlLoWeRsCrIpT"));
    assert(!IsFollowerScript(""));
    assert(!IsFollowerScript("DialogueFollower"));
    assert(!IsFollowerScript("DialogueFollowerScriptExtra"));
    assert(!IsFollowerScript("QF_DialogueFollower_000750BA"));
    assert(!IsFollowerScript("TrainedAnimalScript"));
    // Missing saved bindings must permit Reconcile for the initialized lowercase script,
    // while normal actions and genuinely uninitialized scripts stay blocked.
    const bool recognized = IsFollowerScript("dialoguefollowerscript");
    assert(CanCommand(true, recognized, false, 0, true));
    assert(!CanCommand(true, recognized, false, 4, true));
    assert(!CanCommand(true, false, false, 0, true));
    const auto savedBindingsIssue = InstallationIssues(true, recognized, false, 8, 8);
    assert(savedBindingsIssue.find("saved") != std::string::npos);
    assert(savedBindingsIssue.find("not initialized") == std::string::npos);
    assert(InstallationIssues(true, true, true, 4, 8).empty());
    const auto structureIssue = InstallationIssues(false, true, false, 4, 1);
    assert(structureIssue.find("runtime aliases") != std::string::npos);
    assert(structureIssue.find("winning plugin") == std::string::npos);
    assert(InstallationIssues(true, true, false, 4, 8).find("saved") != std::string::npos);
    assert(InstallationIssues(true, false, false, 4, 8).find("not initialized") != std::string::npos);
    assert(InstallationIssues(true, true, true, 9, 8).find("capacity") != std::string::npos);
    assert(CanRepair(true, true, true, false));
    assert(CanRepair(true, true, false, true));
    assert(!CanRepair(true, true, false, false));
    assert(!CanRepair(false, true, true, true));
    assert(!CanRepair(true, false, true, true));
    for (std::int32_t action = 0; action <= 7; ++action) {
        assert(CanCommand(true, true, true, action, true));
        assert(CanCommand(true, true, true, action, false) == IsPartyAction(action));
        assert(!CanCommand(false, true, true, action, true));
        assert(!CanCommand(true, false, true, action, true));
        assert(CanCommand(true, true, false, action, true) == (action == 0));
    }
    assert(!CanCommand(true, true, true, -1, true));
    assert(CanCommand(true, true, true, 8, false)); // Primary selection is available without debug options.
    assert(!CanCommand(true, true, false, 8, false));
    assert(!CanCommand(true, true, true, 9, true));
    assert(!CanCommand(true, true, false, 0, false));
    std::vector<Row> rows{{0}, {0x123}, {0x456, false, true, true, 1}};
    assert(Diagnose(rows) == 2);
    for (const auto& row : rows) assert(row.issues.empty());
    assert(GlobalIssues(2, 4, 2, 1).empty());
    assert(GlobalIssues(4, 4, 4, 0).empty());
    assert(GlobalIssues(0, 1, 0, 1).empty());

    // A duplicate must not increase the real party count or mask a stale count.
    rows.push_back({0x123});
    assert(Diagnose(rows) == 2);
    assert(rows.back().issues.find("duplicate") != std::string::npos);
    assert(GlobalIssues(2, 4, 3, 1).find("count differs") != std::string::npos);
    assert(GlobalIssues(2, 4, 2.5f, 1).find("count differs") != std::string::npos);

    // Dead aliases remain visible, but neither they nor their duplicates count as live followers.
    rows.push_back({0x789, true});
    rows.push_back({0x789, true});
    assert(Diagnose(rows) == 2);
    assert(rows[4].issues.find("dead reference") != std::string::npos);
    assert(rows[5].issues.find("duplicate") != std::string::npos);

    rows[1].teammate = false;
    rows[1].currentFaction = false;
    rows[1].waiting = -1;
    assert(Diagnose(rows) == 2);
    assert(rows[1].issues.find("teammate") != std::string::npos);
    assert(rows[1].issues.find("CurrentFollowerFaction") != std::string::npos);
    assert(rows[1].issues.find("invalid waiting") != std::string::npos);
    rows[1].teammate = rows[1].currentFaction = true;
    rows[1].waiting = 0;
    Diagnose(rows);
    assert(rows[1].issues.empty());  // A later refresh clears old diagnoses.
    assert(GlobalIssues(2, 4, 2, 0).find("recruitment gate") != std::string::npos);
    assert(GlobalIssues(3, 2, 3, 0).find("exceeds") != std::string::npos);

    // Work queued before loading another save must never execute against the new game.
    assert(CurrentGame(7, 7, true));
    assert(!CurrentGame(7, 8, true));
    assert(!CurrentGame(7, 7, false));
    assert(CurrentCommand(123, 123, true));
    assert(!CurrentCommand(123, 124, true));
    assert(!CurrentCommand(123, 0, true));
    assert(!CurrentCommand(0, 0, true));
    assert(!CurrentCommand(123, 123, false));
    std::cout << "Visibility-gated refresh, logging, diagnostics and save-generation regression checks passed.\n";
}
