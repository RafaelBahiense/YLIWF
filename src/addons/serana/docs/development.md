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

[TravelFormation.cpp](../native/TravelFormation.cpp) applies temporary host packages
during ordinary following or sandboxing, without overriding Dawnguard records,
scripts or aliases. Quest packages, foreign interrupts, scenes, waiting and combat
take priority over formations. Stand down permits existing combat but retains
the quest and scene restrictions. Controller commands release these movement
overrides before dispatch.
