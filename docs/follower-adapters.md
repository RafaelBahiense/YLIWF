# Native follower add-ons

Add-ons integrate quest-managed followers through either an external controller
([FollowerAdapter.h](../include/FollowerAdapter.h)) or restrictions on a core-owned
follower ([FollowerPolicy.h](../include/FollowerPolicy.h)). The external API
delegates commands to the follower's controller and reports completion;
[core-owned policies](#policies-for-core-owned-followers) restrict actions and request packages.
Supplementary controllers can use [policies for core-owned followers](#policies-for-core-owned-followers)
without claiming the follower or replacing the quest owner's policy.
See the [available add-ons](../README.md#add-ons).

## External ownership and scope

Active followers appear in Followers and participate in party commands when
allowed. They retain their owner's state, aliases and recruitment.

They do not consume YLIWF recruitment slots or receive essential-status changes,
homes, timers or primary promotion. Combat protection and travel formations require
adapter opt-in. Formation packages temporarily control movement only.
The vanilla presence flag includes active external
followers; YLIWF's count and recruitment limit remain separate.
Duplicate claims or an actor already in YLIWF aliases disable commands.

## API v1

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
| `setFollowDistance` (optional) | Apply Close/Normal/Far synchronously, return 1 on success, preserve follow/wait state and respect restrictions |
| `canRecruitThroughDialogue` (optional) | Read-only: return 1 for an inactive actor eligible for the owner's recruitment dialogue, ignoring only the vanilla one-follower limit |
| `canReceiveCombatProtection` (optional) | Read-only: return 1 when the host may apply its crossfire-protection ability; respect quest restrictions |
| `canTravelInFormation` (optional) | Read-only: return 1 only during ordinary following; reject quest movement, scenes and foreign overrides |
| `setFormationPackage` (optional) | Apply the supplied host Travel package FormID synchronously; zero releases only the adapter's formation movement, even when its controller is blocked |
| `setStandDownPackage` (optional) | Apply the host combat-ignore package only when permitted; zero releases only the adapter's stand-down override, including while quest-blocked |

Callbacks run on the main thread: keep them quick, nonblocking and exception-free.
Pointers and context must remain valid until process exit; no hot unloading.
Only fixed-width values, pointers and caller-owned buffers cross the x64 ABI.
No STL/engine objects, allocation ownership or exceptions cross it.
Invalid versions, tables, IDs, states and capabilities are rejected.

The mandatory table prefixes are `BaseAPISize` and `BaseAdapterSize`. Check optional
callbacks with `CanNotifyState`, `HasFollowDistance`, `HasRecruitmentEligibility`,
`HasCombatProtection`, `HasTravelFormation` and `HasStandDown` before access.
Travel requires both callbacks. Older v1 tables remain valid; unsupported features
stay under the owner's controller and Stand down reports them as unavailable.

Call `stateChanged(registration)` after external state changes. It is thread-safe;
the host coalesces notifications and checks state/counts on the main thread.
Notifications do not complete pending commands. The host also observes adapters
once per unpaused, unfrozen second to catch controller changes without events.
This runs independently of menu visibility and does not dispatch Papyrus calls.

For `Pending`, call `complete(registration, request, success, reason)` exactly
once when finished. It is thread-safe and copies the reason. The host verifies
the reported final state. Failure must mean finished with a known failure,
not still executing.

v1 commands support Follow, Wait and Dismiss for active followers. Optional
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

## Source layout and build

```text
src/addons/<addon>/
  addon.json
  native/       # C++ and headers
  plugin/       # Optional ESP source
  papyrus/      # Optional scripts
  tests/native/ # Native test entry points (*_test.cpp)
```

Create folders only when needed. Automatic build discovery supports native
components and optional `papyrus/` sources. Add-on scripts compile against the
core bridge and ship only in that add-on's ZIP. ESP components require explicit
build integration.

```json
{"name": "Example", "version": "1.0.0", "adapterApiVersion": 1}
```

CMake discovers manifests, uses each add-on's version for SKSE/Windows DLL
metadata, applies shared configuration and adds DLLs to
`native_plugins`. ModTools records hashes and packages separate ZIPs with notices.
Add-on `*_test.cpp` files become separate CTest executables in `native_tests`.
Generic host/API tests live under the root `tests/` folder.
Full builds require all declared DLLs; repackaging includes only supplied DLLs
with matching source-manifest hashes.

Core and add-on release versions are independent. `adapterApiVersion` declares
the API used by the implementation; it must match the version requested during
registration. Validate this against the SDK at compile time. Unsupported
host APIs are rejected at runtime. Binary and source ZIPs use the add-on's name
and version, independent of the core ZIP filename.

External authors may use their own SKSE project with the SDK header.
`PapyrusController.h` supplies optional bound-controller helpers.

## Policies for core-owned followers

Compatibility patches that replace a follower's scripts may route recruitment
into the core instead of registering an external owner. `FollowerPolicy.h`
exposes `YLIWF_GetFollowerPolicyAPI(1)` independently of the adapter ABI. Register
one stable provider at SKSE PostLoad; its read-only inspection runs on the main
thread and returns action restrictions, an optional owner quest FormID, a package
preference and a reason. It must neither execute Papyrus nor modify game state.
Providers remain loaded for the session.

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
interrupt packages remain intact. Preferences are rediscovered after load and
withdrawn on dismissal; no package or controller state is serialized by this API.
