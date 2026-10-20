# Fura FDE implementation and validation

## Controller ownership

Fura uses the core roster and command queue. The add-on registers a read-only
quest policy rather than an external follower owner.

`aaFuraFollowerScript.SetFollower(Int Status)` preserves its script identity,
signature and `DLC1FuraRef` / `CurrentFollowerFaction` properties. Status 1 uses
the normal recruitment transaction; status 0 requests an owner-validated release
of the explicit actor. Calls wait for verified completion and are never replayed
automatically.

The three Follow and two Wait response scripts route their speaker to
`FollowActor` / `WaitActor`. All replacement scripts check that the matching
quest-policy module is available before delegating to the core.

Bloodmouth's acceptance response belongs to `aaFuraQ2`. It sets stage 10 and
requests release through the separate `aaFura` controller, which owns
`aaFuraFollowerScript`.

## Packages and dialogue

FDE's ordinary `aaFuraFollowPackage` has higher quest priority than the core's
packages. The module prepends a runtime eligibility condition excluding the
original Fura reference, allowing core follow/wait/distance packages to run.
FDE special quest packages retain their conditions and priority.

The module adds runtime conditions to the winning vanilla Follow, Wait, Dismiss,
Trade and order responses, hiding them only for the original Fura. FDE's voiced
responses retain their record identities and audio paths. Its Follow, Wait and
Dismiss responses also require `aaFuraCannotFollow == 0`. These package and
dialogue edits are rebuilt at startup and are not written to an ESP or save.

## Quest policy

The provider validates the running FDE controller, saved actor/faction properties
and original alias. Missing bindings suspend control. `aaFuraCannotFollow`,
`aaFuraAtSawmill`, betrayal/hostility, scenes and FDE special packages block core
commands, homes, movement orders and active combat roles. Recruitment also
requires the introduction flag.

Party commands skip restricted followers; queued commands recheck the policy
before execution. `ReleaseActorForQuest` validates the registered owner and
permits explicit FDE story departures during player-command locks. The add-on
does not repair quest flags, stages or aliases, or adopt unmanaged followers on load.

## In-game validation

- Check core recruitment and the configured cap, Follow/Wait from both interfaces,
  party commands and dismissal when Fura is not the primary follower.
- Check distance, formations, stand down, combat roles, voiced responses and the
  absence of duplicate follower topics.
- Verify FDE's sawmill, Bloodmouth and Harkon movement and dialogue retain priority.
- Accept the vengeance task from a save before that response: verify automatic
  release, blocked recruitment while restricted, and recruitment after completion.
- Check save/load with an idle follower and a pending quest release separately.

Native and compiler tests do not establish engine/VM timing or voice playback.
