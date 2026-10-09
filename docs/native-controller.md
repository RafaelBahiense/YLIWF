# Native controller

See [architecture](architecture.md) for rationale. This document defines execution
and persistence contracts.

## Ownership

| Responsibility | Implementation |
| --- | --- |
| Policy and plans | `ControllerRules.h` |
| Admission, queue, bindings and verification | `Controller.cpp` |
| Effects, scheduling and continuations | `ControllerExecutor.cpp` |
| Native saves | `ControllerState.h`, `StateCodec.h`, `ControllerStorage.cpp` |
| Eight vanilla forwarding methods | `DialogueFollowerScript.psc` |
| Mechanical engine calls | `YLIWF_Engine.psc` |
| Quest-managed followers | [Add-on API](follower-adapters.md) |
| Packages, conditions and aliases | `src/plugin/` |

Plans run on the main thread. CommonLib calls are used where available;
Papyrus adapters handle remaining engine APIs. They validate the ticket before
acting and acknowledge afterward, without follower policy or their own queue.
`YLIWF_SKSE` exports only maintained script entrypoints and logging helpers.

## Registry and saved ABI

`DialogueFollower` aliases are the roster: primary 0, animal 1, extras 2–8.
Bindings validate owner, ID and case-insensitive name. Alias writes mark the
quest for saving; saved counts alone do not preserve occupants.

Recruitment preserves an occupied primary. Promotion assigns primary before
clearing extra. Manual primary selection exchanges actors, retaining their
waiting state and deadlines; captured occupants are validated before mutation.
Lowering the cap preserves followers and blocks recruitment.

The `CTRL` record under SKSE ID `YLIW` stores version-one native state:
active plan, queue, actor handles, progress, delays, deadlines, receipts and
latent callers. Explicit little-endian fields avoid pointers and padding.
Runtime structures use named fields; packing belongs only to the codec.

Operation/effect IDs and record layouts remain stable. Retired IDs are reserved;
effect 10 (`ReservedUpdate`) resumes as a no-op. Primary exchange references its
second actor through the existing actor pool. This format does not migrate
SFF/YLIF saves.

SKSE remaps retained handles on load. Missing handles for accepted work block
execution; an unavailable speaker hint can be cleared. Bounded invalid or
unsupported records remain blocked diagnostics when saved again. Recovery needs
the original save pair and matching build, not another save's load-order table.

Extra aliases and mod globals resolve from the canonical plugin.
There is no separately reconstructed follower roster.

## Requests and completion

An idle executor starts immediately on the unpaused main thread. Busy, paused,
off-thread and reentrant requests enter a 16-request FIFO. Full queues reject
new work; adjacent maintenance syncs coalesce. Follower commands preserve order.
Repeated UI tickets reuse their queued, active or recent results.

UI/events submit without waiting. Vanilla compatibility methods are native
latent calls returning after verified completion, so dependent quests can await
dismissal. Up to 32 callers retain stack, function, script and object identity.
Registration precedes execution; results return only after the original stack
suspends. Identity checks prevent resuming a reused stack, and pending results
outlive rolling receipt history.

Activation requests count refresh only. Explicit reconciliation also cleans
dead slots and promotes followers. Background success preserves command feedback.

Party commands atomically capture unique humanoid targets using selected alias
`-2`; plans resolve current slots after promotion. Dismissal can release dead
registrations. Feedback is transient; accepted native commands survive loading.

Every acknowledgement must match ticket and step offset. Completion verifies
aliases, teammate/faction/waiting state, counts, gates and dismissal results.
Counts publish after partial failure; the last 16 receipts retain outcomes.
Verification covers observable state, not eventual movement or dialogue playback.
Partial effects are not replayed automatically.

## Delays, timers, and loading

Dismissal saves its remaining two-second delay, excluding paused time.
Waiting deadlines are 72 game hours ahead; follow, cancel and clear remove them.
Timers enqueue ordinary commands after checking the saved deadline.

The scheduler sleeps when idle and posts main-thread work when needed.
Its thread never reads engine/VM state. Menu snapshots refresh independently,
only while visible.

Loading invalidates transient tasks and clocks, then resumes native state.
Accepted engine calls wait for their saved adapter stack; they are never
redispatched because of loading or VM pressure. Caller identities are checked
against the loaded VM. Returning to the main menu stops scheduling.

After 30 unpaused seconds without progress, diagnostics retain ownership and
report an unknown outcome. Timeout never permits replay or overlapping repair.
Invalid saved phases and caller tables remain blocked.

Install matching DLL/PEX files and upgrade while idle. Keep `.ess` and `.skse`
together. Papyrus queues and suspended old executor bytecode are not converted.
Saves without the native record start an empty queue; aliases still own the roster.
Saved command UI tickets are cleared; unaccepted tasks use generation guards.

Binding repair validates the canonical quest and repairs vanilla primary/count
properties without replacing registrations or deadlines.
See [repair instructions](debugging.md#repair-missing-bindings).

## Validation

`build.ps1 -Target Test` covers policy, queues, acknowledgements, ABI, save
round trips, invalid data, handle remapping and caller identity.
Engine stand-ins cannot prove VM scheduling or persistence.
Use the [in-game checklist](debugging.md#verification), including save/load
during native delays and pending adapter calls.
