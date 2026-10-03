# Native gameplay controller

Design rationale is in [architecture.md](architecture.md). This document records
execution and persistence contracts.

## Ownership

| Responsibility | Implementation |
| --- | --- |
| Decisions and ordered plans | `include/ControllerRules.h` |
| Bindings, request admission, queue, and verification | `src/native/Controller.cpp` |
| Effects, scheduling, continuations, and repair | `src/native/ControllerExecutor.cpp` |
| Eight vanilla compatibility methods | `DialogueFollowerScript.psc` |
| Native save data and SKSE callbacks | `ControllerState.h`, `StateCodec.h`, `ControllerStorage.cpp` |
| Mechanical vanilla API/external controller calls | `YLIWF_Engine.psc` |
| Activation, death, combat, unloading, and timers | Native event listeners and scheduler |
| Packages, dialogue conditions, factions, and aliases | `src/plugin/` |

The native worker executes plans on the main thread. It calls CommonLib directly
where available. Alias assignment/clearing, teammate and relationship setters,
messages, objectives, globals, and hireling calls use small engine adapters.
These adapters validate the current ticket immediately before the call and report
completion afterward. They contain no queue, plan, delay, or follower policy.

`YLIWF_SKSE` exports only functions called by the maintained scripts, plus the
typed `DebugInt` logger. Queue, planning, roster, and repair primitives remain
internal C++ functions; they are not registered with the Papyrus VM.

## Registry and saved ABI

`DialogueFollower` aliases remain authoritative: primary 0, animal 1, extras 2–8.
Bindings require the expected owner, ID, and case-insensitive name. Recruitment
preserves an occupied primary; promotion assigns primary before clearing extra.
Lowering the cap preserves followers and blocks further recruitment.
Manual primary selection exchanges the selected extra alias with the primary,
or clears the extra after filling an empty primary. Both actor handles are
retained and the adapter validates the captured occupants before editing aliases.
On acknowledgement, native waiting deadlines move with their actors; timeout
processing waits until the exchange completes. Neither actor is dismissed.

Alias writes mark the owning quest's runtime data for saving. Saved counts alone
do not persist alias occupants. No separate follower roster is restored on load.

Runtime commands, steps, and receipts have named fields; each command/step owns
its actor handle. One optional active operation replaces separate ownership flags.
Only the save codec uses packed arrays and actor indices. Version one is the
current YLIWF co-save format; this encoding does not provide SFF/YLIF migration.
The primary-exchange effect stores its second actor in the existing actor pool;
its encoded argument references that handle. Other effects keep their encoding.
Operation/effect numbers and version-one FIFO/receipt records retain their layout;
retired wrapper-only operation IDs are named `Reserved...`. Saved effect ID 10
(`ReservedUpdate`) resumes as a no-op; current plans publish counts at completion.
A versioned `CTRL` record under the stable SKSE ID `YLIW` stores native state in
the matching `.skse` co-save: plan, progress, queue, retained actor handles,
remaining delay, deadlines, completion receipts, and latent caller identities.
The codec uses explicit little-endian fields and validates lengths and invariants
before publishing a loaded state. It never writes pointers or struct padding.

Actor handles are retained while native work references them. SKSE remaps them
on loading; missing handles for accepted work block execution rather than replay
against another actor. An unavailable last-speaker hint can be cleared. Bounded
unsupported or invalid records are retained as blocked diagnostic records on
subsequent saves. They cannot resume using another save's load-order table;
recovery requires the original save pair and its matching build.

Extra aliases and mod globals are resolved directly from the canonical plugin.
`DialogueFollowerScript` extends `Quest` and has only vanilla properties/methods;
there is no Papyrus state container or separate restored follower roster.

## Requests and completion

An idle executor starts immediately on the unpaused main thread when no older
request is queued. Off-thread, paused, busy, and reentrant requests use the FIFO.
UI/event callers do not wait for completion. The FIFO holds at most 16 requests;
full queues reject without evicting accepted work. Adjacent maintenance syncs
coalesce; follower commands form ordering barriers. Existing UI tickets reuse
queued, active, or recent receipts.

Compatibility methods use native latent functions. They return after verified
completion, preserving dependencies such as Blades recruitment waiting for
completed dismissal. At most 32 callers await Boolean results. A continuation
records its original stack, function, script, and object handle; identity checks
prevent returning a result to a reused stack. Verified results remain with the
caller until it resumes, even if the rolling receipt history expires.
Caller registration precedes immediate execution; a result waits until the
original VM stack is actually suspended before it is returned.

Activation requests a coalesced count refresh, without dead-slot cleanup or
promotion. A successful background refresh preserves the last command's
diagnostic result. Explicit reconciliation still runs the full `Sync` policy.

Group actions append one request per unique humanoid follower atomically.
Selected alias `-2` captures party targets; planning resolves current slots after
promotion. Dead captured followers can still have service flags released.
Group feedback is transient; accepted requests survive reload.

Validation checks ownership and the expected occupant without applying effects;
one executor switch performs mutations. An acknowledgement
must match the exact ticket and offset; duplicates and skipped steps cannot
advance progress. Counts are published even after partial failure. Completion
verifies alias, teammate, waiting, faction, animal count, count/gate, and dismissal
state. The last 16 receipts retain command results.

Receipts certify observable state at completion, not eventual dialogue playback,
physical movement, or AI behavior. Partial effects are not automatically replayed.

## Delays, timers, and loading

Dismissal saves a two-second remaining delay, excluding paused time. Waiting
followers save absolute deadlines 72 game hours ahead. Cancel/follow/clear edits
remove deadlines. Native timers forward due requests into the ordinary queue;
the scheduler checks the saved deadline before accepting a timeout.
Starting and cancelling timers only edit native deadlines; they do not dispatch
Papyrus calls or register/unregister alias game-time updates.

A scheduling thread sleeps indefinitely when no deadline needs service. During
active delays it schedules main-thread checks; game-hour timers and deferred
caller returns use slower checks.
The thread never reads engine/VM state. Menu snapshots are independent of this
scheduler and still refresh only while visible.

Loading invalidates transient tasks and clocks, then resumes saved native state.
An accepted engine call stays in its saved phase until the adapter stack returns;
it is never resubmitted just because loading or VM pressure delayed it. The saved
caller identities are checked against the loaded VM before returning a result. Returning to the main menu
stops scheduling for the departed game.

After 30 unpaused seconds without progress, diagnostics retain ownership and
report an unknown outcome. A timeout does not authorize overlapping repairs or
replay. Invalid saved phases, delays, or caller tables remain visible diagnostics.

**Install DLL and PEX files together, and upgrade while idle.** Old Papyrus-stored
queues and suspended executor bytecode are not converted. Keep the `.ess` and
matching `.skse` files together; the engine saves followers in the quest, while
the co-save stores pending native work. New games and older saves without this
native record start with an empty native queue, not a reconstructed roster.

UI tickets and group feedback are transient. Saved accepted commands have those
tickets cleared so they can execute after loading without targeting an old UI
session. Tasks not yet accepted by the native queue remain generation-guarded.

Binding repair validates the canonical quest and repairs its vanilla primary
alias/count properties without replacing actor registrations or deadlines.
See [repair instructions](debugging.md#repair-missing-bindings).

## Validation

`build.ps1 -Target Test` checks saved numbers, wrappers, adapter guards, planning,
queue invariants, acknowledgements, delay/pause handling, co-save round trips,
corrupt data rejection, handle remapping, caller identity, and artifact integrity.
Engine stand-ins do not prove VM scheduling or save/load behavior. Follow the
[in-game checklist](debugging.md#verification), including a save/reload during a
native dismissal delay and while an adapter/caller is pending.
