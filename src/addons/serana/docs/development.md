# Serana implementation

[Serana.cpp](../native/Serana.cpp) delegates commands to `DLC1_NPCMentalModelScript`.
The [add-on API](../../../../docs/follower-adapters.md) defines the host contracts.

| Command | Controller method | Requirements |
| --- | --- | --- |
| Follow | `StopWaiting()` | Active, `CanFollow`, not `LockedIn` |
| Wait | `Wait()` | Also `IsWillingToWait` |
| Dismiss | `Dismiss()` | Also `CanBeDismissed` |

VM completion schedules a main-thread state check before acknowledgement.
Quest, location, combat and death events refresh host state; periodic observation
catches other changes. Events do not establish command completion.

Recruitment eligibility requires dismissal, `CanFollow`, no `LockedIn` or
`TurnOffComeWithMe`, and compatible live bindings. Only the vanilla follower-count
gate is relaxed; her remaining dialogue conditions still apply.

Distance sets exactly one of `FollowDistanceClose`, `FollowDistanceMedium` or
`FollowDistanceFar`. AI reevaluates only on change; blocked controllers wait until
eligible. Combat protection requires following or waiting, `CanFollow` and no
`LockedIn` restriction.

`MovementPermissions` opts into core-managed formation and Stand-down packages
during ordinary following or sandboxing. Quest packages and controller locks
withdraw permission. The core handles interrupts and common movement guards;
Serana retains native distance handling. See the
[shared movement contract](../../../../docs/follower-adapters.md#core-managed-external-movement).

Owner method dispatch uses `PapyrusCommands.h`, including session invalidation
and Serana's additional dismissal verification.

## Reanimation control

Action 1 toggles combat reanimation, respecting controller restrictions. The
per-save preference defaults to allowed (`YLSR` / `REAN` v1, four-byte boolean).
Invalid records are retained and disable the control.

`Reanimation.cpp` hooks spell scoring (0x0C) and reanimation cast admission (0x06).
Combat workers compare an atomic Serana handle published on the main thread and
cleared during loading. Disabling interrupts active reanimation; spellbooks,
raised actors, other casters, scrolls, staves and scripted casts are preserved.

Validate in game: toggle before/during combat, level while disabled, switch saves,
re-enable, and check another necromancer still casts normally.
