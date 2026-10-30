# Combat tests

Optional development add-on for repeatable combat-role tests in the
[combat arena](arena.md), included in the same add-on.

## Installation

Install the matching updated YLIWF core and
`YouLeadIWillFollow-CombatTests-0.1.0.zip`. Enable its
`YouLeadIWillFollow.CombatArena.esp` and keep it with
`YouLeadIWillFollow.CombatTests.dll` and `YLIWF_CombatTests.pex`.
Restart Skyrim after installing the DLL.

## Run an encounter

1. Have other followers wait outside. Open **YLIWF → Combat tests** and click
   **Enter arena**, then close paused menus to travel with the test follower.
2. Choose an encounter under its combat role in **Test sets**.
3. Select a loaded, following core-managed follower who meets the role's
   requirements. Click **Start test** and close paused menus.

Each encounter measures 60 active seconds. **Repeat last test** uses the last
run's follower, encounter and settings. Results appear under **Last result**,
with details in **Full report**.

| Role | Encounter | Setup and observations |
| --- | --- | --- |
| Shield defender | Sustained melee | A sword opponent applies pressure. Observe stamina management, blocks, bashes and counterattack openings. Requires a one-handed weapon and shield. |
| Healer | Party pressure | A sword opponent pressures the healer while the player receives five controlled injury waves. Observe healing response, self-healing, positioning and magicka. Requires a known ally-healing spell. |

### Healer: Party pressure

The player is the injured ally. At 0, 12, 24, 36 and 48 active seconds, health is
lowered to 45% of its maximum if it is above that threshold. The runner captures
the player's original health and restores it on completion, abort or interrupted
save recovery. Stay in the arena and do not attack, cast or heal yourself.

Reports count restorative health-effect applications from the selected healer,
separately for the player and the healer. Effects can apply without restoring
useful health; sampled health gains also include regeneration. First-heal time,
time below 70% health, magicka, distance and role states help interpret the result.
No opponent attacks makes pressure observations inconclusive. Completing the
encounter confirms cleanup, not that the role performed well.

### Shield defender: comparison settings

Expand **Comparison settings** within Sustained melee to select action controls:

| Control mode | Behavior |
| --- | --- |
| Styles only | Combat-style preferences; baseline for comparison. |
| Recovery gate | Rejects selected new attack/ready/power/bash requests during recovery. |
| Guard requests | Requests shield blocking at idle opportunities and verifies blocking and release. |
| Combined | Both controls; also gates new offense during an accepted or observed guard. |

Native control modes are experimental, supported on **Skyrim 1.6.1170**, and
apply only to the selected test actor. Ordinary gameplay retains style-based
shield defense. Unsupported action-hook sites disable native modes; Styles only
remains available. Compare settings using the same follower and encounter.

## Arena travel

**Enter arena** records the player's position and facing. **Return to previous
location** returns there after the test and its restoration finish. The return
marker is saved in the SKSE co-save and removed after a confirmed return.
Travel requires a loaded, dismounted player outside combat and scenes.
Close paused menus to let travel proceed.

Entering through `coc YLIWFCombatArena` does not record a return location.
Followers use their normal following behavior during arena travel.

## Test behavior

Existing attacks finish normally. Guard requests have a cooldown and require
the follower to be idle, face a nearby threat, and retain YLIWF combat permission.
The native action validator chains its previous callee. Requests rejected by
the engine are counted separately from YLIWF rejections. Acceptance does not
prove that an animation or block occurred.
A completed rejection from the verified native processor is counted without
ending the test or taking guard ownership. Pending results, validation-only
processing and unknown processors remain unconfirmed; their recovery state is
retained for manual review instead of retrying.
If an accepted guard is not observed within 0.75 active seconds, further guard
requests are suspended for that run and one cleanup release is issued. Combat
continues after cleanup, with guard-mode results marked incomplete. A rejected
release clears ownership only when the graph is readable, blocking is false and
the engine's block-intent flag is clear. Otherwise recovery requires review;
no release is replayed.

An accepted guard must enter blocking within 0.75 active seconds. Release is
requested once and must be observed within one active second. An unconfirmed
release retains recovery state for manual review; use the pre-test save rather
than repeatedly issuing commands. The controller releases only its own guard;
an existing block or another block-start request retains external ownership.

Stand down must be off. A follower waiting outside is not eligible until following
and loaded again.

Observe without attacking or casting. Player attacks or another actor joining
invalidate the controlled run.

The runner records the follower's position, resources and role; selects Shield
defender; restores resources for setup; and places the follower and its private
opponent at fixed markers. It checks opponent damage protection, counts down,
then measures 60 active seconds. Paused menus and frozen time suspend progress.
Measurement starts after the engine confirms the opponent's sword and both
actors' combat state. Startup has a ten-active-second limit; a failure reports
the missing confirmation without replaying combat commands.
It does not add follower equipment, perks or spells.

**Abort and restore** ends a run. Completion restores the captured position,
resources and role, and removes the owned opponent and return marker. Normal AI
can move the follower again afterward. Later user role changes are preserved.
Quest restrictions can delay restoration; the panel reports this instead of
forcing movement. Inspect reported failures before starting another run.

The last report shows observed attack, block and bash starts, sampled health
loss, time under reported role statuses, and the CSV filename. CSVs are written
to the SKSE log directory. Animation starts are observations, not successful
hits or blocks. CSV rows identify startup versus measurement and include the
opponent's equipped weapon FormID; startup failure reports its name too. Brief
states between samples can be missed. No observed opponent
attacks makes the defense result inconclusive. Use **Repeat last test** to run
the same scenario again.
There is no run limit. After restoration completes, Start and Repeat are available
again. If only guard recovery remains, **Recheck guard recovery** verifies a
previously completed release without replaying commands. Close paused menus to
process the recheck. Unconfirmed dispatches remain blocked and require review.

Defender columns record requested/owned/yielded control, stance (0 Pressure,
1 Guard, 2 Recover, 3 GuardRecover, 4 Counter), combat-style and threat FormIDs,
distance, heading angle, guard commitment, recovery, stamina maximum and decision
age in seconds. A stale decision or unowned style distinguishes role intent from
live control. `combat_style` is the last observed decision's style;
`live_combat_style` reads the controller at sampling time to expose later overrides.
FormIDs are hexadecimal. The reason column is CSV-quoted.
Animation states use the engine's numeric attack-state enum.

Hit-event columns are cumulative engine events for the test pair, including
blocked, power and bash flags on incoming events. The report also shows totals.
They do not measure damage per event or guarantee one event per weapon swing;
combat mods can affect event delivery. Third-party hits invalidate the run.
Stamina differences include attacks, hits, regeneration and other effects; they
are not isolated action costs. Exact stamina costs are not inferred for combat mods.

Action columns record the mode, hook availability, validation and rejection
counts, guard/release requests, accepted requests, observed state changes,
timeouts and the last action FormID. Counts cover the test session, including
setup and restoration. Guard phases are 0 Idle, 1 StartPending, 2 Holding,
3 ReleasePending and 4 Fault. No validation observations while the actor attacks
would indicate an action path that the hook does not cover.
Dispatch columns include completed guard rejections, processor return value,
result code, flags and validation outcome. Resolved left-action counts separate
blocks, bashes and unknown events; guard requests must resolve to `blockStart`.
The validation mask records initial reject/pass (1/2), idle-selection reject/pass
(4/8) and resolved-check reject/pass (16/32). A verified pre-dispatch rejection
with result -1 is counted without claiming a guard or stopping the run.
Unknown events from other callers are preserved. A stopping row captures the final
failure before cleanup. The full report is also saved beside its CSV as a text
file. Start and Repeat are disabled with an explanation while recovery remains
pending.
Guard diagnostics include suspension, rejected releases, graph readability,
block intent, and the recovery state and stamina fraction at the last request.

The opponent uses a private dynamic NPC base with the invulnerable flag, a
one-handed sword and fixed attack-speed settings. Shared NPC records are not
edited. Its inherited weapons and ammo are removed, while armor is retained.
Unexpected death, bleedout, health loss, other actors entering, or a
change in follower eligibility invalidates the run. Scripted kills and other
combat mods can bypass ordinary damage protection; such a run is not a pass.

## Saves and limits

Use a dedicated test save. Position/resource restoration does not undo combat
events, quest reactions, crime, AI history or changes made by other mods.
Equipment remains under the actor's ordinary AI and role behavior.

An interrupted test stores its encounter, actor restore plan and captured player
health in the add-on's SKSE co-save record. An issued release is observed after
load rather than repeated. A saved pending or held guard whose current ownership cannot be
verified requires manual review; recovery does not send a new release to claim
it. The co-save callback reads native ownership state without engine
access.
Loading restores instead of restarting combat; old helper tickets are rejected.
Keep ESS/SKSE together and the add-on installed until restoration completes.
Missing references or conflicting quest ownership require review. An issued
position operation is checked rather than replayed. Unsupported bounded records
are retained and disable new tests.

The standalone UI preview simulates encounter progress without executing combat.

## Validation

Native tests cover active-time accounting, protection checks and restore-plan
validation. In-game validation must cover Start, Abort during each phase,
completion, damage-protection failure, another actor entering, a new quest lock,
and saving/loading during the test and restoration. Verify original role,
position and resources, owned-actor cleanup, CSV output and repeat runs. For Party
pressure, verify injury-wave timing, healer-attributed effects and player health
restoration after completion, abort and loading a save taken during the run.
