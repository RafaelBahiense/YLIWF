# In-game testing API

Definitions follow **Suite → TestCase → Phase**. The declaration describes the
encounter; the runner supplies game-thread execution, command receipts,
subject preparation, measurements and restoration. Each suite owns its definitions:

| File | Cases |
| --- | --- |
| [`DefenderSuite.h`](../scenarios/DefenderSuite.h) | Defender behavior, protection scenarios and natural leveled opponents |
| [`HealerSuite.h`](../scenarios/HealerSuite.h) | Party pressure with five injury waves |
| [`PrimitiveSuite.h`](../scenarios/PrimitiveSuite.h) | Guard/release, normal bash, power bash and right-hand attack |

```cpp
using namespace mod::combat_tests;
using namespace testing;

const auto definition = Suite{"Shield defender"}
    .Configure("Defender tuning")
    .Prepare([](Context& ctx) {
        const auto arena = ctx.PrepareArena();
        return arena.state == State::Ready ? ctx.PreparePlayer() : arena;
    })
    .Case(TestCase{7, "Protect the archer"}
        .Prepare([](Context& ctx) { return ctx.PrepareSubject(); })
        .Actor(fixtures::Spawner("attacker", fixtures::Archetype::Sword)
            .At(0, 900).Target("archer"))
        .Actor(fixtures::Spawner("flanker", fixtures::Archetype::Heavy)
            .At(-256, 650).Target("subject"))
        .Actor(fixtures::Spawner("archer", fixtures::Archetype::ArcherAlly)
            .Role(ActorRole::Ally).Health(400).ProtectHealth(false)
            .At(0, 600).Target("attacker"))
        .Phase({"Intercept", 20})
        .Phase({"Protect under pressure", 40}));
```

Keep definitions immutable while a session references them. Names and descriptions
are owned strings. Case IDs identify cases for selection and reports; actor setup comes from their declarations.

## Responsibilities and lifetimes

| Scope | Preparation | Restoration |
| --- | --- | --- |
| Suite | Capture configuration, validate arena, position observer | Suite cleanup hook and suite resources after all cases |
| Case repetition | Capture/summon subject, acquire control, create declared actors, settle equipment/resources, start combat | End owned native actions, phase/case cleanup, remove created actors, restore subject resources/role/follow state/position |
| Phase | Run its preparation hook, then measure for its duration | Run its cleanup hook and release phase resources before the next phase |

Repetitions recreate the case encounter. Phases retain it. Phase transitions do
not respawn actors, reset equipment or replenish resources unless a hook requests
that action. The shared backend prepares the borrowed subject before running the
case's custom preparation and actor declarations.

`Prepare`, `Observe` and `Cleanup` hooks return `Ready`, `Pending` or `Failed`.
Pending hooks are polled; engine operations must retain receipts and observe
previously issued commands. A timeout or another poll does not authorize replay.
Completed hooks are not replayed while resource cleanup waits. `Observe` runs on
each measured update. A failed hook fails the case and enters restoration.

The runner advances after a failed case only when restoration succeeds. User
cancellation and unresolved ownership stop advancement. Suite participant state
is runtime-only: finish or abort a suite before saving. Loading does not resume
its actors, phases or callbacks.
## Resources and actor ownership

`Context::Spawn` creates an actor through the execution provider. The arena
provider registers the new actor immediately with its case scope. `ActorID(key)`
resolves a named resource or the borrowed `subject`; it returns zero when absent.

`Resources::Own(scope, key, id, cleanup)` registers other temporary state.
Cleanup is reverse acquisition order, with child resources before parents. A
cleanup callback reports Ready only after its operation is verified. Pending or
failed cleanup retains the journal and blocks the next case. Destruction does
not call the engine. `ResetForLoad` drops runtime callbacks as part of the host's
explicit load/revert path; it is not a substitute for restoration.

Actor cleanup checks the captured reference handle, form ID and expected base.
It issues stop/disable/deletion once, then observes disabled/deletion flags.
Unrelated arena occupants are reported by identity and are never deleted.
Borrowed subjects are restored through the shared backend rather than deleted.

`ActorSpawner` configures a participant's archetype, role, health, protection,
position and initial target. Positions are offsets from the subject's arena
start position. `HealthFromSubject(fraction, minimum, maximum)` scales the initial
maximum health from the subject. `subject` is reserved for the borrowed follower;
other keys use letters, digits, underscores and hyphens.

Each actor names its initial target with `Target(key)`. Actors on the same side
remain ineligible targets. The first declared opponent is the subject's initial
target unless `SubjectTarget(key)` selects another opponent. It remains the
focus of the detailed opponent metrics; the registry
also reports hits, attack starts and health loss for every participant.
Case IDs do not select loadouts, actor counts, teams or positions.

The arena provider supports up to 16 named case participants. Fixture archetypes
are sword, heavy melee, bow, projectile, concentration, area and archer ally.
The leveled mage spawner uses `PreserveLoadout().ProtectHealth(false)`: its native
equipment, spells, resource maxima and regeneration are retained. It participates
in the same target validation, command receipts and scoped cleanup as controlled
actors. Preserved-loadout actors are excluded from fixture normalization and
spell isolation.
The shared services prepare equipment/resources, poll each command receipt,
verify all participants, start their declared engagements and register cleanup.
Protection and observer isolation publish the complete participant sets to
worker hooks. Text reports list each actor; the CSV `participants` column records
`key:reference:currentTarget:incomingHits:outgoingHits:healthLost` entries separated
by semicolons. Existing focus-opponent columns remain available.

A custom factory can derive its spawn specification from the suite's captured
settings and delegate creation to `Context::Spawn`:

```cpp
.Actor(ActorSpawner{"attacker"}.Using([](Context& ctx, const ActorSpec& requested) {
    auto spec = requested;
    spec.archetype = ctx.Settings<MySettings>()->opponentArchetype;
    spec.target = "subject";
    return ctx.Spawn(spec);
}))
```

Factories return Ready only after their actor is registered under the declared
key. Pending factories are polled and must observe existing work rather than
create another actor. Context creation registers both the participant and its
cleanup. Custom resources use `Resources::Own`; they do not replace participant
registration. The engine-free scheduler and registry can use a different
execution provider without depending on Skyrim types.
## Phase interventions and assessment

`Phase::OnEnter(action, value)` declares stamina, reposition or player-injury interventions.
The phase runs them in order before its custom preparation hook and retains a
command cursor while a receipt is pending. Completed commands are not replayed.
`TestCase::Interventions()` derives the expected count from these declarations.

Hooks read the provider's current evidence with `Context::Observation<T>()`.
`AdvancePhase()` ends a phase early when its evidence is satisfied.
`Finish(verdict, reason)` ends the case without entering later phases. Both paths
wait for phase cleanup before advancing or completing. Case cleanup and native
action release still run, and unresolved restoration prevents a passing result.
Completion and observation state are cleared for each repetition.

`TestCase::Expect(requirements)` stores typed assessment requirements, available
through `Expectations<T>()`. The defender uses
[`DefenderAssessment.h`](../scenarios/DefenderAssessment.h):

```cpp
.Expect(suite::Requirements::Tactical({
    suite::Criterion::AllyExposure,
    suite::Criterion::Intervention,
    suite::Criterion::MultipleThreats,
    suite::Criterion::AllOpponentsEngaged
}))
```

Lifecycle failure is Failed; missing coverage is Inconclusive. The combined
protection case requires both simultaneous threats, an attack from each declared
opponent, measured ally exposure, reaching the attacker and an intervention.
Assessment uses these declared criteria rather than selecting behavior by case ID.
## Configuration and scheduling

`SuiteSession::Begin(definition, settings)` captures a copy of typed settings.
Hooks read it with `ctx.Settings<T>()`. Configuration editing belongs in the
Configuration subtab and is locked while execution or restoration is active.
`Configure` names the suite's configuration section.

CaseCatalog validates IDs and borrows the case definitions without copying
names or durations. The menu and scheduler use declaration order. IDs can be
sparse, from 0 through 31. All selection bits identify cases.

Cases declare reusable groups with `.Tag(testing::Tag{"ranged"})`.
`CaseCatalog::Tagged(tag)` selects matching cases; `NamedSelection` combines a
label, tag and variation preference. `CaseCatalog::Select(selection, repeats)`
produces `RunOptions` containing the case mask, repetitions and
`expandVariations` flag.

`RunPlan(catalog, options)` schedules the selected cases.
`TestCase::Repeat(n)` overrides the inherited count.
`RunPlan::CaseRuns(id)` reports its actual repeat/variation count.
`Variation<Settings>(id, label, configure)` declares a typed settings override;
`Variation(id, label)` declares a variation without changing settings.
`AlternateVariations()` reverses their order on alternating repetitions.
`SuiteSession` applies each override to a fresh copy of the captured settings
before preparing the encounter. Configuration changes do not carry into the
next repetition or case.

Challenge on/off comparisons use parameterized repetitions. Case IDs connect
selection and reporting to the declared cases.

## Validation

Engine-free tests cover partial preparation, fresh actors per repetition,
configuration capture, scoped cleanup, pending restoration, phase transitions,
and variation order. The preview renders the case/phase UI. Loading, actor
disposal, borrowed-subject restoration and VM scheduling still require in-game
verification. Keep one aggregate report for the complete suite; report incomplete
phases and failed cases alongside successful ones.
