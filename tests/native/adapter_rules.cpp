#include <doctest/doctest.h>

#include "adapters/AdapterRules.h"
#include "adapters/MovementRules.h"
#include "AdapterCommandState.h"
#include "common/MovementRecords.h"

namespace {
    TEST_CASE("Adapter observations do not require a visible menu") {
        float seconds = 0;
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, 0));
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, -1));
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, 0.5f));
        REQUIRE(mod::adapter_rules::ObservationDue(seconds, 0.5f));
        REQUIRE(seconds == 0);
        REQUIRE(mod::adapter_rules::ObservationDue(seconds, 5));
        REQUIRE_FALSE(mod::adapter_rules::ObservationDue(seconds, 0));
    }

    using namespace yliwf::sdk;
    using mod::adapter_rules::Requests;
    using mod::adapter_rules::Valid;

    static_assert(sizeof(void*) == 8 && sizeof(FollowerState) == 204 && BaseAdapterSize == 56 &&
                  offsetof(Adapter, canRecruitThroughDialogue) == 64 &&
                  offsetof(Adapter, canReceiveCombatProtection) == 72 &&
                  offsetof(Adapter, canTravelInFormation) == 80 && sizeof(Adapter) == 120 && BaseAPISize == 24 &&
                  sizeof(API) == 40);

    Adapter ValidAdapter() {
        Adapter adapter;
        adapter.id = "test.controller";
        adapter.name = "Test controller";
        adapter.enumerate = [](void*, ActorID*, std::uint32_t) { return 0u; };
        adapter.inspect = [](void*, ActorID, FollowerState*) { return 1u; };
        adapter.start = [](void*, ActorID, Command, RequestID, char*, std::uint32_t) { return StartResult::Pending; };
        adapter.setFollowDistance = [](void*, ActorID, FollowDistance) { return 1u; };
        adapter.canRecruitThroughDialogue = [](void*, ActorID) { return 1u; };
        adapter.canReceiveCombatProtection = [](void*, ActorID) { return 1u; };
        adapter.canTravelInFormation = [](void*, ActorID) { return 1u; };
        adapter.setFormationPackage = [](void*, ActorID, std::uint32_t) { return 1u; };
        return adapter;
    }

    TEST_CASE("Core movement is an optional v1 extension with explicit feature support") {
        auto adapter = ValidAdapter();
        adapter.setFollowDistance = nullptr;
        adapter.canTravelInFormation = nullptr;
        adapter.setFormationPackage = nullptr;
        adapter.movementPermissions = [](void*, ActorID, std::uint32_t, const FollowerState*) { return AllMovement; };
        adapter.coreMovementFeatures = AllMovement;
        CHECK(Valid(adapter));
        CHECK(HasFollowDistance(adapter));
        CHECK(HasTravelFormation(adapter));
        CHECK(HasStandDown(adapter));
        adapter.size = 104;
        CHECK(Valid(adapter));
        CHECK_FALSE(HasFollowDistance(adapter));
        CHECK_FALSE(HasTravelFormation(adapter));
        CHECK_FALSE(HasStandDown(adapter));
        adapter.size = sizeof(Adapter);
        adapter.coreMovementFeatures = 8;
        CHECK_FALSE(Valid(adapter));
        adapter.coreMovementFeatures = AllMovement;
        adapter.movementPermissions = nullptr;
        CHECK_FALSE(Valid(adapter));
    }

    TEST_CASE("A previous v1 host still exposes state notification without core movement") {
        API host;
        host.stateChanged = [](AdapterID) {};
        host.isMovementPackage = [](std::uint32_t) { return 1u; };
        CHECK(HasCoreMovement(&host));
        host.size = 32;
        CHECK(CanNotifyState(&host));
        CHECK_FALSE(HasCoreMovement(&host));
    }

    TEST_CASE("Shared movement resolves existing record ranges without accepting overflow indexes") {
        using namespace mod::movement_records;
        CHECK(Distance(0) == 0x990);
        CHECK(Distance(2) == 0x992);
        CHECK(Distance(Distances) == 0);
        CHECK(Formation(0) == 0x9A0);
        CHECK(Formation(7) == 0x9A7);
        CHECK(Formation(8) == 0x9C0);
        CHECK(Formation(15) == 0x9C7);
        CHECK(Formation(Formations) == 0);
        CHECK(PassiveOrAvoidance(0) == 0xA11);
        CHECK(PassiveOrAvoidance(3) == 0xA14);
        CHECK(PassiveOrAvoidance(4) == 0xA20);
        CHECK(PassiveOrAvoidance(35) == 0xA3F);
        CHECK(PassiveOrAvoidance(StandDown) == 0);
    }

    mod::adapter_movement_rules::Snapshot Walking() {
        return {.state = State::Following, .permissions = AllMovement, .available = true, .normalMovement = true};
    }

    TEST_CASE("Shared movement preserves requested priority without unauthorized fallback") {
        using namespace mod::adapter_movement_rules;
        auto value = Walking();
        CHECK(Select(value, false, false, false) == 0);
        CHECK(Select(value, true, false, false) == MovementBit(Movement::Distance));
        CHECK(Select(value, true, true, false) == MovementBit(Movement::Formation));
        CHECK(Select(value, true, true, true) == MovementBit(Movement::StandDown));
        value.permissions = MovementBit(Movement::Distance);
        CHECK(Select(value, true, true, false) == 0);
        CHECK(Select(value, true, false, true) == 0);
        value.permissions = 0;
        CHECK(Select(value, true, false, false) == 0);
        value.permissions = 8;
        CHECK(Select(value, true, true, true) == 0);
    }

    TEST_CASE("Shared movement preserves waiting and permits Stand down during combat") {
        using namespace mod::adapter_movement_rules;
        auto value = Walking();
        value.combat = true;
        CHECK_FALSE(Permitted(value, Movement::Distance));
        CHECK_FALSE(Permitted(value, Movement::Formation));
        CHECK(Permitted(value, Movement::StandDown));
        value.combat = false;
        value.state = State::Waiting;
        CHECK_FALSE(Permitted(value, Movement::Distance));
        CHECK_FALSE(Permitted(value, Movement::Formation));
        CHECK(Permitted(value, Movement::StandDown));
        for (const auto state : {State::Inactive, State::Unavailable}) {
            value.state = state;
            CHECK(Select(value, true, true, true) == 0);
        }
    }

    TEST_CASE("Shared movement yields to pending owners, scenes and foreign interrupts") {
        using namespace mod::adapter_movement_rules;
        for (const auto field : {&Snapshot::pending, &Snapshot::scene, &Snapshot::foreignInterrupt}) {
            auto value = Walking();
            value.*field = true;
            CHECK(Select(value, true, false, false) == 0);
            CHECK(Select(value, true, true, true) == 0);
        }
        for (const auto field : {&Snapshot::available, &Snapshot::normalMovement}) {
            auto value = Walking();
            value.*field = false;
            CHECK(Select(value, true, true, true) == 0);
        }
    }

    TEST_CASE("Shared movement refresh is bounded and ignores inactive time") {
        float seconds = 0;
        CHECK_FALSE(mod::adapter_movement_rules::RefreshDue(seconds, -1));
        CHECK_FALSE(mod::adapter_movement_rules::RefreshDue(seconds, 0));
        CHECK_FALSE(mod::adapter_movement_rules::RefreshDue(seconds, 0.1f));
        CHECK(mod::adapter_movement_rules::RefreshDue(seconds, 0.1f));
        CHECK(seconds == 0);
        CHECK(mod::adapter_movement_rules::RefreshDue(seconds, 5));
        CHECK_FALSE(mod::adapter_movement_rules::RefreshDue(seconds, 0));
    }

    TEST_CASE("Owner command callbacks cannot complete twice or clear a newer request") {
        papyrus::CommandState calls;
        CHECK_FALSE(calls.Begin(0, 1));
        CHECK_FALSE(calls.Begin(1, 0));
        CHECK(calls.Begin(1, 10));
        CHECK_FALSE(calls.Begin(1, 11));
        const auto epoch = calls.Generation();
        CHECK_FALSE(calls.Finish(1, 11, epoch));
        CHECK(calls.Pending(1));
        CHECK(calls.Finish(1, 10, epoch));
        CHECK_FALSE(calls.Finish(1, 10, epoch));
        CHECK(calls.Begin(1, 11));
        CHECK_FALSE(calls.Finish(1, 10, epoch));
        CHECK(calls.Pending(1));
        calls.Invalidate();
        CHECK_FALSE(calls.Pending(1));
        CHECK(calls.Begin(1, 12));
        CHECK_FALSE(calls.Finish(1, 12, epoch));
        CHECK(calls.Pending(1));
        CHECK(calls.Finish(1, 12, calls.Generation()));
    }

    TEST_CASE("Stand-down is optional and never reads beyond an older adapter table") {
        auto adapter = ValidAdapter();
        adapter.setStandDownPackage = [](void*, ActorID, std::uint32_t) { return 1u; };
        CHECK(HasStandDown(adapter));
        adapter.size = 96;
        CHECK_FALSE(HasStandDown(adapter));
        adapter.size = sizeof(Adapter);
        adapter.setStandDownPackage = nullptr;
        CHECK_FALSE(HasStandDown(adapter));
    }

    TEST_CASE("Host Notification Compatibility") {
        API host;
        host.stateChanged = [](AdapterID) {};
        REQUIRE_MESSAGE((CanNotifyState(&host)), "Host notification extension unavailable");

        host.size = BaseAPISize;
        REQUIRE_MESSAGE((!CanNotifyState(&host) && !CanNotifyState(nullptr)),
                        "Old or absent host exposes optional callback");
    }

    TEST_CASE("Adapter Table Compatibility") {
        REQUIRE_MESSAGE((!Valid(Adapter{})), "Incomplete adapter accepted");

        auto adapter = ValidAdapter();
        REQUIRE_MESSAGE((HasTravelFormation(adapter)), "Complete travel capability unavailable");
        adapter.size = offsetof(Adapter, setFormationPackage);
        REQUIRE_MESSAGE((!HasTravelFormation(adapter)), "Partial travel extension accepted");
        adapter.size = offsetof(Adapter, canTravelInFormation);
        REQUIRE_MESSAGE((!HasTravelFormation(adapter) && HasCombatProtection(adapter)),
                        "Protection-only table exposes travel callbacks");
        REQUIRE_MESSAGE((Valid(adapter)), "Protection-only v1 table rejected after travel extension");
        adapter.size = sizeof(Adapter);
        adapter.setFormationPackage = nullptr;
        REQUIRE_MESSAGE((!HasTravelFormation(adapter)), "Permission without movement callback accepted");
        adapter = ValidAdapter();
        REQUIRE_MESSAGE(
            (HasFollowDistance(adapter) && HasRecruitmentEligibility(adapter) && HasCombatProtection(adapter)),
            "Optional callbacks unavailable");

        adapter.size = offsetof(Adapter, canReceiveCombatProtection);
        REQUIRE_MESSAGE(
            (HasFollowDistance(adapter) && HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter)),
            "Recruitment v1 table exposes a missing protection tail");
        REQUIRE_MESSAGE((Valid(adapter)), "Recruitment v1 table rejected after extension");

        adapter.size = offsetof(Adapter, canRecruitThroughDialogue);
        REQUIRE_MESSAGE(
            (HasFollowDistance(adapter) && !HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter)),
            "Distance-only v1 table lost its callback or exposes a missing recruitment tail");
        REQUIRE_MESSAGE((Valid(adapter)), "Valid version-one adapter rejected");

        adapter.size = BaseAdapterSize;
        REQUIRE_MESSAGE(
            (!HasFollowDistance(adapter) && !HasRecruitmentEligibility(adapter) && !HasCombatProtection(adapter)),
            "Original v1 table exposes optional callbacks");
        REQUIRE_MESSAGE((Valid(adapter)), "Original v1 adapter rejected after extension");

        adapter.size = sizeof(Adapter);
        adapter.version = 2;
        REQUIRE_MESSAGE((!Valid(adapter)), "Unsupported ABI accepted");

        adapter.version = 1;
        adapter.size = 8;
        REQUIRE_MESSAGE((!Valid(adapter)), "Truncated adapter table accepted");
    }

    TEST_CASE("Invalid Capability Bits") {
        FollowerState following;
        following.state = State::Following;
        following.commands = AllCommands;
        following.commands |= 128;
        REQUIRE_MESSAGE((!Supports(following, Command::Follow)), "Invalid capability bits accepted");
    }

    TEST_CASE("Combat Protection Ownership") {
        using mod::adapter_rules::CombatProtection;
        REQUIRE_MESSAGE((CombatProtection(true, State::Following, true, false, false) &&
                         CombatProtection(true, State::Waiting, true, false, false)),
                        "Active eligible followers do not receive protection");
        REQUIRE_MESSAGE((!CombatProtection(false, State::Following, true, false, false)),
                        "Disabled protection remains active");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Inactive, true, false, false) &&
                         !CombatProtection(true, State::Unavailable, true, false, false)),
                        "Dismissed or conflicting followers retain protection");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Following, false, false, false)), "Provider opt-out ignored");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Following, true, true, false)),
                        "Dead actor receives protection");
        REQUIRE_MESSAGE((!CombatProtection(true, State::Following, true, false, true)),
                        "Adapter changes native-owned protection");
    }

    TEST_CASE("Request Ownership And Quarantine") {
        Requests requests;
        constexpr AdapterID owner = 1;
        constexpr AdapterID otherAdapter = 2;
        constexpr ActorID actor = 100;

        const auto request = requests.Add(owner, actor, Command::Wait);
        REQUIRE_MESSAGE((request.has_value() && !requests.Add(otherAdapter, actor, Command::Dismiss)),
                        "Duplicate controller command accepted");
        REQUIRE_MESSAGE((!requests.Find(otherAdapter, *request)), "Another adapter can acknowledge a request");

        auto* pending = requests.Find(owner, *request);
        pending->started = true;
        pending->quarantined = true;
        REQUIRE_MESSAGE((!requests.Add(owner, actor, Command::Wait)), "Timed-out call can be replayed");
        REQUIRE_MESSAGE((requests.Remove(owner, *request) && requests.Add(owner, actor, Command::Follow).has_value()),
                        "Late acknowledgement cannot release quarantine");

        requests.Reset();
        const auto afterLoad = requests.Add(owner, actor, Command::Dismiss);
        REQUIRE_MESSAGE((afterLoad && *afterLoad > *request && !requests.Find(owner, *request)),
                        "Old-save acknowledgement can finish a new request");

        for (ActorID nextActor = actor + 1; nextActor < actor + 16; ++nextActor)
            REQUIRE_MESSAGE((requests.Add(owner, nextActor, Command::Wait).has_value()), "Queue filled prematurely");
        REQUIRE_MESSAGE((!requests.Add(owner, actor + 16, Command::Wait)), "Adapter queue limit ignored");
    }
}
