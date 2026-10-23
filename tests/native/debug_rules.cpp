#include <doctest/doctest.h>

#include "diagnostics/DebugRules.h"
#include "diagnostics/Debug.h"
#include "ui/Refresh.h"
#include "followers/PartyRules.h"
#include "settings/SettingsRules.h"
#include "SettingsDefaults.h"
#include "settings/SettingsFile.h"
#include "followers/FollowerView.h"
#include "followers/FollowDistanceRules.h"
#include "TemporaryDirectory.h"

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

template <>
struct fmt::formatter<TraceValue> : fmt::formatter<int> {
    auto format(const TraceValue& value, fmt::format_context& context) const {
        ++*value.formatCalls;
        return fmt::formatter<int>::format(42, context);
    }
};

namespace {
    using namespace mod::debug_rules;

    struct InstallationFixture {
        bool structureReady = true;
        bool scriptReady = true;
        bool bindingsReady = true;
        bool debugOptions = true;

        bool Allows(std::int32_t action) const {
            return CanCommand(structureReady, scriptReady, bindingsReady, action, debugOptions);
        }

        bool CanRepair(bool reconcile = false) const {
            return mod::debug_rules::CanRepair(structureReady, scriptReady, bindingsReady, reconcile);
        }

        std::string Issues(std::int32_t cap, std::int32_t capacity) const {
            return InstallationIssues(structureReady, scriptReady, bindingsReady, cap, capacity);
        }
    };

    TEST_CASE("Follow Distance Choices") {
        using namespace mod::follow_distance;
        for (int preset = 0; preset < 3; ++preset) {
            const auto individual = Decode(Encode(preset, true), 1);
            const auto party = Decode(Encode(preset, false), 1);
            REQUIRE((individual.preset == preset && individual.individual));
            REQUIRE((party.preset == preset && !party.individual));
        }
        REQUIRE((Decode(-1, 2).preset == 2 && !Decode(-1, 2).individual));
        REQUIRE((Decode(42, -1).preset == 1));
        // Global Far replaces a Close override; changing one actor afterward
        // leaves the other actor's saved party-applied Far choice intact.
        int first = Encode(0, true), second = Encode(1, true);
        first = second = Encode(2, false);
        REQUIRE((Decode(first, 1).preset == 2 && !Decode(first, 1).individual));
        first = Encode(0, true);
        REQUIRE((Decode(first, 1).preset == 0 && Decode(second, 1).preset == 2));
        REQUIRE((Decode(second, 0).preset == 2));  // Save choice wins over a different INI default.
    }

    TEST_CASE("Ini Parsing") {
        using namespace mod::settings_rules;
        REQUIRE((IniValue("[General]\r\n Value = 2 ; note\r\n[Debug]\r\nValue=1\r\n", "general", "value") == "2"));
        REQUIRE((!IniValue("[Debug]\nValue=1\n", "General", "Value")));
        REQUIRE((IniValue(mod::settings_defaults::Ini, "General", "sPerkForms") == ""));
        REQUIRE((ParseInteger(*IniValue(mod::settings_defaults::Ini, "General", "iMaxFollowers"), 1, 8) == 8));
        REQUIRE((ParseInteger(*IniValue(mod::settings_defaults::Ini, "General", "bFollowerHomes"), 0, 1) == 0));
        REQUIRE((ParseInteger(*IniValue(mod::settings_defaults::Ini, "General", "iFollowDistance"), 0, 2) == 1));
        REQUIRE((ParseInteger(*IniValue(mod::settings_defaults::Ini, "General", "iTravelFormation"), 0, 3) == 0));
        REQUIRE((ParseInteger(" 2 // comment", 0, 2) == 2));
        for (auto text : {"", "invalid", "1junk", "-1", "3", "9999999999999"})
            REQUIRE((!ParseInteger(text, 0, 2)));
        REQUIRE((!ParseInteger("2", 0, 1)));  // Boolean options accept only zero or one.
        REQUIRE((!ParseInteger("0", 1, 8)));
    }

    TEST_CASE("Settings File Creation") {
        using namespace mod::settings_rules;
        using namespace mod::settings_file;
        const tests::TemporaryDirectory directory;
        const auto path = directory.path / "Plugins" / "settings.ini";
        REQUIRE((CreateMissing(path, mod::settings_defaults::Ini).result == Result::Created));
        auto read = [&] {
            std::ifstream file(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(file), {});
        };
        REQUIRE((read() == mod::settings_defaults::Ini));
        const std::string custom = "[General]\r\niMaxFollowers=invalid\r\nUnknownUserSetting=keep\r\n";
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file << custom;
        }
        REQUIRE((CreateMissing(path, mod::settings_defaults::Ini).result == Result::Existing && read() == custom));
        REQUIRE((!ParseInteger(*IniValue(read(), "General", "iMaxFollowers"), 1, 8)));
        REQUIRE((CreateMissing(path.parent_path(), mod::settings_defaults::Ini).result == Result::Failed));
        REQUIRE((read() == custom));  // A creation failure cannot reset the user file.
    }

    TEST_CASE("Follower Positions") {
        using namespace mod::follower_view;
        const Space room{1, 0, true, true}, otherRoom{2, 0, true, true};
        const Space outdoors{3, 10, false, true}, nearby{4, 10, false, true}, otherWorld{5, 11, false, true};
        REQUIRE((Comparable(room, room) && !Comparable(room, otherRoom)));
        REQUIRE((Comparable(outdoors, nearby) && !Comparable(outdoors, otherWorld)));
        REQUIRE((!Comparable(room, outdoors)));
        REQUIRE((!Comparable({0, 10, false, true}, outdoors)));
        REQUIRE((!Comparable({3, 10, false, false}, outdoors)));
        REQUIRE((!Comparable({3, 0, false, true}, {4, 0, false, true})));
        REQUIRE((DistanceMeters(room, room, {0, 0, 0}, {210, 280, 0}) == 5.0));
        REQUIRE((DistanceMeters(room, room, {0, 0, 0}, {0, 0, 0}) == 0.0));
        REQUIRE((!DistanceMeters(room, otherRoom, {0, 0, 0}, {0, 0, 0})));
        REQUIRE((!DistanceMeters(room, room, {std::numeric_limits<float>::infinity(), 0, 0}, {0, 0, 0})));
        REQUIRE((!DistanceMeters(room, room, {0, 0, 0}, {0, std::numeric_limits<float>::quiet_NaN(), 0})));
    }

    TEST_CASE("Form ID Parsing") {
        using mod::settings_rules::ParseFormID;
        REQUIRE((ParseFormID("00058F75") == 0x58F75));
        REQUIRE((ParseFormID("0x00058f75") == 0x58F75));
        REQUIRE((ParseFormID("0XFFFFFFFF") == 0xFFFFFFFF));
        for (const auto* invalid : {"", "0x", "58F75junk", "-1", "+1", " 1", "1 ", "100000000"})
            REQUIRE((!ParseFormID(invalid)));
    }

    TEST_CASE("Alias Bindings") {
        REQUIRE((mod::party_rules::ValidBinding(true, 0, "Follower", true)));
        REQUIRE((mod::party_rules::ValidBinding(true, 0, "follower", true)));
        REQUIRE((mod::party_rules::ValidBinding(true, 0, "FoLlOwEr", true)));
        REQUIRE((!mod::party_rules::ValidBinding(true, 0, "FollowerExtra", true)));
        REQUIRE((!mod::party_rules::ValidBinding(true, 0, "Follower", false)));
        REQUIRE((!mod::party_rules::ValidBinding(false, 0, "Follower", true)));
        REQUIRE((!mod::party_rules::ValidBinding(true, 1, "Animal", false)));
        for (std::uint32_t aliasID = 2; aliasID <= 8; ++aliasID) {
            const auto name = fmt::format("ExtraFollower0{}", aliasID - 1);
            REQUIRE((mod::party_rules::ValidBinding(true, aliasID, name, false)));
            REQUIRE(
                (mod::party_rules::ValidBinding(true, aliasID, fmt::format("extrafollower0{}", aliasID - 1), false)));
            REQUIRE(
                (mod::party_rules::ValidBinding(true, aliasID, fmt::format("EXTRAFOLLOWER0{}", aliasID - 1), false)));
            REQUIRE(
                (mod::party_rules::ValidBinding(true, aliasID, fmt::format("eXtRaFoLlOwEr0{}", aliasID - 1), false)));
            REQUIRE((!mod::party_rules::ValidBinding(true, aliasID, name, true)));
            REQUIRE((!mod::party_rules::ValidBinding(false, aliasID, name, false)));
            REQUIRE((!mod::party_rules::ValidBinding(true, aliasID, "ExtraFollower99", false)));
        }
        REQUIRE((!mod::party_rules::ValidBinding(true, 99, "ExtraFollower01", false)));
    }

    TEST_CASE("Slot Discovery And Counts") {
        using mod::party_rules::Slot;
        // Null slots and foreign/animal bindings are invalid, not free. A filled
        // non-actor alias must also stay occupied so recruitment cannot overwrite it.
        std::vector<Slot> slots{{},                                 // Invalid alias.
                                {.valid = true, .occupied = true},  // Occupied by a non-actor.
                                {.valid = true},                    // Available alias.
                                {.valid = true, .occupied = true, .actorID = 0x123},
                                {.valid = true, .occupied = true, .actorID = 0x123},  // Duplicate registration.
                                {.valid = true, .occupied = true, .dead = true, .actorID = 0x456},
                                {.valid = true, .occupied = true, .actorID = 0x789}};
        REQUIRE((mod::party_rules::FindActor(slots, 0) == -1));
        REQUIRE(
            (mod::party_rules::FindActor(slots, 0x456) == 5));  // Dead actors remain managed until explicit cleanup.
        REQUIRE((mod::party_rules::FindActor(slots, 0x123) == 3));  // Prefer the first duplicate.
        REQUIRE((mod::party_rules::FindActor(slots, 0x999) == -1));
        REQUIRE((mod::party_rules::FindFree(slots) == 2));
        REQUIRE((mod::party_rules::FindFirstLiving(slots) == 3));
        REQUIRE((mod::party_rules::CountLive(slots) == 2));
        REQUIRE((slots[5].actorID == 0x456 && slots[5].occupied));  // Counting never clears a dead alias.
        slots[2].occupied = true;
        REQUIRE((mod::party_rules::FindFree(slots) == -1));
        slots.push_back({.valid = true, .occupied = true, .actorID = 0xABC});
        REQUIRE((mod::party_rules::CountLive(slots) == 3));
        slots.back() = {};  // Load/clear reads actual slots, not a permanent native registry.
        REQUIRE((mod::party_rules::CountLive(slots) == 2));
        REQUIRE((mod::party_rules::CountLive({}) == 0));
        REQUIRE((mod::party_rules::FindFree({}) == -1));
        REQUIRE((mod::party_rules::FindFirstLiving({}) == -1));
        for (auto& slot : slots)
            if (slot.actorID)
                slot.dead = true;
        REQUIRE((mod::party_rules::CountLive(slots) == 0));
        REQUIRE((mod::party_rules::FindFirstLiving(slots) == -1));
    }

    TEST_CASE("Party Globals") {
        const auto empty = mod::party_rules::CountGlobals(0, 4);
        REQUIRE((empty.vanillaCount == 0 && empty.partyCount == 0 && empty.recruitGate == 1));
        const auto partial = mod::party_rules::CountGlobals(3, 4);
        REQUIRE((partial.vanillaCount == 1 && partial.partyCount == 3 && partial.recruitGate == 1));
        const auto full = mod::party_rules::CountGlobals(4, 4);
        REQUIRE((full.vanillaCount == 1 && full.recruitGate == 0));
        REQUIRE((mod::party_rules::CountGlobals(5, 4).recruitGate == 0));  // Do not silently dismiss an over-cap party.
        REQUIRE((mod::party_rules::CountGlobals(-1, 0).partyCount == 0));
        const auto questOnly = mod::party_rules::CountGlobals(0, 8, true);
        REQUIRE((questOnly.vanillaCount == 1 && questOnly.partyCount == 0 && questOnly.recruitGate == 1));
    }

    TEST_CASE("Dialogue Recruitment Gate") {
        using mod::party_rules::DialogueFollowerCount;
        // Native followers first, then Serana: only her recruitment dialogue sees zero.
        REQUIRE((DialogueFollowerCount(2, 8, false, true, false) == 0));
        REQUIRE((DialogueFollowerCount(2, 8, false, false, false) == 1));
        // Background refresh uses the same speaker policy, even with a full native party.
        REQUIRE((DialogueFollowerCount(8, 8, false, true, false) == 0));
        // Serana first, then a normal follower: external presence does not consume a slot.
        REQUIRE((DialogueFollowerCount(0, 8, true, false, true) == 0));
        REQUIRE((DialogueFollowerCount(8, 8, true, false, true) == 1));
        REQUIRE((DialogueFollowerCount(0, 8, false, false, true) == 1));
        REQUIRE((DialogueFollowerCount(0, 8, false, false, false) == 0));
    }

    TEST_CASE("Visible Refresh Lifecycle") {
        mod::refresh::State refresh;
        const auto now = mod::refresh::State::Clock::time_point{} + std::chrono::seconds(10);
        refresh.MarkDirty();
        REQUIRE((refresh.Queue(now) == 0));  // Events and load completion do no work while hidden.
        refresh.SetVisible(true);
        const auto first = refresh.Queue(now);
        REQUIRE((first && refresh.Queue(now) == 0));  // Coalesce render frames.
        refresh.Inspect(0x123);                       // Inspection arriving after polling queued must not be lost.
        const auto request = refresh.Start(first);
        REQUIRE((request && request->inspectID == 0x123));
        refresh.MarkDirty();  // A gameplay change during capture still needs a fresh read.
        refresh.Finish(first, request->revision);
        const auto changed = refresh.Queue(now);
        REQUIRE((changed));
        const auto changedRequest = refresh.Start(changed);
        REQUIRE((changedRequest && changedRequest->inspectID == 0));
        refresh.Finish(changed, changedRequest->revision);
        REQUIRE((refresh.Queue(now + std::chrono::milliseconds(999)) == 0));
        const auto poll = refresh.Queue(now + std::chrono::seconds(1));
        REQUIRE((poll));
        refresh.SetVisible(false);
        REQUIRE((!refresh.Start(poll)));  // Closing cancels queued captures, not gameplay commands.
        refresh.MarkDirty();
        REQUIRE((refresh.Queue(now + std::chrono::seconds(5)) == 0));
        refresh.SetVisible(true);
        const auto reopened = refresh.Queue(now + std::chrono::seconds(1));
        REQUIRE((reopened && reopened != poll));  // Reopen immediately, before the polling interval.
        refresh.Finish(poll, request->revision);
        REQUIRE((refresh.Queue(now + std::chrono::seconds(1)) == 0));  // Old callback cannot free the new task.
        REQUIRE((refresh.Start(reopened)));
        refresh.Reset();
        REQUIRE((!refresh.Start(reopened)));
        REQUIRE((refresh.Queue(now + std::chrono::seconds(5)) == 0));
        refresh.SetVisible(true);
        const auto newSave = refresh.Queue(now + std::chrono::seconds(1));
        REQUIRE((newSave && newSave != reopened));
        const auto newRequest = refresh.Start(newSave);
        REQUIRE((newRequest));
        refresh.Finish(newSave, newRequest->revision);
        refresh.Inspect(0x456);
        const auto inspecting = refresh.Queue(now + std::chrono::seconds(1));
        const auto inspectRequest = refresh.Start(inspecting);
        REQUIRE((inspectRequest && inspectRequest->inspectID == 0x456));
        refresh.Inspect(0x789);  // A later inspection while capturing gets its own refresh.
        refresh.Finish(inspecting, inspectRequest->revision);
        const auto nextInspect = refresh.Queue(now + std::chrono::seconds(1));
        REQUIRE((refresh.Start(nextInspect)->inspectID == 0x789));
    }

    TEST_CASE("Logging Defers Formatting") {
        int formatCalls = 0, getterCalls = 0;
        auto diagnostic = [&] {
            ++getterCalls;
            return std::string("cap=4");
        };
        mod::debug::Logging = false;
        mod::debug::Trace("Native.Formatted", nullptr, "value={}", TraceValue{&formatCalls});
        mod::debug::TraceLazy("Native.Lazy", nullptr, diagnostic);
        REQUIRE((formatCalls == 0 && getterCalls == 0 && traceWrites == 0));

        mod::debug::Logging = true;
        mod::debug::Trace("Native.Formatted", nullptr, "value={}", TraceValue{&formatCalls});
        REQUIRE((formatCalls == 1 && traceWrites == 1));
        REQUIRE((traceEvent == "Native.Formatted" && traceDetail == "value=42"));
        mod::debug::TraceLazy("Native.Lazy", nullptr, diagnostic);
        REQUIRE((getterCalls == 1 && traceWrites == 2));
        REQUIRE((traceEvent == "Native.Lazy" && traceDetail == "cap=4"));
        mod::debug::Trace("Native.Fields", nullptr, "count={} base={:08X} recruit={}", -1, 0x123U, true);
        REQUIRE((traceDetail == "count=-1 base=00000123 recruit=true"));
        mod::debug::Logging = false;
        mod::debug::Trace("Native.Formatted", nullptr, "value={}", TraceValue{&formatCalls});
        mod::debug::TraceLazy("Native.Lazy", nullptr, diagnostic);
        REQUIRE((formatCalls == 1 && getterCalls == 1 && traceWrites == 3));
    }

    TEST_CASE("Follower Script Recognition") {
        // Saved script names may differ in case from the installed PEX declaration.
        REQUIRE((IsFollowerScript("DialogueFollowerScript")));
        REQUIRE((IsFollowerScript("dialoguefollowerscript")));
        REQUIRE((IsFollowerScript("DIALOGUEFOLLOWERSCRIPT")));
        REQUIRE((IsFollowerScript("dIaLoGuEfOlLoWeRsCrIpT")));
        REQUIRE((!IsFollowerScript("")));
        REQUIRE((!IsFollowerScript("DialogueFollower")));
        REQUIRE((!IsFollowerScript("DialogueFollowerScriptExtra")));
        REQUIRE((!IsFollowerScript("QF_DialogueFollower_000750BA")));
        REQUIRE((!IsFollowerScript("TrainedAnimalScript")));
    }

    TEST_CASE("Missing Saved Bindings") {
        // Missing saved bindings must permit Reconcile for the initialized lowercase script,
        // while normal actions and genuinely uninitialized scripts stay blocked.
        const bool recognized = IsFollowerScript("dialoguefollowerscript");
        const InstallationFixture missingBindings{.scriptReady = recognized, .bindingsReady = false};
        const InstallationFixture uninitialized{.scriptReady = false, .bindingsReady = false};
        REQUIRE((missingBindings.Allows(0)));
        REQUIRE((!missingBindings.Allows(4)));
        REQUIRE((!uninitialized.Allows(0)));
        const auto savedBindingsIssue = missingBindings.Issues(8, 8);
        REQUIRE((savedBindingsIssue.find("saved") != std::string::npos));
        REQUIRE((savedBindingsIssue.find("not initialized") == std::string::npos));
    }

    TEST_CASE("Installation Issues") {
        const InstallationFixture healthy;
        const InstallationFixture brokenStructure{.structureReady = false, .bindingsReady = false};
        const InstallationFixture missingBindings{.bindingsReady = false};
        const InstallationFixture uninitialized{.scriptReady = false, .bindingsReady = false};
        REQUIRE((healthy.Issues(4, 8).empty()));
        const auto structureIssue = brokenStructure.Issues(4, 1);
        REQUIRE((structureIssue.find("runtime aliases") != std::string::npos));
        REQUIRE((structureIssue.find("winning plugin") == std::string::npos));
        REQUIRE((missingBindings.Issues(4, 8).find("saved") != std::string::npos));
        REQUIRE((uninitialized.Issues(4, 8).find("not initialized") != std::string::npos));
        REQUIRE((healthy.Issues(9, 8).find("capacity") != std::string::npos));
    }

    TEST_CASE("Repair Permissions") {
        const InstallationFixture healthy;
        const InstallationFixture missingBindings{.bindingsReady = false};
        const InstallationFixture brokenStructure{.structureReady = false};
        const InstallationFixture uninitialized{.scriptReady = false};
        REQUIRE((healthy.CanRepair()));
        REQUIRE((missingBindings.CanRepair(true)));
        REQUIRE((!missingBindings.CanRepair()));
        REQUIRE((!brokenStructure.CanRepair(true)));
        REQUIRE((!uninitialized.CanRepair(true)));
    }

    TEST_CASE("Command Permissions") {
        const InstallationFixture healthy;
        const InstallationFixture normalGameplay{.debugOptions = false};
        const InstallationFixture brokenStructure{.structureReady = false};
        const InstallationFixture uninitialized{.scriptReady = false};
        const InstallationFixture missingBindings{.bindingsReady = false};
        const InstallationFixture missingBindingsWithoutDebug{.bindingsReady = false, .debugOptions = false};
        for (std::int32_t action = 0; action <= 7; ++action) {
            REQUIRE((healthy.Allows(action)));
            REQUIRE((normalGameplay.Allows(action) == IsPartyAction(action)));
            REQUIRE((!brokenStructure.Allows(action)));
            REQUIRE((!uninitialized.Allows(action)));
            REQUIRE((missingBindings.Allows(action) == (action == 0)));
        }
        REQUIRE((!healthy.Allows(-1)));
        REQUIRE((normalGameplay.Allows(8)));  // Primary selection is available without debug options.
        REQUIRE((!missingBindingsWithoutDebug.Allows(8)));
        REQUIRE((healthy.Allows(9)));
        REQUIRE((!normalGameplay.Allows(9)));
        REQUIRE((!missingBindings.Allows(9)));
        REQUIRE((!healthy.Allows(10)));
        REQUIRE((!missingBindingsWithoutDebug.Allows(0)));
    }

    TEST_CASE("Follower Row Diagnostics") {
        std::vector<Row> rows{{}, {.formID = 0x123}, {.formID = 0x456, .waiting = 1}};
        REQUIRE((Diagnose(rows) == 2));
        for (const auto& row : rows)
            REQUIRE((row.issues.empty()));
        REQUIRE((GlobalIssues(2, 4, 2, 1).empty()));
        REQUIRE((GlobalIssues(4, 4, 4, 0).empty()));
        REQUIRE((GlobalIssues(0, 1, 0, 1).empty()));

        // A duplicate must not increase the real party count or mask a stale count.
        rows.push_back({.formID = 0x123});
        REQUIRE((Diagnose(rows) == 2));
        REQUIRE((rows.back().issues.find("duplicate") != std::string::npos));
        REQUIRE((GlobalIssues(2, 4, 3, 1).find("count differs") != std::string::npos));
        REQUIRE((GlobalIssues(2, 4, 2.5f, 1).find("count differs") != std::string::npos));

        // Dead aliases remain visible, but neither they nor their duplicates count as live followers.
        rows.push_back({.formID = 0x789, .dead = true});
        rows.push_back({.formID = 0x789, .dead = true});
        REQUIRE((Diagnose(rows) == 2));
        REQUIRE((rows[4].issues.find("dead reference") != std::string::npos));
        REQUIRE((rows[5].issues.find("duplicate") != std::string::npos));

        rows[1].teammate = false;
        rows[1].currentFaction = false;
        rows[1].waiting = -1;
        REQUIRE((Diagnose(rows) == 2));
        REQUIRE((rows[1].issues.find("teammate") != std::string::npos));
        REQUIRE((rows[1].issues.find("CurrentFollowerFaction") != std::string::npos));
        REQUIRE((rows[1].issues.find("invalid waiting") != std::string::npos));
        rows[1].teammate = rows[1].currentFaction = true;
        rows[1].waiting = 0;
        Diagnose(rows);
        REQUIRE((rows[1].issues.empty()));  // A later refresh clears old diagnoses.
        REQUIRE((GlobalIssues(2, 4, 2, 0).find("recruitment gate") != std::string::npos));
        REQUIRE((GlobalIssues(3, 2, 3, 0).find("exceeds") != std::string::npos));
    }

    TEST_CASE("Save And Command Generations") {
        // Work queued before loading another save must never execute against the new game.
        REQUIRE((CurrentGame(7, 7, true)));
        REQUIRE((!CurrentGame(7, 8, true)));
        REQUIRE((!CurrentGame(7, 7, false)));
        REQUIRE((CurrentCommand(123, 123, true)));
        REQUIRE((!CurrentCommand(123, 124, true)));
        REQUIRE((!CurrentCommand(123, 0, true)));
        REQUIRE((!CurrentCommand(0, 0, true)));
        REQUIRE((!CurrentCommand(123, 123, false)));
    }

}
