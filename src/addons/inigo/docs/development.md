# Inigo controller integration

The external adapter uses the existing `InigofollowerDialogue` script on quest
`0012DF` in `Inigo.esp`. It does not claim core aliases or replace controller scripts.

| YLIWF command | Owner method | Observed result |
| --- | --- | --- |
| Follow | `FollowMe()` | `WaitingForPlayer = 0` |
| Wait | `WaitHere()` | `WaitingForPlayer = 1` |
| Dismiss | `Dismiss()` | `WaitingForPlayer = -1` |

Inigo retains teammate status after dismissal. The adapter therefore uses the
waiting value to identify inactivity. Relaxing (`2`) maps to Waiting and is
preserved by another Wait command. Unknown values or missing active teammate
status disable commands without repair.

Inspection verifies the controller's actor and distance-faction properties, the
InigoStatus quest's alias 1, and all three zero-argument methods. Commands require
completed initial recruitment (dialogue stage 10), running controller/status quests,
no current scene, an ordinary following/waiting/relaxing package, and no Bad
Vibrations stage-140 restriction. Quest movement and Inigo's no-movement package
retain priority. The host's periodic observation discovers changes made outside YLIWF.

VM callbacks schedule main-thread inspection before reporting completion. Loading
or starting a game invalidates old callbacks; dispatch is never replayed. Pending
commands release temporary movement and block reapplication until their VM callback
finishes. Old callbacks cannot clear the pending state of a different game session.

## Movement

Ordinary on-foot following uses the core's existing distance packages (`000990`–
`000992`), giving the same Close (128–192), Normal (256–384) and Far (512–768)
radii. The adapter also mirrors the native near/far preference in faction `04EA5E`
for owner-controlled movement, including horses. Waiting values and recruitment
state are preserved.

The adapter opts into core distance, formation and Stand-down movement.
The core accepts only its own movement packages.
Formation movement takes precedence over the selected distance; releasing a
formation returns to distance following when eligible. Both use reversible
interrupt packages. Only recognized core follow/formation/stand-down interrupts on Inigo are
released; foreign interrupts remain untouched. No Inigo package record is edited.

Movement requires ordinary on-foot package `050AED`, active following, live controller
bindings, normal locomotion and no quest lock, scene, dialogue, combat or pending
command. Mounting, swimming and configured horse movement suspend overrides. The
underlying package is checked independently of the temporary interrupt, so quest
movement can revoke eligibility. Replacing an interrupt rechecks permission after
reevaluation.

Core Stand-down movement accepts only the four passive and thirty-two avoidance
packages (`000A11`–`000A14`, `000A20`–`000A3F`). They take priority over travel
preferences and allow combat while retaining controller, scene, locomotion,
horse and foreign-interrupt restrictions. Ordinary waiting and relaxing packages
are also eligible; releasing Stand down never resumes a waiting follower. The
core owns combat suppression, stopping combat and avoidance destinations. The
adapter owns the reversible package interrupt and releases it when eligibility
is revoked or a controller command starts.

The core's shared movement update replaces add-on polling. It releases revoked
movement and reapplies eligible preferences; no add-on thread or serialization
format is needed. `PapyrusCommands.h` handles method dispatch, pending work and
session invalidation. See the
[shared movement contract](../../../../docs/follower-adapters.md#core-managed-external-movement).

## Validation

Rules tests cover follower states, retained teammate status after dismissal,
restrictions, invalid values and distance mapping. Runtime behavior still needs
in-game validation:

1. Recruit Inigo through his dialogue alongside a core follower. Check the roster
   and core slot count.
2. Exercise individual and party Follow, Wait and Dismiss, then recruit him again
   through his own dialogue. Verify a dismissed teammate leaves the active roster.
3. Ask him to relax, verify Waiting, then resume with Follow.
4. Compare Close/Normal/Far against a core follower on open ground. Repeat while
   waiting and confirm that changing distance never resumes following.
5. Start an Inigo scene and exercise his quest restrictions. Controls should pause
   until ordinary behavior resumes.
6. Save/load in following, waiting and dismissed states; verify rediscovery and
   that old commands are not repeated.
7. Test Column, Line and Wedge outside settlements. Disable formations and confirm
   distance following resumes. Enter combat, interiors, settlements and dialogue;
   formation eligibility should match the core's travel restrictions.
8. Test mounted travel, independent horse riding and a foreign interrupt package.
   Inigo's controller should retain those activities, then return to on-foot
   spacing when ordinary following resumes.
9. Enable Stand down during combat and while waiting or relaxing. Check that
   Inigo stops attacking, follows passively or stays put as appropriate, and
   yields to scenes and quest movement. Return to normal behavior and verify
   combat and eligible distance/formation preferences resume. Save/load with
   Stand down active and confirm the temporary order is cleared.
