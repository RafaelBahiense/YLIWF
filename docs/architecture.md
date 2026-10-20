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
| `src/addons/` | Optional follower adapters |
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
Followers or Debug is visible; gameplay runs independently.

See [debugging](debugging.md) and the [build guide](build-release.md).
