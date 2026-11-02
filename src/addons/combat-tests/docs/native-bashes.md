# Native bash dispatch

Normal bashes use the engine's right-attack selection with NPC input mode. The
owned request must resolve to `bashStart` before deferred work can dispatch.

Power bashes use the add-on's standalone `YLIWFCombatTestNpcPowerBash` idle with
the right-power-attack action. The idle requires the Power Bash perk, active
blocking, block intent and an idle attack
state. It has no parent or previous idle and does not join the ordinary attack
tree. The core also evaluates the winning power-attack root conditions before
dispatch, preserving modded root restrictions. The resolved event must be
`bashPowerStart`; normal bashes and weapon attacks are rejected for this request.

Both bash kinds use engine-created `TESActionData` with NPC input mode and the
captured native combat processor. For power bashes, checked idle event and idle
form populate the explicit-idle output fields. Initial and resolved validation
still run; the selected event must match the requested kind. Capture is
thread-local and restricted to the owned test actor. Power-bash configuration
requires the supported processor, selector and resolved-validation protocols.

The host receives idle and condition-gate form IDs before guard acquisition.
Forms are resolved and conditions evaluated on the main thread. Worker-side
validation uses only the owned action identity and expected bash kind. No actor
perks, factions or vanilla idle records are modified.

Before dispatch, the core arms an engine-free evidence mailbox with actor, test
generation and request ID. The add-on forwards actual graph callbacks through
the host's `bashRequest` / `bashAnimation` functions, preserving that captured
key. Callbacks never acquire the controller mutex. Old actor/generation/request
keys are rejected; rejection, completion and invalidation clear the mailbox.
Graph events do not carry engine request IDs: attribution uses the single armed
owned request and requires the previous action to settle before another starts.

`SBF_BashStart` acknowledges execution alongside native `kBash` observations.
Once graph start is known, native idle alone cannot complete the request: a
specific `bashStop`, `SBF_BashStop` or `bashExit` must precede two idle samples
with animation recovery clear when readable. Generic `attackStop`, outgoing
notifications and hit frames do not establish completion. Native-state-only
completion remains the fallback when no graph start was observed.

Observed dodge, stagger or recoil interruptions and test teardown mark the
request interrupted; they do not themselves prove cleanup. Interrupted requests
are excluded from completed totals. A completed request does not prove a landed
hit or useful interrupt. Reports separate graph starts/ends, native starts,
interruption, rejection and unconfirmed execution. Existing bounded observation
and no-replay rules apply.
