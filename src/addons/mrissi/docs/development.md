# M'rissi controller integration

The adapter uses `MRTDialogueFollowerScript` on `MRTDialogueFollower` (`033233`)
in `MrissiTailOfTroubles.esp`, with actor reference `1AEF15`. Its `pFollowerAlias`
must belong to that quest at alias 0; another actor in the alias disables commands.
The dismissed-faction property and all three zero-argument methods are validated
before dispatch.

| Command | Owner method | Completion |
| --- | --- | --- |
| Follow | `FollowerFollow()` | Active alias, teammate, WaitingForPlayer = 0 |
| Wait | `FollowerWait()` | Active alias, teammate, WaitingForPlayer = 1 |
| Dismiss | `DismissFollower()` | Alias cleared and dismissed/inactive state |

The actor must be alive, conscious and using an ordinary follow or idle package.
No-follow faction `033236`, broken faction `2AD67D`, scenes, pending commands,
mount-handling faction `047657` and foreign interrupt packages suspend controls.
Unknown waiting values or inconsistent registration never trigger repairs.
Location list `2AD680`, including parent locations, suppresses Wait and Dismiss
while allowing Follow. Dismiss also requires the following state, matching the
authored dialogue. The broken controller's separate animation methods are never
called by this adapter.

## Movement ownership

Package `033234` allows on-foot distance and formation movement. Idle packages
`047662` and `047663` retain sandboxing; they allow commands and passive Stand
down, but not travel overrides. The core's three distance packages, sixteen
formation packages and thirty-six Stand-down packages are the only accepted
interrupts. Stand down takes priority over formation, then distance.

Movement releases during restricted locations, scenes, dialogue, mounting,
swimming, sitting, missing 3D or foreign interrupts. Travel also releases during
combat and waiting. The base package is checked independently of the interrupt,
and permission is checked again after reevaluation. No authored package record,
faction, alias, quest stage, inventory or actor protection flag is rewritten.

A bounded 250 ms observer schedules main-thread checks; the worker reads no
engine objects. Commands release temporary movement before invoking the owner
through Papyrus. Completion is reported only after the VM callback and a fresh
state inspection. Save loads invalidate callbacks and clear transient movement;
commands are never replayed. The core persists actor distance preferences. The
adapter adds no serialization format.

## Validation

Rule tests cover registration, waiting and dismissal, restrictions, invalid
values and movement ownership. In-game verification remains required:

1. Recruit through M'rissi's dialogue; verify roster discovery without consuming
   core slots. Follow, Wait and Dismiss individually and through party commands.
   Resume following before testing Dismiss from a waiting state.
2. Compare distance presets with a core follower on open ground. Let M'rissi
   sandbox, then walk away; idle behavior should keep priority until following
   resumes. Change distance while waiting and confirm she stays put.
3. Test formations outside settlements, combat and dialogue; ordinary distance
   following should resume when formations end.
4. Test Stand down while following, waiting and in combat. Restore normal behavior
   and confirm combat and eligible travel preferences resume.
5. Exercise story scenes, no-follow/broken states, restricted locations, mounting
   and foreign interrupts. Controls and movement must yield without changing
   story state or another controller's package.
6. Save/load while following, waiting, dismissed and during a pending command.
   Verify rediscovery, cleared temporary orders and no repeated commands.
