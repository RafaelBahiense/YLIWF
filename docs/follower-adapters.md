# Native follower add-ons

An add-on is a separate SKSE DLL implementing
[FollowerAdapter.h](../include/sdk/FollowerAdapter.h). It delegates commands to
a follower's existing controller and reports completion.
See the [available add-ons](../README.md#add-ons).

## Ownership and scope

Active followers appear in Followers and participate in party commands when
allowed. They retain their owner's state, aliases and recruitment.

They do not consume YLIWF slots or receive its packages, protection, homes,
timers or primary promotion. The vanilla presence flag includes active external
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

Callbacks run on the main thread: keep them quick, nonblocking and exception-free.
Pointers and context must remain valid until process exit; no hot unloading.
Only fixed-width values, pointers and caller-owned buffers cross the x64 ABI.
No STL/engine objects, allocation ownership or exceptions cross it.
Invalid versions, tables, IDs, states and capabilities are rejected.

The original prefixes remain `BaseAPISize` and `BaseAdapterSize`.
Extensions append `stateChanged` and `setFollowDistance`; check size before
accessing them, using `CanNotifyState(api)` for notifications.

Call `stateChanged(registration)` after external state changes. It is thread-safe;
the host coalesces notifications and checks state/counts on the main thread.
Notifications do not complete pending commands. Visible captures also discover
state; no continuous roster polling is added.

For `Pending`, call `complete(registration, request, success, reason)` exactly
once when finished. It is thread-safe and copies the reason. The host verifies
the reported final state. Failure must mean finished with a known failure,
not still executing.

v1 supports Follow, Wait and Dismiss for active followers, not recruitment.

## Distance preferences

The host stores choices in a hidden marker faction, without adding packages or
aliases. Global selection resets the current roster; individual selection changes
one actor. Unsupported adapters retain their own spacing.

Choices apply on load, notifications and visible captures when Follow is allowed.
Blocked followers retain a global choice for later; their individual controls
are disabled. Callbacks must recheck eligibility before modifying state.

## Commands and saves

The host queues at most 16 requests, one outstanding per actor, and serializes
controller calls. Eligibility is checked again at dispatch.
After 30 active seconds without acknowledgement, the actor is quarantined.
Paused/frozen VM time does not count. A late acknowledgement releases quarantine;
the call is never replayed, and other actors remain usable.

Adapter queues and feedback are transient. Loading cancels queued requests and
invalidates old completions; accepted calls are not reissued. The owner's saved
state is rediscovered. Add-ons must account for the owner's persistence and
saved VM stacks.

## Serana example

[Serana.cpp](../src/addons/serana/native/Serana.cpp) validates the initialized
`DLC1_NPCMentalModelScript`, actor/alias bindings, properties and methods.
State comes from that controller.

| Command | Dawnguard method | Guard |
| --- | --- | --- |
| Follow | `StopWaiting()` | Active, `CanFollow`, not `LockedIn` |
| Wait | `Wait()` | Also `IsWillingToWait` |
| Dismiss | `Dismiss()` | Also `CanBeDismissed` |

VM completion schedules a main-thread state check before acknowledgement.
Quest locks and recruitment are never bypassed.
Location and Dawnguard quest events notify the host; visible refreshes cover
other changes. Events are hints, not proof Papyrus has finished.

Distance sets exactly one of `FollowDistanceClose`, `FollowDistanceMedium`
or `FollowDistanceFar`, after validating all three. AI reevaluates only on change.
Blocked/inactive controllers wait until eligible.

Replacement scripts must preserve the controller's behavior as well as its
interface; validate compatibility in game.

## Source layout and build

```text
src/addons/<addon>/
  addon.json
  native/       # C++ and headers
  plugin/       # Optional ESP source
  papyrus/      # Optional scripts
```

Create folders only when needed. CMake discovers each add-on's `native/**/*.cpp`;
ESP/script components need build integration when introduced.

```json
{"name": "Example"}
```

CMake discovers manifests, applies shared configuration and adds DLLs to
`native_plugins`. ModTools records hashes and packages separate ZIPs with notices.
No per-add-on build-script edits are needed.
Full builds require all declared DLLs; repackaging includes only supplied DLLs
with matching source-manifest hashes.

External authors may use their own SKSE project with the SDK header.
