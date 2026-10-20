# Native follower add-ons

An add-on is a separate SKSE DLL implementing
[FollowerAdapter.h](../include/FollowerAdapter.h). It delegates commands to
a follower's existing controller and reports completion.
See the [available add-ons](../README.md#add-ons).

## Ownership and scope

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

The mandatory table prefixes are `BaseAPISize` and `BaseAdapterSize`.
Optional callbacks are `stateChanged`, `setFollowDistance`, `canRecruitThroughDialogue`
and `canReceiveCombatProtection`, plus the paired travel callbacks and `setStandDownPackage`.
Use `CanNotifyState`, `HasFollowDistance`, `HasRecruitmentEligibility` and `HasCombatProtection` before
accessing optional callbacks.
Use `HasTravelFormation` before accessing either travel callback. Older v1 tables
remain valid and retain ordinary movement.
Use `HasStandDown` before accessing the stand-down callback; unsupported adapters
remain under their own controller and are reported as unavailable for that order.

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
components; ESP/script components require explicit build integration.

```json
{"name": "Example", "version": "1.0.0", "adapterApiVersion": 1}
```

CMake discovers manifests, uses each add-on's version for SKSE/Windows DLL
metadata, applies shared configuration and adds DLLs to
`native_plugins`. ModTools records hashes and packages separate ZIPs with notices.
Add-on `*_test.cpp` files become separate CTest executables in `native_tests`.
Generic host/API tests live under the root `tests/` folder.
No per-add-on build-script edits are needed.
Full builds require all declared DLLs; repackaging includes only supplied DLLs
with matching source-manifest hashes.

Core and add-on release versions are independent. `adapterApiVersion` declares
the API used by the implementation; it must match the version requested during
registration. Validate this against the SDK at compile time. Unsupported
host APIs are rejected at runtime. Binary and source ZIPs use the add-on's name
and version, independent of the core ZIP filename.

External authors may use their own SKSE project with the SDK header.
