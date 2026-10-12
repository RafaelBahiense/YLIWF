#include "FormationRules.h"
#include "FormationPath.h"
#include "FormationGeometry.h"
#include "FormationProgress.h"
#include "FormationGroundRules.h"
#include "FormationLocationRules.h"
#include <cstdlib>
#include <iostream>

namespace {
    using namespace mod::formation_rules;

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

    void CheckSavedRanks() {
        for (int slot = 0; slot < Slots; ++slot) {
            Check(Slot(Rank(slot, false)) == slot && Slot(Rank(slot, true)) == slot,
                  "Ownership changes the saved slot");
            Check(!OwnsOffset(Rank(slot, false)) && OwnsOffset(Rank(slot, true)),
                  "Recovery cannot distinguish owned offsets");
        }
        Check(!ValidRank(-1) && !ValidRank(32) && Slot(-1) == -1, "Invalid saved rank accepted");
        Check(Active(16) && Slot(23) == 7 && !OwnsOffset(16), "Travel gates confused with old offsets");
    }

    void CheckStaleAndCancelledRequests() {
        const RequestIdentity current{0xABCDEF0012345678, 0x1234, 7};
        Check(CanExecute(current, current, true, true), "Current permitted request rejected");
        Check(!CanExecute(current, {0x1111111122222222, current.actor, current.ticket}, true, true),
              "A saved stack from another Skyrim launch matched a reused ticket");
        Check(!CanExecute(current, {current.session, current.actor, 6}, true, true), "Old save-load ticket accepted");
        Check(!CanExecute(current, {current.session, 0x5678, current.ticket}, true, true),
              "Request changed another actor");
        Check(!CanExecute(current, current, false, true) && !CanExecute(current, current, true, false),
              "Loading or revoked eligibility allowed an engine call");
    }

    void CheckStableSlots() {
        std::array<int, Slots> preferences{3, 0, -1, 5, -1, -1, -1, -1};
        const auto assigned = AssignSlots(preferences, 4);
        Check(assigned[0] == 3 && assigned[1] == 0 && assigned[3] == 5, "Existing followers reshuffled");
        Check(assigned[2] == 1 && assigned[4] == -1, "New follower did not use an unoccupied slot");
    }

    void CheckConflictingSlots() {
        std::array<int, Slots> preferences{2, 2, -1, 15, -1, -1, -1, -1};
        const auto assigned = AssignSlots(preferences, Slots);
        std::array<bool, Slots> seen{};
        for (const auto slot : assigned) {
            Check(slot >= 0 && slot < Slots && !seen.at(static_cast<std::size_t>(slot)),
                  "Duplicate or invalid formation slot");
            seen.at(static_cast<std::size_t>(slot)) = true;
        }
        Check(assigned[0] == 2, "Valid existing slot lost during conflict resolution");
    }

    void CheckIndependentSpacing() {
        std::array<int, Slots> presets{};
        presets.fill(1);
        Check(mod::formation_geometry::Column(presets)[0].behind == 256 &&
                  mod::formation_geometry::Column(presets)[2].behind == 768,
              "Normal column spacing incorrect");
        presets[1] = 2;
        Check(mod::formation_geometry::Column(presets)[0].behind == 256 &&
                  mod::formation_geometry::Column(presets)[1].behind == 768 &&
                  mod::formation_geometry::Column(presets)[2].behind == 1024,
              "A farther follower overlaps the next slot or changes earlier slots");
        presets[0] = 0;
        Check(mod::formation_geometry::Column(presets)[0].behind == 128, "Individual close spacing ignored");
    }

    void CheckMovementPath() {
        Path path;
        path.Observe({0, 0, 0});
        Check(!path.Behind(128), "Formation extrapolated into unvisited terrain");
        path.Observe({0, 256, 64});
        const auto straight = path.Behind(128);
        Check(straight && straight->x == 0 && straight->y == 128 && straight->z == 32,
              "Route interpolation or height incorrect");
        path.Observe({256, 256, 64});
        const auto corner = path.Behind(384);
        Check(corner && corner->x == 0 && corner->y == 128, "Column cut across a corner");
        path.Observe({257, 257, 64});
        Check(path.Behind(384)->y == 128, "Idle jitter moved the formation");
        path.Observe({4096, 4096, 64});
        Check(!path.Behind(128), "Teleport left a route through the map");
        path.Clear();
        Check(!path.Behind(0), "Load reset retained a previous route");
    }

    void CheckTravelRefresh() {
        for (int slot = 0; slot < Slots; ++slot) {
            const auto first = NextTravelRank(slot, Rank(slot, false));
            const auto second = NextTravelRank(slot, first);
            Check(first != second && Active(first) && Active(second), "Target refresh re-selected the same package");
            Check(Slot(first) == slot && Slot(second) == slot && !OwnsOffset(second),
                  "Travel refresh changed a follower's slot or requested offset cleanup");
            Check(NextTravelRank(slot, second) == first, "Travel variants did not cycle");
            Check(NextTravelRank(slot, -1) == first && NextTravelRank(slot, Rank(slot, true)) == first,
                  "Inactive or saved-offset state did not start the first Travel variant");
        }
    }

    void CheckGeometryProjection() {
        Path route;
        route.Observe({0, 0, 0});
        Check(route.Behind(0).has_value(), "A zero-offset layout requires unnecessary route history");
        route.Observe({0, 512, 128});
        std::array<int, Slots> presets{};
        presets.fill(1);
        auto destinations = mod::formation_geometry::Project(route, mod::formation_geometry::Column(presets));
        Check(destinations[0] && destinations[0]->y == 256 && destinations[0]->z == 64,
              "Geometry did not project column spacing onto the recorded route");
        Check(destinations[1] && destinations[1]->y == 0 && !destinations[2],
              "Geometry extrapolated slots beyond recorded terrain");
        mod::formation_geometry::Layout layout{};
        layout[0].behind = 128;
        destinations = mod::formation_geometry::Project(route, layout);
        Check(destinations[0] && destinations[0]->y == 384, "Route projection depends on the column layout");
    }

    bool Observe(Progress& progress, Point actor, Point target, float elapsed) {
        return progress.Observe(actor, target, target, elapsed);
    }

    void CheckArrivalAndSteadyMotion() {
        Progress arrived;
        for (int second = 0; second < 30; ++second)
            Check(Observe(arrived, {0, 0, 0}, {64, 0, 0}, 1), "Arrived follower was treated as stuck");
        Progress walking;
        for (int second = 0; second < 30; ++second) {
            const float x = static_cast<float>(second) * 32;
            Check(Observe(walking, {x, 0, 0}, {x + 512, 0, 0}, 1), "Walking follower was treated as stuck");
        }
    }

    void CheckStallRecovery() {
        Progress progress;
        Check(Observe(progress, {0, 0, 0}, {512, 0, 0}, 1), "First movement observation failed");
        for (int second = 0; second < 5; ++second)
            Check(Observe(progress, {2, 2, 0}, {512, 0, 0}, 1), "Fallback fired before the stall threshold");
        Check(!Observe(progress, {2, 2, 0}, {512, 0, 0}, 1) && progress.Reason() == RecoveryReason::Stalled,
              "Stationary follower did not fall back to ordinary following");
        for (int second = 0; second < 9; ++second)
            Check(!Observe(progress, {256, 0, 0}, {512, 0, 0}, 1), "Recovery rapidly re-entered formation");
        Check(Observe(progress, {256, 0, 0}, {512, 0, 0}, 1) && progress.Reason() == RecoveryReason::None,
              "Caught-up follower could not rejoin after cooldown");
    }

    void CheckDistantFollowerRecovery() {
        Progress progress;
        Check(!Observe(progress, {0, 0, 0}, {4096, 0, 0}, 1) && progress.Reason() == RecoveryReason::TooFar,
              "Formation kept control of a follower too far behind");
        for (int second = 0; second < 20; ++second)
            Check(!Observe(progress, {0, 0, 0}, {2048, 0, 0}, 1),
                  "Far-away follower rejoined as soon as cooldown expired");
        Check(Observe(progress, {0, 0, 0}, {512, 0, 0}, 1),
              "Ordinary following could not return control after catching up");
    }

    void CheckSuspensionAndHitches() {
        Progress progress;
        Check(Observe(progress, {0, 0, 0}, {512, 0, 0}, 1), "Observation setup failed");
        Check(Observe(progress, {0, 0, 0}, {512, 0, 0}, 60), "A single loading hitch triggered fallback");
        progress.Suspend();
        Check(Observe(progress, {0, 0, 0}, {512, 0, 0}, 1) && progress.StalledSeconds() == 0,
              "Combat or scene suspension retained a partial stall measurement");
        Check(!Observe(progress, {0, 0, 0}, {4096, 0, 0}, 1), "Recovery setup failed");
        progress.Suspend();
        Check(progress.RetryAfter() == Progress::RetrySeconds && progress.Reason() == RecoveryReason::TooFar,
              "Suspension erased fallback protection");
        progress = Progress{};
        Check(progress.Reason() == RecoveryReason::None && progress.RetryAfter() == 0,
              "Save-load reset retained transient recovery state");
    }

    void CheckIndependentRecovery() {
        Progress blocked, moving;
        Check(!Observe(blocked, {0, 0, 0}, {4096, 0, 0}, 1), "Blocked follower did not recover");
        Check(Observe(moving, {0, 0, 0}, {512, 0, 0}, 1) && moving.Reason() == RecoveryReason::None,
              "One follower's fallback suspended another follower");
    }

    void CheckLongColumnRecovery() {
        Progress progress;
        Check(progress.Observe({0, 0, 0}, {-4096, 0, 0}, {0, 0, 0}, 1),
              "A rear slot was mistaken for a follower lagging behind the party");
        Check(!progress.Observe({4096, 0, 0}, {-4096, 0, 0}, {0, 0, 0}, 1),
              "Follower far from both leader and slot did not recover");
        for (int second = 0; second < 9; ++second)
            Check(!progress.Observe({0, 0, 0}, {-4096, 0, 0}, {0, 0, 0}, 1), "Near-player recovery ignored cooldown");
        Check(progress.Observe({0, 0, 0}, {-4096, 0, 0}, {0, 0, 0}, 1),
              "A recovered follower near the player could not rejoin a distant rear slot");
    }

    void CheckLineAndWedgeLayouts() {
        using mod::formation_geometry::Arrange;
        using mod::formation_geometry::Style;
        std::array<int, Slots> presets{};
        presets.fill(1);
        const auto line = Arrange(Style::Line, presets);
        Check(line[0].behind == 0 && line[0].side == -256 && line[1].side == 256 && line[2].side == -512 &&
                  line[3].side == 512 && line[3].behind == 0,
              "Line does not reserve its center for the player");
        const auto wedge = Arrange(Style::Wedge, presets);
        Check(wedge[0].behind == 256 && wedge[1].behind == 256 && wedge[0].side == -256 && wedge[1].side == 256 &&
                  wedge[2].behind == 512 && wedge[2].side == -512,
              "Wedge does not reserve its tip for the player");
        for (const auto offset : wedge)
            Check(offset.behind > 0 && offset.side != 0, "A follower occupies the wedge's player position");
        presets[1] = 2;
        const auto wider = Arrange(Style::Line, presets);
        Check(wider[1].side == 512 && wider[3].side == 1024 && wider[2].side == -512,
              "Individual spacing overlaps later followers or changes the other side");
    }

    void CheckMovementOrientation() {
        using mod::formation_geometry::Arrange;
        using mod::formation_geometry::Project;
        using mod::formation_geometry::Style;
        Path route;
        route.Observe({0, 0, 0});
        Check(!route.Heading(), "Facing was invented before any movement");
        route.Observe({0, 512, 0});
        std::array<int, Slots> presets{};
        presets.fill(1);
        const auto north = Project(route, Arrange(Style::Line, presets));
        Check(north[0] && north[1] && north[0]->x == -256 && north[1]->x == 256 && north[1]->y == 512,
              "Movement heading projects the line onto the wrong side");
        route.Observe({1, 513, 0});
        const auto still = Project(route, Arrange(Style::Line, presets));
        Check(still[1]->x == north[1]->x && still[1]->y == north[1]->y, "Idle jitter rotated or moved the line");
        route.Observe({32, 512, 0});
        Check(route.Heading()->x > 0 && route.Heading()->x < 0.5f && route.Heading()->y > 0.8f,
              "A short turn snapped the formation heading instead of smoothing it");
        route.Clear();
        route.Observe({0, 0, 0});
        route.Observe({0, 512, 0});
        for (int step = 1; step <= 24; ++step)
            route.Observe({0, 512 - static_cast<float>(step) * 32, 0});
        Check(route.Heading()->y < -0.9f, "An exact movement reversal never changed the heading");
        route.Observe({4096, 4096, 0});
        Check(!route.Heading(), "Teleport retained the previous movement heading");
    }
}

namespace {
    void CheckGroundProjection() {
        using namespace mod::formation_ground;
        const Triangle floor{{Point{-500, -500, 20}, Point{500, -500, 20}, Point{0, 500, 20}}};
        const std::array surfaces{floor};
        const auto grounded = Project({0, 0, 100}, surfaces);
        Check(grounded && grounded->x == 0 && grounded->y == 0 && grounded->z == 20,
              "Ground projection did not preserve the slot XY or use the surface height");
        const Triangle slope{{Point{0, 0, 0}, Point{200, 0, 100}, Point{0, 200, 200}}};
        const auto height = OnTriangle({50, 50, 0}, slope);
        Check(height && height->z == 75, "Sloped ground height was not interpolated");
        const auto edge = OnTriangle({-10, 50, 0}, slope);
        Check(edge && edge->x > 0 && edge->y > 0 && edge->x + edge->y < 200,
              "Adjusted point remained outside or exactly on the navmesh boundary");
        const auto inside = OnTriangle(*edge, slope);
        Check(inside && SquaredDistance(*inside, *edge) < 0.01f, "Edge inset left the ground plane");
    }

    void CheckGroundSearchLimits() {
        using namespace mod::formation_ground;
        const Triangle floor{{Point{-500, -500, 0}, Point{500, -500, 0}, Point{0, 500, 0}}};
        const std::array surfaces{floor};
        Check(!Project({2000, 0, 0}, surfaces), "Distant navmesh pulled the formation away from its slot");
        Check(!Project({0, 0, 300}, surfaces), "Ground projection jumped to a different floor beyond its height limit");
        Check(!Project({0, 0, 0}, {}), "Missing loaded navmesh produced a destination");
        const Triangle line{{Point{0, 0, 0}, Point{0, 1, 0}, Point{0, 2, 0}}};
        Check(!OnTriangle({0, 0, 0}, line), "Degenerate triangle was treated as ground");
        Check(!Project({std::numeric_limits<float>::quiet_NaN(), 0, 0}, surfaces),
              "Invalid coordinates produced a movement target");
    }

    void CheckStableGroundTargets() {
        using namespace mod::formation_ground;
        const Triangle floor{{Point{-500, -500, 0}, Point{500, -500, 0}, Point{0, 500, 0}}};
        const std::array surfaces{floor};
        const auto held = Project({2, 0, 0}, surfaces, {}, Point{0, 0, 0});
        Check(held && held->x == 0, "Small goal jitter moved a valid grounded target");
        const auto moved = Project({64, 0, 0}, surfaces, {}, Point{0, 0, 0});
        Check(moved && moved->x == 64, "Meaningful movement retained a stale target");
        const Triangle raised{{Point{-500, -500, 20}, Point{500, -500, 20}, Point{0, 500, 20}}};
        const std::array changed{raised};
        const auto revalidated = Project({0, 0, 0}, changed, {}, Point{0, 0, 0});
        Check(revalidated && revalidated->z == 20, "Cached target survived a changed ground surface");
    }

    void CheckGroundSlotSeparation() {
        using namespace mod::formation_ground;
        const Triangle floor{{Point{-500, -500, 0}, Point{500, -500, 0}, Point{0, 500, 0}}};
        const Triangle alternative{{Point{150, -100, 0}, Point{250, -100, 0}, Point{150, 100, 0}}};
        const std::array occupied{Point{0, 0, 0}};
        const std::array crowded{floor};
        Check(!Project({0, 0, 0}, crowded, occupied), "Two slots collapsed onto the same ground point");
        const std::array surfaces{floor, alternative};
        const auto separated = Project({0, 0, 0}, surfaces, occupied, Point{0, 0, 0});
        Check(separated && mod::formation_rules::Distance(*separated, occupied[0]) >= MinimumSeparation,
              "Projection ignored an available separated candidate or retained a crowded cached target");
    }

    void CheckPlayerGroundAnchor() {
        using namespace mod::formation_ground;
        Check(SuitableAnchor({0, 0, 80}, {0, 0, 0}), "Ordinary player ground offset rejected");
        Check(!SuitableAnchor({0, 0, 200}, {0, 0, 0}), "A high ledge advanced the walking route through empty air");
        Check(!SuitableAnchor({0, 0, 0}, {100, 0, 0}), "Distant ground was treated as the player's walking route");
    }
}

namespace {
    void CheckSettlementBoundaries() {
        struct Location {
            Location* parentLoc = nullptr;
            bool settlement = false;
        };

        const auto matches = [](const Location* location) { return location->settlement; };
        Location hold;
        Location wilderness{&hold};
        Location town{&hold, true};
        Location courtyard{&town};
        Location nested{&courtyard};
        Check(!InSettlement(static_cast<Location*>(nullptr), matches), "Missing location blocks wilderness travel");
        Check(!InSettlement(&wilderness, matches), "A hold was incorrectly treated as a settlement");
        Check(InSettlement(&town, matches), "Town allows formation movement");
        Check(InSettlement(&courtyard, matches) && InSettlement(&nested, matches),
              "Nested settlement locations allow formation movement");
        courtyard.parentLoc = &hold;
        Check(!InSettlement(&courtyard, matches), "Leaving the settlement retains its restriction");
        Location cycle;
        cycle.parentLoc = &cycle;
        Check(InSettlement(&cycle, matches), "Malformed ancestry enables formations instead of failing closed");
    }
}

int main() {
    CheckSettlementBoundaries();
    CheckGroundProjection();
    CheckGroundSearchLimits();
    CheckStableGroundTargets();
    CheckGroundSlotSeparation();
    CheckPlayerGroundAnchor();
    CheckSavedRanks();
    CheckStaleAndCancelledRequests();
    CheckStableSlots();
    CheckConflictingSlots();
    CheckIndependentSpacing();
    CheckMovementPath();
    CheckTravelRefresh();
    CheckGeometryProjection();
    CheckArrivalAndSteadyMotion();
    CheckStallRecovery();
    CheckDistantFollowerRecovery();
    CheckSuspensionAndHitches();
    CheckIndependentRecovery();
    CheckLongColumnRecovery();
    CheckLineAndWedgeLayouts();
    CheckMovementOrientation();
    std::cout << "Formation slot, recovery and spacing tests passed.\n";
}
