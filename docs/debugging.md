# Debugging follower state

Open **SKSE Menu Framework → You Lead, I Will Follow**. The framework is optional
for gameplay and required for these controls.

## Menu and command feedback

**Followers** shows registered actors, Following/Waiting status, and Follow,
Wait, Dismiss, and Make primary commands. The primary follower is labeled in the
list. **Debug options** reveal installation diagnostics,
IDs, empty slots, repairs, and actor inspection/recruitment. **Log mod flow** is
independent; both switches default to off.

Follow All and Wait All apply to unique living followers; Dismiss All requires
confirmation and includes dead registrations. Group commands target the roster
at submission and report verified results per follower. The animal is excluded.
Location and approximate straight-line distance in metres are visible with debug
options off. Distance is unavailable for unloaded actors or different
interiors/worldspaces.

Opening Followers or Debug refreshes the snapshot. State changes invalidate it
for the next visible frame, with a one-second refresh fallback. Closing the menu
stops captures and refresh tasks; Settings and other mods' pages do not poll
the roster. The visibility observer reads no game state.

Dismiss, clear, recruit, and release require confirmation. Close a paused menu
to let the controller and engine adapters execute. Queue acceptance is not completion: feedback comes from
verified results. The UI allows one pending command; the shared controller queue
holds up to 16 requests.

After 30 unpaused seconds without progress, diagnostics show an unknown outcome
and retain ownership. A delayed operation is not replayed automatically.
See [controller contracts](native-controller.md#delays-timers-and-loading) for
save/load behavior.

## Capture a problem

1. Enable **Log mod flow** and reproduce the issue.
2. Use **Dump full context**, including the inspected actor if relevant.
3. Copy the newest log and dump from the SKSE log directory, normally
   `Documents/My Games/Skyrim Special Edition/SKSE`.
4. Disable flow logging when finished. **Save debug settings** persists both
   switches in `Data/SKSE/Plugins/YouLeadIWillFollow.ini`.

`YouLeadIWillFollow.log` contains timestamps, thread IDs, and numbered native/Papyrus
flow events. Logs rotate at 10 MiB with five backups. Saving settings, dumping,
and disabling logging flush the log.

Matching VM messages for `DialogueFollowerScript` and
mod-prefixed scripts are forwarded with severity and stack text even when flow
logging is off. A separate `Papyrus.0.log` is not required.

`YLIWF-context-<timestamp>-<sequence>.txt` captures settings, load order,
native checkpoints/callers, selected quest stages/aliases/script variables, all
array entries, and actor
faction, teammate, waiting, package, location, and protection state. Selection
follows mod, follower/hireling, home, Blades, party-actor, and script links.

Dumps work with logging off. They read live values individually, not an atomic
save snapshot or running VM stacks/timer queues. Large dumps may briefly pause
the game; unrelated quests or dependencies hidden inside other scripts may be
outside their selection.

## Installation diagnostics

After loading, Followers or Debug reports the winning `DialogueFollower` plugin,
configured cap, alias capacity, and resolved extra aliases (expected seven). This
summary is also logged with flow logging off and when the diagnosis changes.

YLIWF is an expected winning plugin. Structural failures identify missing IDs,
wrong alias types/names/owners, duplicates, or unavailable globals.
**Validated slots** means slots passing these checks, not occupied followers.
An uninitialized script is reported separately.

Fix structural quest conflicts before attempting binding repair. A valid quest
with missing vanilla properties can use reconciliation; other repairs become
available after it completes and the snapshot refreshes. `PlayerFollowerCount`
also gates dialogue, so a temporary zero during recruitment is normal.

## Repair commands

Save before repairing. Use an actor's **reference FormID**, selected in the Skyrim
console, for inspection; its NPC base ID is not sufficient. Check whether another
follower framework owns an actor before releasing orphan service flags.

| Action | Effect |
| --- | --- |
| Reconcile counts / dead slots | Repairs validated alias/global bindings, cleans dead slots, promotes an extra follower when needed, and recalculates counts/gates. |
| Repair flags / duplicates | Keeps the primary registration if present, otherwise the selected slot; restores service/protection flags and a valid waiting state/timer. |
| Follow / Wait | Deduplicates registration and applies the quest's service flags and follow/wait behavior. |
| Make primary | Moves a living extra follower into the vanilla primary slot. The previous primary takes their extra slot; following/waiting state and remaining waiting deadlines stay with each actor. Works with a full party and does not dismiss either follower. |
| Dismiss | Silently dismisses and verifies removal from aliases and teammate/current-follower state; reevaluates AI and reconciles the party. |
| Clear this slot | Clears the selected alias/timer; releases service state only if no registration remains. Does not run hireling dismissal. |
| Recruit into party (debug) | Uses normal recruitment for a living eligible actor with capacity and no existing registration. |
| Release orphan service flags | Releases an unregistered actor's teammate/current-follower/wait/protection state; refuses the animal alias. |

Follow, Wait, Dismiss, and Make primary work without debug options. Repairs do not reset quests,
change quest stages, or resurrect actors. Original essential/protected flags can
only be restored while their native cache is available; it is not saved.

## Repair missing bindings

Use reconciliation when the vanilla `pFollowerAlias` or `pPlayerFollowerCount`
property is missing or points at the wrong form. Extra aliases and YLIWF globals
are resolved natively, so they no longer depend on saved script properties.

1. Load the save and enable **Debug options**.
2. Click **Reconcile counts / dead slots** once, then close the menu.
3. After completion, refresh or dump again. Expect the canonical primary alias,
   the vanilla count global, seven resolved extra aliases, and matching counts.
4. Save and reload to confirm persistence, then retry failed recruitment.

Repair validates quest identity, alias names/types/ownership, globals, and the
request ticket before writing. It cannot repair corrupt or unsupported native
co-save records or replay an operation whose outcome is unknown.
## Writing flow logs

Native formatting occurs after the logging flag is checked:

```cpp
mod::debug::Trace("Native.CountFollowerSlots", nullptr, "slots={}", slots);
mod::debug::TraceLazy("Native.ApplyFollowerDialogueGate", speaker, [&] {
    return fmt::format("cap={}", GetEffectiveFollowerCap());
});
```

Argument expressions run before the call. Use `TraceLazy` for getters or expensive
diagnostic work; its callback runs synchronously and is never retained.

Papyrus should pass values already available to the script:

```papyrus
YLIWF_SKSE.Debug("Papyrus.follower3dnpc.SetFollower.exit", FollowerActor)
YLIWF_SKSE.DebugInt("Papyrus.follower3dnpc.DismissFollower.enter", None, "iMessage", iMessage)
YLIWF_SKSE.DebugForm("Papyrus.follower3dnpc.ResolveFollower.return", None, "result", target)
```

`DebugBool` records booleans. These native functions check the flag before
formatting and write through a thread-safe sink on Papyrus tasklets. Forms are
logged as IDs or `None`, without querying mutable actor state.

Papyrus also evaluates arguments first. Guard getters and composed strings:

```papyrus
If YLIWF_SKSE.IsDebugLoggingEnabled()
    YLIWF_SKSE.Debug("Papyrus.follower3dnpc.DismissFollower.enter", akFollower, "follower=" + akFollower)
EndIf
```

Build/install instructions are in the [build guide](build-release.md).

## Verification

Run `build.ps1 -Target Test` for automated checks. Before release, test in Skyrim:

1. With logging off, recruit two followers and test follow, wait, dismissal,
   death/promotion, and fast travel. Repeat with logging on, then disable it and
   confirm flow lines stop.
2. Toggle debug options separately from logging. Check normal roster actions,
   hidden IDs/empty slots/repairs, and immediate dismissal confirmation. Cancel
   preserves the party; completion removes only the selected actor.
3. Dump with logging on and off. Check follower/home aliases, hireling, animal,
   and Blades state, load order, and actor package/faction data.
4. Test installation diagnosis with a correct quest, a conflicting override,
   missing vanilla properties, and an uninitialized script. Repair healthy bindings
   repeatedly and confirm they survive loading.
5. Remove service flags or duplicate an actor across aliases. Repair retains the
   primary registration, valid waiting state and 72-hour timer, correct protection,
   and counts matching unique live followers.
6. Clear one duplicate slot, then the last registration. Check service release
   and promotion; unrelated aliases and quest stages stay unchanged. Dismiss a
   dead slot and reconcile changed counts.
7. Recruit an eligible inspected actor and release an orphan. Reject player,
   dead actor, base ID, animal, invalid ID, and full-party requests. Lower the cap
   below party size: preserve followers and block further recruitment.
8. Test wait timeout, unload, homes, animal commands, Blades recruitment, and
   optional 3DNPC forwarding. Blades must finish dismissal before continuing.
9. Save during a dismissal line and an accepted engine adapter, then reload and
   check completion. Keep the matching `.skse` co-save with the `.ess`. Trigger alias
   events during that wait; confirm one executor, ordered pending maintenance,
   and a verified receipt without duplicate effects.
10. Save/reload twice with another recruitment between cycles; registrations and
    bindings must persist.
11. Queue a UI command while paused, then load another save. Reject old UI tickets
    without affecting the new party. Delayed/old-script work must not allow
    overlapping repairs. A paused menu exceeding 30 seconds must not trigger an
    active-time stall warning.
12. Close the menu, change followers through dialogue, and reopen: refresh
    immediately. Test Settings/other pages and a non-pausing menu. A queued
    capture must skip after closing; command feedback survives reopening and
    inspection remains correct during refreshes. Under VM load, delayed work
    should complete without replay.
13. Test all three group commands with eight followers, duplicates, and dead
    slots. Cancel Dismiss All, then confirm it; verify partial failures and save/load
    during execution. With debug options off, check location/distance in the same
    room, outdoors, and across different interiors or unloaded areas.
