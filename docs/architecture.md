# Architecture

YLIWF rewrites SFF's implementation while retaining much of its gameplay and
record design. The goals are explicit state ownership, easier diagnosis and
reproducible source builds.

## Source layout

| Path | Responsibility |
| --- | --- |
| `src/native/`, `include/` | Follower behavior and Skyrim integration |
| `src/papyrus/` | Vanilla compatibility and engine-call adapters |
| `src/plugin/` | Mutagen C# definitions generating the ESP |
| `src/addons/` | Optional follower adapters |
| `assets/settings.ini` | Default settings |
| `src/shared/`, `mod.json` | Shared identity and filenames |
| `tools/`, `cmake/` | Setup, builds, validation and packaging |
| `tests/` | Native and tooling checks |

ESP records are generated from C# so aliases, packages, dialogue and bindings can
be reviewed and versioned with their implementations. Creation Kit and xEdit
remain useful for inspection. Dependency pins and release source archives identify
the inputs used to build each release.

## Native ownership

C++ owns follower policy, commands, timers and controller persistence.
`DialogueFollowerScript` retains eight vanilla forwarding methods.
Small `YLIWF_Engine` adapters perform engine calls without dependable native
equivalents and acknowledge completion.

This reduces VM work while preserving interfaces used by quests and dialogue.
Further offloading requires clear ownership, completion checks and save/load
testing; performance gains require in-game measurement.

## Completion and persistence

One executor orders requests and verifies their resulting state. Acceptance does
not mean completion, and a timeout does not authorize replay.

`DialogueFollower` aliases remain the roster. Alias writes mark the quest for
saving; the SKSE co-save stores pending work and timers. The save investigation
showed that correct runtime state must also be recorded by the engine.

See [controller contracts](native-controller.md) for queue and save details.

## Diagnostics

The menu exposes registrations and repair tools without being required for normal
play. Debug controls and flow logging are separate. Snapshots refresh only while
Followers or Debug is visible; gameplay runs independently.

See [debugging](debugging.md) and the [build guide](build-release.md).
