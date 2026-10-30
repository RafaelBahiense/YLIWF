# Follower integration

Add-ons integrate quest-managed followers through either an external controller
([FollowerAdapter.h](../../include/FollowerAdapter.h)) or restrictions on a core-owned
follower ([FollowerPolicy.h](../../include/FollowerPolicy.h)). The external API
delegates commands to the follower's controller and reports completion;
[core-owned policies](#policies-for-core-owned-followers) restrict actions and request packages.
Supplementary controllers can use [policies for core-owned followers](#policies-for-core-owned-followers)
without claiming the follower or replacing the quest owner's policy.
See [add-on development](README.md) for source layout and builds.

## Adapter actions

Implement `actions` and `invokeAction` to expose buttons and boolean toggles in
the selected follower's **Adapter options** section. Check
`HasActionControls(host)` for host support; `API.supportedActions` lists the
supported `ActionKind` bits.

`actions` is read-only, on the main thread, and writes at most the supplied
capacity (up to 16). Each `FollowerAction` has a stable nonzero adapter-local ID,
kind, enabled flag, toggle value, label, description and disabled reason.
Text buffers must be null-terminated. Duplicate IDs and malformed descriptors
invalidate the entire list. Button values are zero; toggle values are zero or one.

The host copies descriptors into UI snapshots. Invocation returns to the game
thread, verifies the session and expected adapter, re-enumerates the action and
checks its current enabled state and value. Pending/quarantined follower commands,
dead actors and conflicting ownership block invocation. The provider must also
revalidate its own restrictions. `invokeAction` is synchronous: return one only
when the effect is complete, with optional result text. Do not retain buffers,
schedule deferred effects or call `API.complete` for these actions.

Providers own persistence of their settings and must reset transient state on
save changes. Notify `API.stateChanged` when an action's value or availability
changes outside invocation. Actions do not grant follower ownership or bypass
quest policies.

## External ownership and scope

Active followers appear in Followers and participate in party commands when
allowed. They retain their owner's state, aliases and recruitment.

They do not consume YLIWF recruitment slots or receive essential-status changes,
homes, timers or primary promotion. Combat protection and travel formations require
adapter opt-in. Formation packages temporarily control movement only.
The vanilla presence flag includes active external
followers; YLIWF's count and recruitment limit remain separate.
Duplicate claims or an actor already in YLIWF aliases disable commands.

## Registration and callbacks

At SKSE `PostLoad`, resolve `YLIWF_GetFollowerAdapterAPI` from the core DLL
with `GetProcAddress`. Call `QueryAPI(InterfaceVersion)`; null means unsupported.
Validate table size/version before registration.

```cpp
const yliwf::sdk::Adapter adapter{
    sizeof(yliwf::sdk::Adapter), yliwf::sdk::InterfaceVersion,
    "org.example.my-follower", "My follower controller", nullptr,
    Enumerate, Inspect, Start
};
const auto registration = api->registerAdapter(&adapter);
```

| Callback | Contract |
| --- | --- |
| `enumerate` | Write reference FormIDs; return the number written, bounded by capacity |
| `inspect` | Return 1 for owned actors, including inactive ones; fill state, command bits and optional reason; 0 for unrelated actors |
| `start` | Recheck eligibility; return `Rejected` without effects, `Completed` after finishing, or `Pending` |
| `setFollowDistance` | Apply Close/Normal/Far synchronously, return 1 on success, preserve follow/wait state and respect restrictions |
| `canRecruitThroughDialogue` | Read-only: return 1 for an inactive actor eligible for the owner's recruitment dialogue, ignoring only the vanilla one-follower limit |
| `canReceiveCombatProtection` | Read-only: return 1 when the host may apply its crossfire-protection ability; respect quest restrictions |
| `canTravelInFormation` | Read-only: return 1 only during ordinary following; reject quest movement, scenes and foreign overrides |
| `setFormationPackage` | Apply the supplied host Travel package FormID synchronously; zero releases only the adapter's formation movement, even when its controller is blocked |
| `setStandDownPackage` | Apply the host combat-ignore package only when permitted; zero releases only the adapter's stand-down override, including while quest-blocked |
| `coreMovementFeatures` and `movementPermissions` | Opt into core package handling and report independent Distance, Formation and StandDown permissions for the underlying owner package |

Callbacks run on the main thread: keep them quick, nonblocking and exception-free.
Pointers and context must remain valid until process exit; no hot unloading.
Only fixed-width values, pointers and caller-owned buffers cross the x64 ABI.
No STL/engine objects, allocation ownership or exceptions cross it.
Invalid versions, tables, IDs, states and capabilities are rejected.

Registration requires `enumerate`, `inspect` and `start`. Implement the other
callbacks for the capabilities your adapter provides; leave unused callbacks null.

Validate table bounds with `BaseAPISize` and `BaseAdapterSize`. Check host and adapter
callbacks with `CanNotifyState`, `HasFollowDistance`, `HasRecruitmentEligibility`,
`HasCombatProtection`, `HasTravelFormation` and `HasStandDown` before access.
`UsesCoreMovement` identifies features handled by the core; `HasCoreMovement(host)`
checks host support. Custom formation handling requires both
`canTravelInFormation` and `setFormationPackage`. Unsupported features stay
under the owner's controller; Stand down reports them as unavailable.

Call `stateChanged(registration)` after external state changes. It is thread-safe;
the host coalesces notifications and checks state/counts on the main thread.
Notifications do not complete pending commands. The host also observes adapters
once per unpaused, unfrozen second to catch controller changes without events.
This runs independently of menu visibility and does not dispatch Papyrus calls.

For `Pending`, call `complete(registration, request, success, reason)` exactly
once when finished. It is thread-safe and copies the reason. The host verifies
the reported final state. Failure must mean finished with a known failure,
not still executing.

Commands support Follow, Wait and Dismiss for active followers. Implementing
recruitment eligibility relaxes the count gate only for that dialogue speaker;
the owner's dialogue still recruits and enforces quest conditions. Background
count updates preserve this gate, and closing dialogue restores follower presence.

## Distance preferences

The host stores choices in a hidden marker faction, without adding packages or
aliases. Global selection resets the current roster; individual selection changes
one actor. Unsupported adapters retain their own spacing.

Choices apply on load and observed controller changes when Follow is allowed.
Blocked followers retain a global choice for later; their individual controls
are disabled. Callbacks must recheck eligibility before modifying state.

## Core-managed external movement

Set `coreMovementFeatures` to the supported `MovementBit` flags and provide
`movementPermissions(context, actor, ownerPackage, state)`. It is a read-only main-thread
callback returning the subset currently allowed by the owner's controller and
underlying package. Unknown or unadvertised bits reject the decision. The core
does not infer movement permission from command capabilities.
`state` is the fresh owner inspection, valid only during the callback; adapters
can reuse it instead of repeating their controller inspection.

`Distance` uses YLIWF's ordinary follow packages. `Formation` and `StandDown` use
the host's travel and passive/avoidance packages. Stand down takes priority over
formation, then distance; a blocked higher-priority request does not fall back to
another forced behavior. Custom callbacks remain available for features without
core opt-in. Do not implement package application in both places for one feature.

`setFollowDistance` mirrors an owner's native distance preference.
It runs before core distance selection and must preserve waiting/recruitment.
An adapter using its owner's distance implementation can opt
into Formation and StandDown without opting into Distance.

The core checks loaded state, scenes/dialogue, locomotion, combat, pending commands
and foreign interrupts. Distance and formation require following outside combat;
Stand down may operate during combat and waiting. Adapters retain additional
story, sandbox, package and mount restrictions. Returning zero withdraws permission.

Owned interrupts are tracked per actor. Only a package applied by this service is
released during runtime; foreign replacements stay intact. Permission and the
underlying package are rechecked after reevaluation. The shared main-thread
movement update runs at a 200 ms cadence while selections exist, independently
of menus; paused/frozen time suspends observation. No add-on worker is needed.

Commands release transient movement before owner dispatch. Save loads clear
transient selections and reconcile recognized saved packages only for unique
external owners opting into this service. Conflicting/native ownership excludes
cleanup. Preferences are rediscovered; no new serialization format is added.
Cleanup for unloaded actors waits for their process/3D and rechecks ownership
before touching the restored interrupt.

`host->isMovementPackage(package)` recognizes the service's package records for
owner validation. It is read-only, main-thread only, and grants no authority to
install or release an interrupt. Add-ons using this service require a host for
which `HasCoreMovement(host)` succeeds.

## Commands and saves

Crossfire protection uses the core setting and ability for eligible living
followers. The host removes it on dismissal, restriction or disabling the setting,
and reconciles saved abilities on load. It does not change essential flags.
Player friendly-fire protection also covers player teammates.

The host queues at most 16 requests, one outstanding per actor, and serializes
controller calls. Eligibility is checked again at dispatch.
After 30 active seconds without acknowledgement, the actor is quarantined.
Paused/frozen VM time does not count. A late acknowledgement releases quarantine;
the call is never replayed, and other actors remain usable.

Adapter queues and feedback are transient. Loading cancels queued requests and
invalidates old completions; accepted calls are not reissued. The owner's saved
state is rediscovered. Add-ons must account for the owner's persistence and
saved VM stacks.

## Travel formations

The host assigns eligible adapter followers a shared formation target and distance
preference. There are eight movement slots; core followers have priority when full.
These slots are separate from recruitment aliases and do not transfer ownership.
Movement suspends for pending commands and the ordinary formation restrictions.

The adapter must recheck permission when applying a package, preserve foreign
movement overrides, and release only its own temporary movement. Releases are
allowed while waiting, dismissed or quest-locked. The host clears saved formation
movement on load and reevaluates permission during travel updates.

## Papyrus helpers

External authors may use their own SKSE project with the SDK headers.
`PapyrusController.h` supplies bound-controller helpers.
`PapyrusCommands.h` provides `papyrus::Commands` for zero-argument owner methods:
call `Dispatch` with the bound object, method, request identity and verification
function. It retains the controller, schedules verification on the main thread,
tracks per-actor pending work and reports one completion. Invalidate the
process-lifetime helper on SKSE PreLoadGame and NewGame. Old callbacks cannot
complete or clear a request from a different session. The helper never retries
or times out a VM call; the host's acknowledgement/quarantine contract applies.

The core owns registration, observations, request queues, acknowledgements and
movement packages. Adapters own controller interpretation, quest permissions and
final-state verification. Owner-specific scripts and hooks stay in the add-on.

## Policies for core-owned followers

Compatibility patches that replace a follower's scripts may route recruitment
into the core instead of registering an external owner. `FollowerPolicy.h`
exposes `YLIWF_GetFollowerPolicyAPI(1)` independently of the adapter ABI. Register
one stable provider at SKSE PostLoad; its read-only inspection runs on the main
thread and returns action restrictions, an optional owner quest FormID, a package
preference and a reason. It must neither execute Papyrus nor modify game state.
Providers remain loaded for the session.

After a stored state transition, call `stateChanged(providerID, actorID)` on the
main thread to refresh package and combat policy promptly. Queue and coalesce
notifications originating in VM callbacks; do not call it from `inspect`.
Periodic policy observation remains the fallback. The notification cannot grant
permissions or authorize replaying a pending command.

Several providers may apply to one actor. Restrictions accumulate; one provider
cannot undo another's restriction. Supplementary providers leave `ownerQuest` zero.
Only the designated owner can authorize quest release. Different nonzero owner
claims or invalid results suspend control and release authority.

The core checks policies at command execution and before taking movement/combat
control. Quest release validates the registered owner and uses a distinct saved
operation, preserving existing operation IDs. It bypasses user-command locks only
for the owner's explicit release and never authorizes recruitment or alias repair.
Use `YLIWF_SKSE.HasQuestPolicy(actor, owner)` before a replacement script delegates
control. These policies do not add external followers or consume adapter slots.

### Package requests

`packagePreference` supports `Default` (no request), `Follower` (eligible YLIWF
alias packages) and `Requested` (the runtime package FormID in `package`).
The highest `packagePriority` wins. Equal-priority requests for the same selection
combine; incompatible requests at the highest priority return package selection
to Skyrim while retaining action restrictions and quest ownership.

For example, a quest can block dismissal and request a travel package at priority
10, then return `Default` when the journey ends. Its package must supply valid
conditions and targets; this API does not fill aliases or bind package data.

Requests apply only to core-managed humanoid aliases. The core resolves a requested
FormID as a package and checks its conditions. A missing or ineligible package
withdraws selection instead of falling back to a different forced package.
Scenes, pending commands and movement restrictions suspend selection. Explicit
policy requests remain active when an actor's 3D unloads, so background AI keeps
the selected follower package. Disabled actors remain suspended. Explicit
interrupt packages remain intact. Preferences are rediscovered after load and
withdrawn on dismissal; no package or controller state is serialized by this API.
