# Architecture changes and rationale

YLIWF is a substantial architectural rewrite of SFF, retaining much of its
gameplay and record design. The goal is to understand and control the mod's
state and make its implementation easier to build, review, and maintain.

This document explains design choices. Implementation contracts are in
[native-controller.md](native-controller.md); operational instructions are in
the [build](build-release.md) and [debugging](debugging.md) guides.

## Maintain the mod as source

| Input | Role |
| --- | --- |
| `src/native/`, `include/` | Native behavior and Skyrim integration |
| `src/papyrus/` | Compatibility interfaces and engine adapters |
| `src/plugin/` | C# definitions that generate the ESP through Mutagen |
| `assets/settings.ini` | Default configuration |
| `src/shared/`, `mod.json` | Shared identity and installed filenames |
| `tools/`, `cmake/` | Setup, compilation, validation, and packaging |
| `tests/` | Native and tooling regression checks |

The ESP is generated from reviewable C# definitions. Aliases, packages, dialogue
conditions, and script bindings can therefore be versioned alongside their
implementations. Creation Kit and xEdit remain useful for inspection.

PowerShell entrypoints and one C# tooling project handle the build. Dependency
pins and lock files identify its inputs; external SDK requirements remain
documented in the build guide. Packaging captures matching sources and notices
so a release can be traced to its inputs.

## Move decisions into native code

C++ owns recruitment policy, slot selection, promotion, cleanup, operation
planning, execution, dismissal delays, and waiting-follower deadlines.
`DialogueFollowerScript` contains only the eight vanilla forwarding methods.
Typed C++ state and SKSE serialization replace the Papyrus state container. Native listeners handle activation, death, combat, and unloading.
Small `YLIWF_Engine` adapters invoke vanilla APIs without dependable CommonLib
equivalents and acknowledge completion from their saved VM stacks.
This moves repeated queries and decisions out of the VM while preserving the
interfaces used by dialogue, quests, and compatibility scripts.

Further native ownership is an experiment, not an end in itself. Changes need
clear state ownership, completion semantics, and save/load verification.
Performance gains require in-game profiling.

## Distinguish acceptance from completion

Requests share one active executor. Idle main-thread requests start immediately;
others use a bounded FIFO, preserving order and preventing reentrant execution.
Named command and step structures replace runtime packing; the save codec keeps
the existing format. Caller identity is published before dispatch, steps
acknowledge progress, and completion checks the resulting engine state.

Saved ownership prevents overlapping execution or replay on a timeout.
Verification covers observable state, not eventual movement or dialogue playback.
Native latent callers store their stack and function/object identity with the
co-save state. They resume after verification against the loaded VM. Deadline scheduling
sleeps when no timer needs service and touches engine state only on the main thread.

## Keep one persistent follower registry

`DialogueFollower` aliases remain authoritative. Alias changes mark their owning
quest's runtime data for saving; pending work uses native structures and retained
handles serialized through SKSE. This avoids maintaining a second roster that must be reconstructed
after loading.

The save investigation showed why runtime correctness alone is insufficient:
registrations must also be recorded by the engine. Explicit binding repair
validates the loaded quest before reconnecting missing properties.

## Make diagnosis separate from gameplay

The SKSE menu exposes registrations and test commands. Debug options enable
repairs and detailed state; flow logging has its own switch. Snapshot refreshes
run only while a diagnostic page is visible, while gameplay events and queued
operations continue independently.

These tools grew out of investigating a broken save. They provide evidence and
targeted recovery rather than requiring the menu for ordinary follower management.
