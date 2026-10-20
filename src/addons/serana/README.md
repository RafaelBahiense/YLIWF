# Serana add-on

Makes Serana available to YLIWF features when her Dawnguard quests allow them.

## Features

| Feature | Support |
| --- | --- |
| Follow, Wait and Dismiss | Individual and party commands, including hotkeys |
| Following distance | Global and individual Close, Normal and Far settings |
| Travel formations | Column, Line and Wedge during ordinary travel |
| Stand down | Following and hostile avoidance without fighting |
| Friendly-fire and crossfire protection | Supported while eligible |
| Receive healing | From core healers, using spells that can affect her |
| Location, distance and state inspection | Supported |
| Recruitment alongside other followers | Through her own dialogue, respecting quest conditions |
| Combat-role assignment | Unsupported |
| Essential-status setting | Dawnguard retains control |
| YLIWF homes and idle sandboxing | Unsupported; her controller retains its behavior |
| Automatic dismissal after 72 hours waiting | Unsupported |
| Primary-slot promotion | Unsupported |
| Core recruitment limit | Does not consume a core slot |
| Controller repair | Diagnostics only; no automatic repair |

## Installation

Install `YouLeadIWillFollow-Serana-<version>.zip` alongside a compatible
[YLIWF core](../../../README.md). Recruit Serana through her own dialogue;
other recruited followers do not block recruitment when her quest permits it.

Dawnguard retains her recruitment, aliases, saved state and quest behavior.
Controller mismatches disable commands and appear in diagnostics.

## Implementation

[Serana.cpp](native/Serana.cpp) delegates commands to `DLC1_NPCMentalModelScript`.
The [add-on API](../../../docs/follower-adapters.md) defines the host contracts.

| Command | Controller method | Requirements |
| --- | --- | --- |
| Follow | `StopWaiting()` | Active, `CanFollow`, not `LockedIn` |
| Wait | `Wait()` | Also `IsWillingToWait` |
| Dismiss | `Dismiss()` | Also `CanBeDismissed` |

VM completion schedules a main-thread state check before acknowledgement.
Quest, location, combat and death events refresh host state; periodic observation
catches other changes. Events do not establish command completion.

Recruitment eligibility requires dismissal, `CanFollow`, no `LockedIn` or
`TurnOffComeWithMe`, and compatible live bindings. Only the vanilla follower-count
gate is relaxed; her remaining dialogue conditions still apply.

Distance sets exactly one of `FollowDistanceClose`, `FollowDistanceMedium` or
`FollowDistanceFar`. AI reevaluates only on change; blocked controllers wait until
eligible. Combat protection requires following or waiting, `CanFollow` and no
`LockedIn` restriction.

[TravelFormation.cpp](native/TravelFormation.cpp) applies temporary host packages
during ordinary following or sandboxing, without overriding Dawnguard records,
scripts or aliases. Quest packages, foreign interrupts, scenes, waiting and combat
take priority over formations. Stand down permits existing combat but retains
the quest and scene restrictions. Controller commands release these movement
overrides before dispatch.

Replacement scripts must preserve the controller's behavior and interface.
