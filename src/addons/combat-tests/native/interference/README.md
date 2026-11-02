# Combat interference controls

`Control.h` and `UND.h` are runner-facing interfaces. `Rules.h` contains engine-independent
scope rules. Keep DLL inspection, engine ABI assumptions, return-address checks
and interception in backend implementation files. The core has no dependency on
this component.

## PermaBlock interruption backend

`GraphNotifications.cpp` observes NPC `NotifyAnimationGraph` through the Character
graph-holder vtable. `PermaBlock.cpp` supplies its caller-scoped interruption
filter. CommonLib/Address Library resolves the table; there are no
third-party function offsets or instruction patches. The current backend accepts
Skyrim 1.6.1170 only.

The loaded `FixNPCPermaBlock.dll` executable PE section defines the caller scope.
Direct calls from that module, for the selected test actor, are counted when the
event is `blockStop` or `bashStop`. Bypass mode suppresses those notifications;
other callers, actors and events chain to the previous implementation. Observe
mode forwards every event. PermaBlock's update loop and timer bookkeeping remain
active, so this isolates its interruption events rather than disabling the DLL.

Installation retains the hook for the process lifetime. Actor scope is armed at
setup and cleared after restoration, on reset and before loading another save.
Counts reset per run. The preference is session-only and cannot change during a
run. Interrupted save recovery does not rearm interception.

The shared notification observer also feeds the bounded combat trace. It
captures a request before chaining and updates its result afterward, without
holding the telemetry lock across engine calls. Module ranges and names are
captured once at installation; workers resolve immediate return addresses only
against that immutable snapshot. Modules loaded later are reported as unknown.
Request tickets retain distinct sequence IDs across run resets so delayed results
cannot overwrite a newer entry in a reused slot. No additional events are sent.

Before arming, verify the actor uses the patched table and the observer remains
its outermost handler. A different actor implementation or a later vtable hook
disables bypass rather than assuming the caller can still be identified. Nested
helpers outside the source module and direct nonvirtual engine calls are outside
this backend's coverage. Zero intercepted calls do not prove the mod inactive.

## UND dodge backend

`UND.cpp` supports Skyrim 1.6.1170 and UND 2.4.3 with SHA-256
`47D7F6DEFD60E294955FC90A5DB1500059EF69912F1B8D95C2C7182B1E3381FE`.
It verifies the DLL file, loaded call targets and callee entry bytes before
patching eleven `Actor::HasKeywordString` calls for `UND_ExcludeDodge` and five
`dodge::able_dodge` calls. A dedicated trampoline is allocated in the intersection
of all call sites' signed 32-bit branch ranges. Its complete storage range is
validated before any call is patched; allocation failure leaves UND untouched.
The private call ABI and offsets remain in this backend.

For the selected actor, exclusion returns true and final eligibility returns
false while enabled. Otherwise the original UND callee runs. Keywords, actor
bases, INI settings, and other actors are unchanged. Eligibility interception
catches queued decisions that reach that gate after arming; already executing
dodges are not cancelled. Counts measure checks and overridden decisions, not
completed dodges or saved stamina. Zero overrides do not establish an intervention.

At each run, the backend rechecks all installed call bytes before arming.
Unsupported profiles or replaced call sites disable the control. Reset, load and
run completion clear actor scope; hooks remain installed and forward to UND.
The setting is session-only. Validate off/on series, another NPC still dodging,
abort/restoration, repeats, save loading, and original behavior after the test.

## Dodge budget

`DodgeBudgetRules.h` owns reserve and cooldown policy. The UND eligibility hook
first runs the original check, then applies the selected actor's budget to
otherwise permitted decisions. A scope epoch prevents delayed callbacks from
updating another run. Worker calls use a locked value snapshot with a 250 ms
expiry; actor inspection and cost evaluation run on the main thread. Missing
cost/state denies the scoped decision and increments the unavailable counter.

`UNDStamina.cpp` isolates the fingerprint-pinned cost function ABI, singleton and
stamina-factor layout. The loaded cost entry is rechecked before evaluation.
The estimate takes the larger step/roll cost and honors UND's cost-enabled
setting. No setting, keyword, spell or actor value is changed. The cooldown is
between permitted eligibility decisions; later UND checks can still prevent
animation, so it does not count completed dodges.

## External attack handoff

`ExternalAttacks.cpp` recognizes the immediate caller `ProjectGapClose.dll` and
its `attackStartSprint` / `attackPowerStart_Sprint` inputs, and
`FixNPCPermaBlock.dll` with `attackStart`, through the shared
notification observer. It reserves the current test actor's attack commitment
before forwarding the input. No third-party function offsets or byte patches
are used. The core exposes only an engine-free actor/token/sequence reservation;
module recognition and animation lifecycle interpretation stay in this backend.
PermaBlock attack counts are separate from its block/bash interruption filter.
Attack handoff applies in Guard and Combined modes whether interruption
suppression is enabled or disabled; it observes and reserves attacks without
suppressing them.

Accepted inputs stay committed across the pre-swing engine-state gap. A matching
SBF stop or `MCO_EndAnimation` begins a finishing handoff: ownership remains, but
the test host rejects new offensive action requests so ordinary AI cannot keep
extending the reservation. Already issued actions must finish. Only two
native-idle samples with readable recovery clear when available release ownership.
Guard selection remains deferred throughout the handoff. This narrow transition
applies in Guard and Combined independently of their regular offense-gating mode.

`Collision_AttackEnd` and generic `attackStop` do not prove completion. Without
a recognized start, fallback requires an observed native attack and recovery
cycle. Rejected inputs without start evidence roll back after stable idle.
Missing evidence retains ownership; no elapsed-time shortcut releases it. Newer
external inputs supersede older IDs and establish a new commitment. Reports
distinguish completion evidence, finishing handoffs, denied follow-up attacks
and scope-end cleanup. Closing the scope removes the handoff offense restriction.

Core guard selection rechecks the reservation before dispatch. Already accepted
guards retain their deadlines and normal release/cleanup path; only confirmed
cleanup permits later guard evaluation. Worker callbacks exchange ordinary values
through a dedicated engine-free mailbox and never acquire the controller mutex.
Neither side holds its mailbox lock across engine calls.

This handoff is armed only in Guard and Combined tests. Reset, load and restoration
clear scope; ending a scope with an active commitment is counted separately from
a reservation cleared during combat. The reservation is not serialized or replayed.
Validate the known pre-swing race, rejected inputs, attack end, overlapping newer
inputs, actor/run isolation, abort and restoration. Direct commands hidden behind
other wrappers are outside this caller-specific backend's coverage.

## Replacing the backend

Preserve the lifecycle and counter contract in `Control.h`. A cooperative mod API
or another verified interception method can replace this implementation without
changing runner, menu or core ownership rules. Keep scope tests in
`tests/native/interference_test.cpp`; engine chaining and caller provenance require
in-game validation.
