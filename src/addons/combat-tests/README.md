# Combat tests

Optional development add-on for repeatable combat-role tests in the
[combat arena](arena.md), included in the same add-on.

## Installation

Install the matching updated YLIWF core and
`YouLeadIWillFollow-CombatTests-0.1.0.zip`. Enable its
`YouLeadIWillFollow.CombatArena.esp` and keep it with
`YouLeadIWillFollow.CombatTests.dll` and `YLIWF_CombatTests.pex`.
Restart Skyrim after installing the DLL.
Controlled-opponent scenarios require Skyrim **1.6.1170**. The health-write
profile validates its callsites before enabling protection.

## Run an encounter

1. Have other followers wait outside. Open **YLIWF → Combat tests** and click
   **Enter arena**. The menu closes for travel and reopens when the arena is ready.
2. Open **Scenarios** and choose an encounter under its combat role.
3. Select a core-managed follower and set the repeat count (1–100).
   For **Defender validation**, select cases and click **Start validation**.
   Other encounters use **Start test**.
   The runner summons distant followers, temporarily enables following when
   needed, and checks equipment and spells after loading.
   Startup waits up to 30 active seconds for existing scenes or combat to finish.

Phase durations appear in the selector; other encounters have a 60-second limit.
Each run restores temporary changes before the next repeat.
Defender validation records failed runs and continues to the next scheduled run
after restoration, including startup failures. Cancellation or unresolved cleanup
stops the series. A defeated follower also requires observed bleedout recovery;
recovery waits up to 30 active seconds. Death or an unavailable subject stops testing.
**Repeat last test** uses
the last follower and encounter with the selected repeat count.
**Full report** compares runs in a table with expandable per-run diagnostics.
The menu closes at startup and reopens on its last selected page after the series
and restoration finish.
Defender tests place the player at the observer marker. Use normal camera
controls to watch. The previous position is restored after the series when the
player remains at that marker; manual movement takes priority. Healer tests
keep the player in their scenario position.

| Role | Encounter | Setup and observations |
| --- | --- | --- |
| Shield defender | Defender validation | Select melee, recovery, ranged or party-protection cases. Defender acceptance covers nine scenarios and a paired Challenge comparison. Requires a one-handed weapon and shield. |
| Shield defender | Defender validation → Additional cases → Leveled mage encounter | A normal leveled fire mage retains its spells, perks, equipment and resources. Measure victory, defeat or survival to the 60-second limit. |
| Healer | Party pressure | A sword opponent pressures the healer while the player receives five controlled injury waves. Observe healing response, self-healing, positioning and magicka. Requires a known ally-healing spell. |

### Protection review

In **Shield defender → Defender validation**, choose **Protection review**.
With **Runs per case = 1**, the series takes 4 minutes 30 seconds of combat:
two Ally protection runs (Challenge on/off), then Archer approach, Projectile
caster, Sustained caster and Two attackers. Setup and restoration add time.

**Run declared variations** doubles Ally protection with Challenge on/off. Each repetition is
a pair; order alternates on/off, then off/on. Other cases retain the configured
Challenge setting. The comparison report shows ally damage, targeting time,
intervention time and defeats. Pairs with different starting equipment, resources
or opponent base are excluded from the mean differences and remain in the report.
Controlled opponents start with 2,000 actual maximum health and magicka after
ability modifiers are accounted for. Ally health is captured once per series.
Equipment and effects settle before final normalization and a separate stability
check. Combat startup uses the prepared actors without resetting their loadouts.
Reports compare the prepared resource and equipment baseline. Combat-time
changes remain outcome evidence. Normal leveled encounters retain their original stats.

Engagement measurements separate first entry within 256 units, first outgoing hit
and damage taken outside that range. Casting-line exits measure movement out of
an estimated corridor anchored to the caster's initial facing during an observed
casting episode. They do not establish projectile evasion or ground-hazard safety.

### Action primitives

Under **Combat tests → Scenarios → Action primitives**, choose **Guard and release**,
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
These scenarios validate action execution; use **Defender validation** to assess
defensive effectiveness.

Right-hand normal attack accepts only the resolved `attackStart` action. Bash,
power, sprint and unknown selections are rejected. Completion requires a final
animation event or a witnessed native attack/recovery cycle followed by stable
idle. A power attack or bash taking over is reported as interruption. The report
and CSV separate dispatch acceptance, owned observation, cleanup and recovery
fallback; a successful animation does not establish a hit on the opponent.


### Shield defender: Leveled mage encounter

Choose **Shield defender → Defender validation → Additional cases → Leveled mage encounter**.
Set **Runs per case** and adjust defender tuning under **Configuration**.

The engine resolves Skyrim's `LCharWarlockFire` leveled list using the installed
records. The mage keeps its normal spells, perks, equipment, combat style,
magicka and regeneration. It can cast from either hand, run out of resources,
switch weapons and die. Spell distribution remains active.

The resolved NPC base is reused across repeats and subsequent comparisons in
the same game session. **Roll new opponent** clears that choice between tests.
Equipment rolls and distributed spells can still vary; compare the recorded
starting spells and stats before attributing a difference to defender tuning.

Results distinguish **Victory**, **Defeat**, **Time limit**, **Cancelled** and
**Invalid test**. A restored defeat continues to the next repeat. Setup, native
action and restoration failures remain invalid tests. Summons belonging to the
participants are allowed; tracked generated summons are removed during cleanup.
Reports identify the resolved NPC, level, starting resources and spells. CSVs
include magicka and both hands' current casting states and spell IDs.

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

### Defender validation

The **Shield defender** suite contains selectable test cases. **Runs per case**
repeats the entire encounter with fresh opponents. A case can contain consecutive
phases that share the same actors: Recovery waves has three 20-second phases;
Repositioning has front, left and right pressure phases. The live view and full
report show phase progress. Temporary actors are cleaned up after each case
repetition; the player returns after the suite ends.

For native scenario definitions and lifecycle contracts, see
[the testing API](native/testing/README.md).

Under **Scenarios → Shield defender → Defender validation**, select the follower
and click **Start validation**. **Runs per case** accepts 1–100, default 1.
The default **Defender acceptance** selection runs ordinary melee, heavy melee,
low stamina, paired ally protection with Challenge on/off, archer approach,
projectile, sustained and area-capable casters, and two attackers: **10 runs,
7 minutes 45 seconds** of measured combat per repetition, plus setup and restoration.
**Defender essentials** selects the four melee/recovery/protection scenarios.

Use **Select cases** to choose one scenario or a batch. **Defender essentials**,
**Defender acceptance**, **All cases**, **Ranged and magic**, **Protection review**
and **Clear selection** provide shortcuts. Acceptance scenarios appear directly;
expand **Additional cases** for longer recovery, movement and endurance cases.
Selected cases keep their listed order; the displayed run count and duration
reflect the selection. At least one case is required. **Repeat validation**
retains the last run's case selection. All defender cases use native guard
control, recovery gating and tactical bash selection.

### Directed evasion and projectile timing

With **Configuration → Allow UND dodging** enabled, the verified UND 2.4.3/TK
backend can execute chosen backward or lateral steps. YLIWF checks ground and
clearance, releases its guard, keeps a 20% stamina reserve and waits two seconds
between requests. UND owns animation stamina costs and immunity. Missing graph
acknowledgement disables directed requests for that run; ordinary defense remains.
Reports distinguish requests, graph acknowledgement and completed displacement
in the requested direction. Exclusion disables both autonomous and directed dodges.

Archer approach lowers the shield after a readable quiet projectile path and
raises it for observed windups or incoming shots. The bounded scan uses current
projectile positions/velocity; it does not predict future shots or certify safety
against homing projectiles or persistent ground hazards. CSVs include scan
availability, quiet-path time and incoming-projectile pressure. Retreat waypoint
selection considers separation and the facing arc of all observed attackers.

### Runtime defender tuning

**Configuration → Enable Challenge (taunt)** controls taunting for the next
series. Turn it off to compare ally protection without Challenge. The setting
is captured in each report and can be saved with the tuning profile.

Under **Configuration**, open **Defender tuning**, edit a named profile, then
click **Apply profile**.
Every repetition in the next series uses the captured values. Editing is locked
during testing, travel and retained recovery. Each text report and CSV records
the profile name, revision and parameter values.

The controls cover approach/retreat/lateral step lengths, movement burst/rest/
refresh intervals, stamina recovery thresholds, and guard speed/settling limits.
**Baseline values** fills the editor; **Apply profile** activates it.
**Discard edits** restores the applied profile in the editor.

**Save applied profile** writes `SKSE/YLIWF/CombatTests/defender-tuning.ini` under
the SKSE log directory. Edit that file while Skyrim stays open, then use
**Reload profile file** between series. Invalid or incomplete files retain the
applied profile. Loading assigns a new session revision.

```ini
schema=1
name=Baseline
approach_step=384
retreat_step=256
lateral_step=256
movement_burst=2
movement_rest=0.6
movement_refresh=0.35
recovery_enter=0.4
recovery_exit=0.65
guard_speed=180
guard_settle=0.2
```

The panel shows allowed ranges. Recovery exit must exceed entry by at least
0.05; movement refresh and guard settling must fit within the movement burst.
Profiles apply to the test subject until teardown. Their tuning does not change
native action acknowledgement or timeout contracts.

### Suite cases

| Phase | Active time | Setup |
| --- | --- | --- |
| Ordinary melee | 45 s | Ordinary sword pressure. |
| Heavy melee | 45 s | Greatsword opponent; record whether power windups actually occur. |
| Low stamina | 60 s | Begin at 15% stamina. |
| Ally protection | 45 s | Sword opponent engages a closer archer ally with finite health. |
| Recovery waves | 60 s | Lower stamina to 15% at 0, 20 and 40 seconds. |
| Distance closing | 60 s | Begin 1,500 units apart. |
| Repositioning | 60 s | Move the opponent to alternating sides at 20 and 40 seconds. |
| Endurance | 120 s | Continuous pressure without resource refills. |
| Archer approach | 45 s | Bow opponent begins at range. |
| Projectile caster | 45 s | Firebolt opponent. |
| Sustained caster | 45 s | Flames opponent. |
| Two attackers | 45 s | Two protected sword opponents start on different sides. |
| Protect against two attackers | 45 s | Two attackers pressure a finite-health archer ally; report all three participants. |
| Area-capable caster | 45 s | Fireball opponent. |

Selecting every case takes **12 active minutes** per repetition, plus setup and restoration. The menu
shows the configured duration. Interference settings stay fixed. Every run
restores the follower before the next setup, including the defeat recovery described
above. Failed runs remain in the text report and CSV with their original reason;
continuing does not change their verdict.

### Ally protection

The defender starts 900 units from a sword opponent. An archer ally starts
300 units from that opponent, between it and the defender, and fights back.
The ally's base health is set to 75% of the defender's maximum health, bounded to 300–900 HP. It is
killable and has no invulnerability, essential or protected flag. Ally defeat
fails the run; cleanup completes before remaining repetitions continue.

The arena target filter rejects deliberate friendly targets, including participant
summons. Area effects can still cause friendly fire and make comparisons
inconclusive. The runner starts combat once; it does not repeatedly force aggro.
Reports record ally starting/remaining health, sampled health loss, time targeted
by the opponent, and the first defender hit or enemy attention shift. Survival
and approach alone do not establish a useful intervention.

Melee fixture verdicts require clean restoration, observed guards and blocked
hits, and no unresolved action timeout. Tactical cases assess their own coverage:
actual heavy windups, reaching the archer, moving under spell pressure, approaching
the ally's attacker, or responding to two simultaneous threats. Missing coverage
is **Inconclusive**. A functional pass does not prove better defensive effectiveness;
compare damage, exposure, stamina, closing distance and ally hit events.
The low-stamina case requires a fresh recovery observation below the applied
entry threshold, followed by an observed exit at or above the exit threshold
during measured combat. Setup refills, missing observations and teardown cannot
complete recovery. Heavy melee requires
a completed native bash and, when dodge budgeting is enabled, a budget decision.
Other melee cases do not require those opportunities. Caster cases require an
actual recorded opponent cast. A case ending with ten seconds of little movement
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
hazard detection.
Against projectile and sustained casts, a defender with sufficient stamina can
advance diagonally along a clear path, then pressure the caster in melee. A close
hostile casting windup permits a normal bash when facing, guard ownership and
resource checks pass. Recovery and area-capable threats retain conservative
spacing. Caster-bash completion and actual spell interruption are separate evidence.
Native guard/bash/challenge execution remains confined to the selected test actor; styles
and movement have independent per-follower state.

### Defender's Challenge

Interception uses an offensive combat style while an enemy threatens an ally.
Recovery and immediate defense retain priority. Controlled opponents have explicit
hostile faction relations with the defender and ally; startup rejects missing
mutual hostility after a bounded settling period. Generated opponents become
Aggressive at combat startup; the subject and ranged ally retain their settings.
Reports record both hostility directions, Precision pre-hit and
post-hit counts, and collision-body availability to distinguish targeting from contact.

Defender's Challenge targets a visible hostile within 512 units. It costs 20
stamina, preserves a 20% stamina reserve, requires at least 40% health and is
unavailable during recovery. Its duration is four seconds with a twelve-second
cooldown. The zero-damage lesser power temporarily constrains the enemy's target
selection while preserving the engine's target eligibility checks.

Acknowledgement requires both the applied effect and the enemy targeting the
defender. An unacknowledged request releases its constraint after two seconds and
retains its cooldown. Ending the test also releases control. Challenge hit events
are counted separately from physical hits. The skill does not restart combat or
restore an old target when it expires; normal combat AI resumes target selection.

### Spell application diagnostic

Open **Combat tests → Diagnostics** in the arena, select a follower and
choose **Run application diagnostic**. The caster is summoned even when unloaded,
then returned to the captured location afterward. The caster must be out of combat. Four
isolated casts compare Challenge and a two-second water-breathing control with
target invulnerability enabled and disabled. Each case casts once and observes
for two active seconds; the complete comparison takes about ten seconds plus
target loading. The menu closes for the test and reopens afterward.

The diagnostic temporarily pauses the caster's AI, uses a dedicated disposable
target, and restores location, AI and target-base protection on completion or abort.
Combat scenarios and arena travel are blocked while it runs. Reports appear in
`SKSE/YLIWF/CombatTests/YLIWF-SpellDiagnostic-*.txt`, with a summary in the menu.
Each case records caster eligibility/rejection reason, `HasMagicEffect`, matching
active spell/effect entries, caster identity, flags and the production detector's
result. Missing effect evidence is distinct from an unacknowledged target switch.
Casting eligibility alone does not prove target acceptance, and this diagnostic
does not establish combat effectiveness.

### Shield defender controls

Defender scenarios combine combat-style preferences, native guard requests and
offense gating during recovery or an owned guard. Native action controls are
experimental, supported on **Skyrim 1.6.1170**, and apply only to the selected
test actor. Unsupported action-hook sites prevent the test from starting.
Ordinary gameplay retains style-based shield defense.

New offensive requests wait while a reserved external attack settles to idle.
The current action is not cancelled, and guard requests wait for confirmed cleanup.
During recovery, new offensive requests remain blocked even while an external
attack finishes. Safe recovery gaps also decline unsolicited shield-raise requests;
owned emergency guards and release processing retain their normal lifecycle.
**Configuration** holds shared dodge, interference and tuning controls. Changes
apply to subsequent tests and repetitions, only for the selected test actor.

**Suppress PermaBlock interruptions** compares the same encounter with
`FixNPCPermaBlock.dll` interruption events allowed or suppressed for the test
follower. Change it between test series without restarting. Reports count observed
`blockStop`/`bashStop` requests and suppressed notifications; zero suppressed events
does not demonstrate a successful intervention. The control leaves the mod's
timers and bookkeeping active and ends after restoration. It requires Skyrim
1.6.1170 and a verified graph hook. The preference resets when loading a save.
Implementation details are isolated in [interference controls](native/interference/README.md).

Turn **Allow UND dodging** off to override Ultimate NPC Dodging's exclusion
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

Defender tests reserve attack ownership before Project Gap Close sprint-attack
notifications or Fix NPC PermaBlock's direct `attackStart` are forwarded.
The reservation spans the gap before the engine
reports attacking. It clears after an observed attack end or native attack-state
cycle and two idle samples, with recovery state clear when available. Rejected
inputs without start evidence withdraw their provisional reservation after two
idle samples. Accepted inputs lacking completion evidence remain yielded.
Reports count proposals, cleared reservations, guard deferrals, interruptions
and safe resumes, with separate PermaBlock attack counts. This cooperation applies only to the current test actor.

Owned guards wait for a readable active `IsBlockHit` reaction to finish before
sending their release, with a three-second bound on this wait. The normal
release deadline starts when the request is sent. If an accepted release has
cleared block intent but a new block-hit reaction keeps the actor blocking,
confirmed reaction time pauses that deadline for at most three cumulative
seconds. Total release observation is bounded to four seconds; no request is
repeated. Cleanup checks both the
actor's block intent and graph `iWantBlock` when available. Reports include
reaction state, graph intent, deferred releases and peak wait time.

### Dodge budget and defensive bashes

- **Budget dodge stamina:** enabled by default when the installed UND is supported.
  Requires enough stamina for UND's current
  estimated dodge cost plus a reserve (default 20%). It uses the larger of
  step and roll estimates, including armor adjustments, conservatively even if
  that actor cannot roll. Permitted decisions have a default two-second cooldown.
  Reserve and cooldown are configurable. During recovery, discretionary dodges
  are declined; imminent heavy attacks and hostile casting retain eligibility
  subject to the same reserve and cooldown. Disabling UND dodging takes priority.
- **Defensive bashes:** respond to frontal windups within 144 units while owning
  a shield guard. Heavy melee windups use power bash when affordable and eligible
  under the engine's perk and idle conditions; ordinary bash is the fallback.
  Recovery suppresses tactical bash requests.
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
follower and its generated opponent at fixed markers. Controlled fixtures check
opponent damage protection before the countdown. Runs measure up to 60 active
seconds; suite cases have their own durations. Paused menus and frozen time
suspend progress. Measurement starts after the engine confirms the required
loadout and both actors' combat state. Startup has a ten-active-second limit; a failure reports
the missing confirmation without replaying combat commands.
It does not add follower equipment, perks or spells.

An actor-scoped follower policy holds the subject at its arena marker during
setup. During combat it selects eligible core role packages, with an arena
anchor as the fallback instead of ordinary player following. Scenes and quest
restrictions retain priority. The policy is withdrawn during restoration;
the original following/waiting state is restored separately. CSV columns
`subject_package` and `requested_package` show actual and requested package IDs.
`subject_in_combat` and `opponent_in_combat` distinguish package handoffs from
leaving combat. A `combat-ended` sample captures both actors before restoration.
`subject_combat_ignoring`, `subject_combat_inactive` and `subject_combat_stopped`
record combat-controller flags; -1 means no controller was available.
Setup uses Travel/Wait at the start marker. The combat fallback uses Skyrim's
combat-capable HoldPosition procedure with a 4,096-unit arena radius and no
preferred walking speed.

Combat override lists also contain faction-gated arena anchors ahead of
Skyrim's player-follow fallback. Only the test subject receives that faction,
which is removed during restoration. Earlier quest and role overrides keep
their order. A scoped target-eligibility filter excludes the observing player
from the opponent's targets, including its detected summons; combat between
participants still uses the engine's normal eligibility checks. Area damage
can still hit the player, so observe from outside the fight.

CSV fields `subject_target`, `opponent_target`, `auxiliary_target`,
`observer_free_camera`, `observer_positioned` and `observer_target_rejections`
record targeting and rejected observer checks. An opponent casting at the
observer invalidates a run just as a melee attack against the observer does.

During defense tests, movement uses a 4,096-unit radius around the defender's
start marker instead of the spectator's position. The bound belongs to the
active actor and run token and is cleared during teardown or game-state reset.
Normal gameplay retains its player tether.

Ranged guard preparation uses walking packages and waits for observed speed
within the profile's guard limit (default 180 units/second), no sprint and a drawn weapon. Reports include
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

Controlled fixtures use dedicated NPC bases defined in the arena ESP,
the scenario's weapon or spell loadout and fixed attack-speed settings.
An actor-scoped health-damage filter keeps controlled opponents alive while
allowing hostile effects to apply. It filters negative Health damage modifiers
at verified direct engine write calls, forwarding a zero delta to the existing
actor-value backend for those opponents. Healing, other actor values and other
actors remain unchanged. The [health-write backend](native/integrations/README.md)
owns the runtime profile and validation contract.
The finite-health ally and leveled mage encounter are excluded. Startup verifies
protection with damage probes, and reports include suppressed calls and amounts.
Its inherited weapons and ammo are removed, while armor is retained.
Spell isolation preserves passive/constant effects and the selected fixture spell.
Other castable spells and powers are removed from the dedicated base before
spawning and from generated actors before combat setup. Non-hostile, self-targeted
spells containing only night-vision effects are retained as utility. Mixed-effect
and other castable spells remain subject to isolation. Shared template and race
records remain read-only.

Restoration interrupts casting, stops combat, disables opponents and marks them
for deletion. Opponents are spawned without forced persistence. Any deleted
reference retained by the engine still resolves to its ESP-defined NPC base.

Controlled fixture startup and measurement check for unexpected spells, including readied spells.
Late distribution invalidates the run rather than changing its loadout during
measurement. Reports list removed spell IDs/names and isolation counts; CSVs
include `fixture_spells_removed`, `fixture_spell_passes` and
`fixture_unexpected_spells`.
In controlled fixtures, unexpected death, bleedout, health loss, other actors entering, or a
change in follower eligibility invalidates the run. Scripted kills and other
combat mods can bypass ordinary damage protection; such a run is not a pass.

## Saves and limits

Suite runs use runtime participant state. Finish or abort the suite before saving;
loading does not resume its participants or phases.

Use a dedicated test save. Position/resource restoration does not undo combat
events, quest reactions, crime, AI history or changes made by other mods.
Equipment remains under the actor's ordinary AI and role behavior.

The standalone UI preview simulates encounter progress without executing combat.

## Validation

Native tests cover active-time accounting, protection checks and restore-plan
validation. In-game validation must cover Start, Abort during each phase,
completion, damage-protection failure, another actor entering, a new quest lock,
and starting a new repetition after restoration. Verify original role,
position and resources, owned-actor cleanup, CSV output and repeat runs. For Party
pressure, verify injury-wave timing, healer-attributed effects and player health
restoration after completion and abort.
Also test a distant waiting follower, a multi-run series, cancelling between runs,
travel in both directions, and menu reopening after restoration. Confirm a single
combined report and CSV, with separate run numbers, in `SKSE/YLIWF/CombatTests`.
