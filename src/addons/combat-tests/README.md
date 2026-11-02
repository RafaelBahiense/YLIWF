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
   **Enter arena**. The menu closes for travel and reopens when the arena is ready.
2. Choose an encounter under its combat role in **Test sets**.
3. Select a core-managed follower, set **Repeats** (1–100), and click **Start test**.
   The runner summons distant followers, temporarily enables following when
   needed, and checks equipment and spells after loading.

Each run measures 60 active seconds and restores temporary changes before the
next repeat. A failed or aborted run stops the series. **Repeat last test** uses
the last follower, encounter and control mode with the selected repeat count.
**Full report** compares runs in a table with expandable per-run diagnostics.
The menu closes at startup and reopens on its last selected page after the series
and restoration finish.

| Role | Encounter | Setup and observations |
| --- | --- | --- |
| Shield defender | Sustained melee | A sword opponent applies pressure. Observe stamina management, blocks, bashes and counterattack openings. Requires a one-handed weapon and shield. |
| Healer | Party pressure | A sword opponent pressures the healer while the player receives five controlled injury waves. Observe healing response, self-healing, positioning and magicka. Requires a known ally-healing spell. |

### Action primitives

Under **Combat tests → Action primitives**, choose **Guard and release**,
**Normal bash**, **Power bash** or **Right-hand normal attack**, select a follower
and set **Repeats**.
Each run uses a sword opponent, temporarily assigns Automatic, and restores the
captured role and resources afterward. The follower needs a one-handed weapon
and shield; Power bash also requires the power-bash perk.

Guard and bash scenarios acquire an owned guard, hold for one second, then
request the follow-up once. Right-hand normal attack requests one attack from
an idle, nonblocking state. Each run ends on evidence, with a
20-second active-time budget. Defender threat selection, stamina budgets and
counterattack timing are bypassed; native eligibility and ownership checks remain.
**Interference settings** controls PermaBlock suppression and UND exclusion.

**Passed** requires owned action completion and successful
restoration. Rejection, interruption or missing opportunity is **Inconclusive**;
unresolved action ownership or restoration is **Failed**. A timed-out request is
never replayed. One report contains the repeated runs and their evidence.
These scenarios validate action execution; use Sustained melee and the Combined
suite to compare defensive effectiveness.

Right-hand normal attack accepts only the resolved `attackStart` action. Bash,
power, sprint and unknown selections are rejected. Completion requires a final
animation event or a witnessed native attack/recovery cycle followed by stable
idle. A power attack or bash taking over is reported as interruption. The report
and CSV separate dispatch acceptance, owned observation, cleanup and recovery
fallback; a successful animation does not establish a hit on the opponent.


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

### Combined validation suite

Under **Shield defender → Sustained melee**, enable **Combined validation suite**,
select the follower, and click **Start Combined suite**. **Runs per phase**
accepts 1–100, default 1. The suite fixes the control mode to Combined.

| Phase | Active time | Setup |
| --- | --- | --- |
| Sustained pressure | 60 s | Ordinary sword pressure. |
| Low stamina | 60 s | Begin at 15% stamina. |
| Recovery waves | 60 s | Lower stamina to 15% at 0, 20 and 40 seconds. |
| Distance closing | 60 s | Begin 1,500 units apart. |
| Repositioning | 60 s | Move the opponent to alternating sides at 20 and 40 seconds. |
| Endurance | 120 s | Continuous pressure without resource refills. |
| Heavy melee | 45 s | Greatsword opponent; record whether power windups actually occur. |
| Archer approach | 45 s | Bow opponent begins at range. |
| Projectile caster | 45 s | Firebolt opponent. |
| Sustained caster | 45 s | Flames opponent. |
| Ally protection | 45 s | Sword opponent targets a private protected passive ally. |
| Two attackers | 45 s | Two protected sword opponents start on different sides. |
| Area-capable caster | 45 s | Fireball opponent. |

One repetition takes **12¼ active minutes**, plus setup and restoration. The menu
shows the configured duration. Interference settings stay fixed. Every run
restores the follower before the next setup. A failed or aborted run, or unresolved
restoration, stops the suite. Completed results remain in one text report and CSV.

Melee baseline verdicts require clean restoration, observed guards and blocked
hits, and no unresolved action timeout. Tactical phases assess their own coverage:
actual heavy windups, reaching the archer, moving under spell pressure, approaching
the ally's attacker, or responding to two simultaneous threats. Missing coverage
is **Inconclusive**. A functional pass does not prove better defensive effectiveness;
compare damage, exposure, stamina, closing distance and ally hit events.
Low-stamina and recovery phases do not require a bash or dodge-budget decision;
those coverage requirements apply to pressure phases. Caster phases require an
actual recorded opponent cast. A phase ending with ten seconds of little movement
and no contact is inconclusive even if it exercised the expected behavior earlier.
Fixture friendly fire and opponent crossfire are reported separately and make
tactical comparisons inconclusive. Hits from outside the fixture still invalidate
the run, with actor IDs included in the reason.

The report records time in approach, guard, interrupt, recovery, counter and
repositioning, movement distance, nearest threat distance and maximum threat count.
CSV rows include the intent, movement request, threat classification and failure
counts. Precision API/callback availability, weapon capsule length and cumulative
tracked contact counts are separate from engine hit events and sampled health loss.
Movement distance includes ordinary AI and dodge movement; it does not attribute
displacement to a particular backend.
Additional CSV columns record both actors' XYZ, the movement destination, progress,
stall/detour counters, guard preparation, caster state, actual cast events and
the engine's cast-check result. Movement status values are 0 direct, 1 detour,
2 no clear candidate, 3 player-distance limit and 4 temporary yield after repeated
stalls. A selected package is not evidence that the actor reached its destination.

Ordinary attack/combo selection remains with Skyrim/SCAR. Directed UND dodge
requests and guard-specific magic protection are unavailable; the defender uses
spacing and conservative shield behavior. Bow reload state does not establish a
safe projectile gap. Fireball tests an area-capable cast, not persistent ground
hazard detection. Pressure on an ally's attacker does not guarantee a taunt.
Native guard/bash execution remains confined to the selected test actor; styles
and movement have independent per-follower state.

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

Guard and Combined also protect the handoff after a reserved external attack:
new offensive requests wait while the completed animation settles to idle.
The current action is not cancelled, and guard requests wait for confirmed cleanup.

**Suppress PermaBlock interruptions** compares the same encounter with
`FixNPCPermaBlock.dll` interruption events allowed or suppressed for the test
follower. Change it between test series without restarting. Reports count observed
`blockStop`/`bashStop` requests and suppressed notifications; zero suppressed events
does not demonstrate a successful intervention. The control leaves the mod's
timers and bookkeeping active and ends after restoration. It requires Skyrim
1.6.1170 and a verified graph hook. The preference resets when loading a save.
Implementation details are isolated in [interference controls](native/interference/README.md).

**Exclude follower from UND dodging** overrides Ultimate NPC Dodging's exclusion
and final eligibility checks for the test follower. Compare identical series with
the setting off and on; reports and CSVs record checks, forced exclusions and
denied eligibility requests. Other actors retain their normal behavior. This
requires Skyrim 1.6.1170 and the verified UND 2.4.3 DLL. The preference resets on
load. It does not cancel a dodge already executing or disable other dodge mods.

Reports include sampled dodge/recovery graph flags and animation callbacks.
CSV availability masks distinguish missing variables from false values. The text
report retains the last 1024 relevant notifications, callbacks and state/guard transitions per
run, with the latest sampled stamina and guard counters for context. Callback
timing includes wall-clock milliseconds since that sample; pauses can increase
this delay. Entries before the first sample are marked `ContextValid=0`. Callback
names and UND eligibility checks do not establish completed dodges; the
`bUND_IsDodgeRoll` flag describes dodge style. The full trace is saved in the text
report; the in-game report shows its summary.

Notification entries identify the immediate caller module and relative address,
thread, returned boolean result and whether the comparison filter suppressed
the request. They precede callbacks emitted during the call. Wrappers can hide
the original producer, and the returned result does not establish completion of
an attack or block. Guard phase, release-dispatch evidence, block intent and
native attack counters accompany each entry. Observation availability and
overwritten-entry counts are included in the CSV and report.

Native guard controls yield while `bIsDodging` is readable and true. An accepted
guard retains its acknowledgement deadline and normal release processing. After
an observed dodge interruption, guard evaluation resumes only when cleanup is
confirmed and both dodge state and block intent are clear. Unconfirmed dispatch
or release still requires review. Reports count deferred episodes, interrupted
guards and safe resumes. Ordinary style-based shield defense is unchanged.

Guard modes reserve attack ownership before Project Gap Close sprint-attack
notifications or Fix NPC PermaBlock's direct `attackStart` are forwarded.
The reservation spans the gap before the engine
reports attacking. It clears after an observed attack end or native attack-state
cycle and two idle samples, with recovery state clear when available. Rejected
inputs without start evidence withdraw their provisional reservation after two
idle samples. Accepted inputs lacking completion evidence remain yielded.
Reports count proposals, cleared reservations, guard deferrals, interruptions
and safe resumes, with separate PermaBlock attack counts. This cooperation applies only to the current test actor;
Styles only and Recovery gate do not enable it.

Owned guards wait for a readable active `IsBlockHit` reaction to finish before
sending their release, with a three-second bound on this wait. The normal
release deadline starts when the request is sent. If an accepted release has
cleared block intent but a new block-hit reaction keeps the actor blocking,
confirmed reaction time pauses that deadline for at most three cumulative
seconds. Total release observation is bounded to four seconds; no request is
repeated. Cleanup checks both the
actor's block intent and graph `iWantBlock` when available. Reports include
reaction state, graph intent, deferred releases and peak wait time.

### Defense experiments

**Comparison settings** includes two independent test controls, disabled by
default. Keep them off for the baseline and change one at a time.

- **Budget UND dodge stamina:** requires enough stamina for UND's current
  estimated dodge cost plus a reserve (default 20%). It uses the larger of
  step and roll estimates, including armor adjustments, conservatively even if
  that actor cannot roll. Permitted decisions have a default two-second cooldown.
  Reserve and cooldown are configurable. Full UND exclusion takes priority.
- **Native defensive bashes:** available in Guard and Combined against frontal
  windups within 144 units while owning a shield guard. **Use power bashes**
  selects power instead of normal bashes; engine perk and idle conditions apply.
  Both require at least 30% stamina, a 20% reserve and a five-second cooldown.
  The additional planning budget is 35 stamina for normal bashes or 60 for power
  bashes, rather than an estimate of exact modded cost. Only the selected bash
  event may dispatch. Accepted requests require observed bashing and completion;
  unknown outcomes stop the run without retrying. Reports include separate
  power-bash counters within the native-bash totals.

Owned bash acknowledgement uses actual animation events as well as sampled
native state. Reports list interrupted bashes separately from completed bashes;
the general **bash starts** count still measures sampled native-state transitions.
Completion does not guarantee a landed hit or successful interruption.

Power bashes use a standalone NPC idle supplied by the arena ESP. They require
the Power Bash perk and pass the loaded power-attack root conditions as well as
the test idle's blocking/attack-state checks. The idle is requested only for the
test follower; it is not attached to the ordinary attack tree. Install the
matching core, add-on DLL and arena ESP together.

Both controls apply only to the test follower and end with restoration. Neither
changes following distance, movement policy or normal gameplay. Dodge budgeting
requires the verified UND profile; native bashes require the matching core.

Reports include sampled health loss per active second, time below 20% stamina,
time at or below one stamina, recovery entries and live exits, native bash
request/completion counters and outgoing bash hit events. Enabled experiments
without observed coverage receive **Inconclusive**, not Passed.

Dodge episodes use readable active-dodge transitions. Surrounding net stamina
loss includes regeneration and other effects; it is not direct cost attribution.
The clean subset excludes observed hits, attacks, fixture changes, unreadable
states and episodes truncated by test end. Recovery restoration at teardown
does not count as a successful combat recovery. Downed followers have a specific
failure reason and their state is recorded in the CSV.

## Arena travel

**Enter arena** records the player's position and facing. **Return to previous
location** returns there after the test and its restoration finish. The return
marker is saved in the SKSE co-save and removed after a confirmed return.
Travel requires a loaded, dismounted player outside combat and scenes.
The menu closes automatically for both travel directions and reopens after arrival.

Entering through `coc YLIWFCombatArena` does not record a return location.
Followers use their normal following behavior during arena travel.

## Test behavior

Existing attacks finish normally. Guard requests have a cooldown and require
the follower to be idle, face a relevant threat, and retain YLIWF combat permission.
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

Stand down must be off. Quest and scene restrictions remain in effect during
summoning and restoration.

Observe without attacking or casting. Player attacks or another actor joining
invalidate the controlled run.

The runner records the follower's position, resources, role and waiting state;
selects the encounter's role; restores resources for setup; and places the
follower and its private opponent at fixed markers. It checks opponent damage protection, counts down,
then measures 60 active seconds. Paused menus and frozen time suspend progress.
Measurement starts after the engine confirms the opponent's sword and both
actors' combat state. Startup has a ten-active-second limit; a failure reports
the missing confirmation without replaying combat commands.
It does not add follower equipment, perks or spells.

During defense tests, movement uses a 4,096-unit radius around the defender's
start marker instead of the spectator's position. The bound belongs to the
active actor and run token and is cleared during teardown or game-state reset.
Normal gameplay retains its player tether.

Ranged guard preparation uses walking packages and waits for observed speed
at or below 180 units/second, no sprint and a drawn weapon. Reports include
`guard_locomotion_speed` and `guard_locomotion_ready`; package ownership alone
does not establish locomotion eligibility. Fire-and-forget spell preparation
includes startup/ready caster states while a hostile current spell is present.
CSV threat visibility, attack and preparation flags separate sight failures from
casting-state classification.

**Abort and restore** ends a run. Completion restores the captured position,
resources, role and waiting state, and removes the owned opponent and return
marker. Normal AI can move the follower again afterward. Later user role changes are preserved.
Follow and Wait changes use the core command queue and require completion
receipts. A newer controller command prevents automatic Wait restoration and
retains recovery information for review.
Quest restrictions can delay restoration; the panel reports this instead of
forcing movement. Inspect reported failures before starting another run.

The last report shows observed attack, block and bash starts, sampled health
loss, time under reported role statuses, and the CSV filename. CSVs are written
to `SKSE/YLIWF/CombatTests` under the game's log directory. Each series produces
one combined text report and one CSV, with a `run` column separating repeats.
The report is updated after each restored run and finalized when the series ends.
Animation starts are observations, not successful
hits or blocks. CSV rows identify startup versus measurement and include the
opponent's equipped weapon FormID; startup failure reports its name too. Brief
states between samples can be missed. No observed opponent
attacks makes the defense result inconclusive. Use **Repeat last test** to run
the same scenario again.
After restoration completes, Start and Repeat are available again.
If only guard recovery remains, **Recheck guard recovery** verifies a
previously completed release without replaying commands. The menu closes during
the recheck. Unconfirmed dispatches remain blocked and require review.

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
failure before cleanup. Start and Repeat are disabled with an explanation while recovery remains
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
Remaining repeats are cancelled when loading a save.
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
Also test a distant waiting follower, a multi-run series, cancelling between runs,
travel in both directions, and menu reopening after restoration. Confirm a single
combined report and CSV, with separate run numbers, in `SKSE/YLIWF/CombatTests`.
