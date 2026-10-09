# Debugging follower state

Open **SKSE Menu Framework → You Lead, I Will Follow**.

## Menu and command feedback

**Followers** shows state, location, approximate distance and ordinary commands.
**Debug options** adds IDs, empty slots, repairs and inspection/recruitment.
**Log mod flow** is independent; both settings default to off.

Global Close/Normal/Far or **Apply to all followers** replaces the current
party's distance choices. Row controls change one actor. Choices persist in
game saves and through primary promotion; Settings **Save** retains the default
for new followers. Animals are excluded. Paths, combat and sandboxing affect
actual spacing.

Follow All/Wait All target unique living humanoids. Dismiss All includes dead
registrations and requires confirmation. Location distance is straight-line;
it may be unavailable for unloaded actors or different interiors/worldspaces.

Snapshots refresh on opening Followers/Debug, after state changes and once per
second while visible. Closed menus, Settings and other mods' pages do not poll.

Dismiss, clear, recruit and release require confirmation. Close paused menus
to allow execution. Feedback reports verified completion, not queue acceptance.
The UI allows one pending command. After 30 unpaused seconds without progress,
an unknown outcome retains ownership and is not replayed.
See [controller contracts](native-controller.md#delays-timers-and-loading).

## Capture a problem

1. Enable **Log mod flow** and reproduce the problem.
2. Use **Dump full context**; inspect the relevant actor first if needed.
3. Copy the newest log and dump from
   `Documents/My Games/Skyrim Special Edition/SKSE`.
4. Disable logging afterward. **Save debug settings** persists both switches.

`YouLeadIWillFollow.log` rotates at 10 MiB with five backups.
Saving settings, dumping and disabling logging flush it.
Relevant VM errors include severity and stack text even with flow logging off.

`YLIWF-context-<timestamp>-<sequence>.txt` includes settings, load order,
native work, linked quests/scripts and actor state. Dumps work without logging.
They read live values, not an atomic save snapshot; unrelated state may be
omitted, and large dumps can briefly pause the game.

## Installation diagnostics

Diagnosis reports the winning `DialogueFollower` plugin, cap, validated slots
and seven expected extra aliases. YLIWF itself is an expected winner.
Validated slots are structurally correct slots, not occupied followers.

Fix quest override conflicts before repairing bindings. Missing IDs, wrong
alias types/names/owners and unavailable globals are structural failures;
uninitialized scripts are reported separately. A temporary zero
`PlayerFollowerCount` during recruitment is normal because it also gates dialogue.

## Repair commands

Save first. Inspection needs the actor's **reference FormID**, not NPC base ID.
Check another framework's ownership before releasing orphan flags.

| Action | Effect |
| --- | --- |
| Reconcile counts / dead slots | Repair valid bindings, clean dead slots, promote if needed and recalculate gates |
| Repair flags / duplicates | Keep primary or selected registration; restore service, protection and waiting state |
| Follow / Wait | Deduplicate and apply follow/wait state |
| Make primary | Exchange primary and extra occupants, preserving each actor's state/deadline |
| Dismiss | Release aliases and service state, verify removal and reconcile |
| Clear this slot | Clear alias/timer; release service only if no registration remains; no hireling dismissal |
| Recruit into party (debug) | Recruit an eligible living actor with capacity |
| Release orphan service flags | Clear an unregistered actor's service/protection state; exclude animal alias |

Ordinary commands and promotion need no debug setting.
Repairs do not reset quests, change stages or resurrect actors.
Original essential/protected flags can be restored only while their unsaved
native cache exists.

## Repair missing bindings

For missing or incorrect `pFollowerAlias`/`pPlayerFollowerCount` properties:

1. Enable debug options and run **Reconcile counts / dead slots** once.
2. Close the menu, then refresh or dump after completion.
3. Confirm primary/count bindings, seven extra aliases and correct counts.
4. Save and reload before retrying recruitment.

Repair validates quest structure and request identity. It cannot repair corrupt
native co-save records or replay an unknown outcome.

## Writing flow logs

Native formatting checks the flag before running:

```cpp
mod::debug::Trace("Native.CountFollowerSlots", nullptr, "slots={}", slots);
mod::debug::TraceLazy("Native.ApplyFollowerDialogueGate", speaker, [&] {
    return fmt::format("cap={}", GetEffectiveFollowerCap());
});
```

Arguments evaluate before a call. Use `TraceLazy` for expensive getters;
its callback runs synchronously.

Papyrus helpers `Debug`, `DebugInt`, `DebugBool` and `DebugForm` check the flag
before formatting. Pass existing values; guard expensive expressions with
`IsDebugLoggingEnabled()`. Forms log as IDs without mutable actor queries.

## Verification

Run `build.ps1 -Target Test`, then check in game:

| Area | Cases |
| --- | --- |
| Gameplay | Recruit, follow, wait, dismiss, death/promotion, fast travel, homes and 72-hour wait timeout |
| Primary/party controls | Full party, duplicates, dead slots, promotion, all three party commands, cancellation and partial failure |
| Distance | Global reset, one-actor override, promotion and save/load; same room, outdoors and unavailable positions |
| Diagnosis/repair | Healthy quest, conflicting override, missing properties, uninitialized script, duplicates and orphan flags |
| Admission | Reject player, dead/base/invalid IDs, animals and full party; lowering cap preserves followers |
| Integrations | Animal/hireling behavior, Blades awaiting dismissal and optional 3DNPC forwarding |
| Persistence | Repeated save/load with recruitment; save during dismissal delay and pending adapter/caller; no duplicate effects |
| UI generations | Command queued while paused, then another save; reject stale work and avoid paused-time stall warnings |
| Refresh/logging | Dialogue changes while closed, reopen immediately, no hidden polling; toggle debug/logging independently and dump with either state |

Keep matching `.ess`/`.skse` pairs. Test under VM load: delayed work must complete
without replay, and repairs must leave unrelated aliases and quest stages intact.
