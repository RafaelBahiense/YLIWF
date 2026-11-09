# Architecture

YLIWF rewrites SFF's implementation while retaining much of its gameplay and
record design. The goals are explicit state ownership, easier diagnosis and
reproducible source builds.

## Source layout

| Path | Responsibility |
| --- | --- |
| `src/native/` | Native implementation and private headers |
| `include/` | Public add-on API headers |
| `src/papyrus/` | Vanilla compatibility and engine-call adapters |
| `src/plugin/` | Mutagen C# definitions generating the ESP |
| `src/addons/` | Optional adapters and development modules |
| `assets/settings.ini` | Default settings |
| `src/shared/`, `mod.json` | Shared identity and filenames |
| `tools/`, `cmake/` | Setup, builds, validation and packaging |
| `tests/` | Native and tooling checks |

Native sources and headers are grouped by subsystem: `followers`, `combat`,
`formations`, `adapters`, `diagnostics`, `input`, `ui`, `settings` and `runtime`.
Shared helpers live in `src/native/common`; the PCH lives in `src/native/PCH.h`.
Private headers sit beside their implementations; only public add-on
headers belong in `include/`.

## Gameplay rules and engine access

Rule headers use ordinary C++ values and do not depend on Skyrim, SKSE or the PCH.
Engine code reads a snapshot, passes it to the rules, then applies the result on
the game thread. Tests supply snapshots directly.

For healing, `combat/HealingSpells` reads costs, effects and engine cast checks.
`combat/HealingSelection.h` decides which spell and target to use.
`combat/HealingForecast.h` estimates restoration through named casting, healing
and regeneration steps. `combat/Healer.cpp` coordinates execution and receipts.

`combat/DefenderObservation` reads bounded combat-group threats on the game thread.
`DefenderTactics.h` selects approach, guard, interrupt, recovery, counter or
repositioning from ordinary snapshots. `ShieldDefender` coordinates each follower's
style and movement; the combat worker hook reconciles style ownership against a
fresh published decision. Ordinary attacks and combos remain with combat AI.
Recovery takes priority over pursuit and counterattacks. Without an imminent
attack it creates breathing room, then stops retreating and lowers the shield;
committed attacks retain their defensive response. Tactical bashes preserve the
recovery state, stamina reserve and cooldown. Power bash requires heavy melee
preparation and live engine eligibility, with normal bash as the fallback.
Recovery permission blocks new offensive selections through external attack
handoffs while preserving the current action. Safe gaps reject discretionary
shield raises. Neither filter cancels an accepted action or invents completion.

`combat/actions/Taunt` coordinates a targeted challenge using effect application
and the observed combat target as acknowledgement. `TauntRules.h` owns eligibility,
duration, cooldown and failure rules. The isolated `integrations/TargetPreference`
backend chains the engine's target eligibility check and consumes an expiring
immutable snapshot. It never makes an otherwise invalid target eligible or writes
combat target fields directly. Ending ownership clears the preference.

`DefenderMovement` owns eight independent marker/package slots. It checks loaded
navmesh, direct clearance and other threats before applying a bounded movement
intent. Skyrim performs route finding. Failed movement yields to ordinary combat
AI; neither navmesh projection nor a selected package proves arrival. Movement
releases on restrictions, role changes and loss of combat eligibility.
Movement destinations use the engine's combat HoldPosition procedure. Approach
requests start beyond 256 units and release within 192 to avoid threshold churn.
Actor displacement drives progress checks. Retreat and lateral refreshes revalidate
an unfinished waypoint rather than continuously moving it ahead of the actor. Recovery spacing
has a release margin to reduce acquisition churn. Stalled destinations are
temporarily excluded; bounded lateral waypoints can open a route around an obstruction, and
repeated failure yields to ordinary AI. Body-width rays at two heights screen
clearance without claiming a capsule sweep. Guard preparation and observed guards
use walking packages. Acquisition checks actual displacement speed, sprint state
and weapon readiness; releasing a package does not prove that the actor stopped.
Hostile spells with a live current spell in startup/ready/charging states count
as preparation. Idle equipped spells and unknown/interrupt states do not establish
an imminent cast, and release does not establish projectile-flight timing.

`include/DefenderTuning.h` defines the sized test-profile transport and validation rules.
The main-thread `combat/DefenderTuning.h` scope binds one immutable profile to an actor/run token
before combat. Movement, recovery and guard eligibility read that scope; other
actors use the default parameters. Teardown and game-state invalidation release
the scope. Test reports capture the applied profile independently of later edits.

`combat/actions/Dispatch` owns experimental native action dispatch, resolved
intent validation and dispatch receipts for guard, release and bashes. Its hooks
and external-attack reservation mailbox support one actor scope; engine dispatch
runs on the main thread. `GuardExecution`, `BashExecution` and `RightAttackExecution` own acknowledgement,
completion, interruption cleanup and unresolved ownership. Their state machines
consume ordinary snapshots and a dispatch callback, so tests can exercise them
without Skyrim. `Samples` reads the engine state; graph callbacks touch only the
keyed animation mailbox. `DefenderActions` supplies tactical intent, permission and
stamina policy, and maps transitions into diagnostic reports. Dispatch acceptance
does not establish animation start or completion. The diagnostic host also accepts
explicit primitive requests with role tactics bypassed; live actor permission and
ownership checks still apply.

ESP records are generated from C# so aliases, packages, dialogue and bindings can
be reviewed and versioned with their implementations. Creation Kit and xEdit
support inspection. Dependency pins and release source archives identify
the inputs used to build each release.

## Native ownership

C++ owns follower policy, commands, timers and controller persistence.
`DialogueFollowerScript` provides eight vanilla forwarding methods.
Small `YLIWF_Engine` adapters perform engine calls without dependable native
equivalents and acknowledge completion.

Quest and dialogue interfaces remain compatible. Completion, save/load behavior
and performance require in-game testing.

## Completion and persistence

One executor orders requests and verifies their resulting state. Acceptance does
not mean completion, and a timeout does not authorize replay.

`DialogueFollower` aliases are the roster. Alias writes mark the quest for
saving so the engine persists registrations. The SKSE co-save stores pending
work and timers.

See [controller contracts](native-controller.md) for queue and save details.

## Diagnostics

The menu exposes registrations and repair tools without being required for normal
play. Debug controls and flow logging are separate. Snapshots refresh only while
Followers, Debug or an add-on tab is visible; gameplay runs independently.

Add-ons own their menu pages and register through the public
[menu-tab API](addons/menu-tabs.md). The core supplies registration and cached roster
data without depending on an add-on's renderer or runner.

See [debugging](debugging.md) and the [build guide](build-release.md).
