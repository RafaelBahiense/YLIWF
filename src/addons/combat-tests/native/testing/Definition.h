#pragma once
#include "Resources.h"
#include "Selection.h"
#include "Assessment.h"
#include <any>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <utility>

namespace mod::combat_tests::testing {
    enum class ActorRole { Opponent, Ally };

    struct ActorSpec {
        std::string key;
        ActorRole role = ActorRole::Opponent;
        std::uint32_t archetype = 0;
        bool protectedHealth = true;
        std::string target;
        float x = 0, y = 512, z = 0, health = 2000;
        float subjectHealthFraction = 0, minimumHealth = 1, maximumHealth = 999999;
        bool preserveLoadout = false;

        bool operator==(const ActorSpec&) const = default;
    };
    enum class Action {
        PrepareArena,
        PreparePlayer,
        PrepareSubject,
        Spawn,
        StaminaFraction,
        Reposition,
        InjurePlayer,
        Guard,
        Release,
        Bash,
        PowerBash,
        RightAttack
    };

    struct Command {
        Action action;
        ActorSpec actor;
        float value = 0;
        Scope scope = Scope::Case;
        std::uint32_t generation = 0;
    };

    class Context {
    public:
        using Execute = std::function<Result(const Command&)>;
        Execute execute;
        Resources resources;
        std::any configuration;
        std::any observation;
        std::optional<Completion> completion;
        float elapsed = 0, phaseElapsed = 0;
        std::uint32_t generation = 0, repetition = 0;
        std::uint32_t subject = 0;
        Scope scope = Scope::Case;

        template <class T>
        const T* Settings() const {
            return std::any_cast<T>(&configuration);
        }

        template <class T>
        const T* Observation() const {
            return std::any_cast<T>(&observation);
        }

        void Finish(Verdict verdict, std::string reason) {
            if (!completion)
                completion = Completion{verdict, std::move(reason)};
        }

        void AdvancePhase() {
            advancePhase = true;
        }

        bool advancePhase = false;

        Result PrepareArena() {
            return Invoke({Action::PrepareArena});
        }

        Result PreparePlayer() {
            return Invoke({Action::PreparePlayer});
        }

        Result PrepareSubject() {
            return Invoke({Action::PrepareSubject});
        }

        Result Spawn(const ActorSpec& actor) {
            return Invoke({Action::Spawn, actor});
        }

        Result StaminaFraction(float value) {
            return Invoke({Action::StaminaFraction, {}, value});
        }

        Result Reposition(float side) {
            return Invoke({Action::Reposition, {}, side});
        }

        Result Apply(const Command& command) {
            return Invoke(command);
        }

        std::uint32_t ActorID(const std::string& key) const {
            if (key == "subject")
                return subject;
            for (const auto lifetime : {Scope::Phase, Scope::Case, Scope::Suite})
                if (const auto id = resources.Find(lifetime, key))
                    return id;
            return 0;
        }

    private:
        Result Invoke(const Command& command) {
            auto scoped = command;
            scoped.scope = scope;
            scoped.generation = generation;
            return execute ? execute(scoped) : Result::Failed("Test execution services are unavailable.");
        }
    };

    using Hook = std::function<Result(Context&)>;

    class ActorSpawner {
    public:
        using Factory = std::function<Result(Context&, const ActorSpec&)>;

        ActorSpawner(std::string key, ActorRole role = ActorRole::Opponent, std::uint32_t archetype = 0,
                     bool protectedHealth = true)
            : spec_{std::move(key), role, archetype, protectedHealth} {}

        ActorSpawner& Archetype(std::uint32_t value) {
            spec_.archetype = value;
            return *this;
        }

        ActorSpawner& Role(ActorRole value) {
            spec_.role = value;
            return *this;
        }

        ActorSpawner& ProtectHealth(bool value = true) {
            spec_.protectedHealth = value;
            return *this;
        }

        ActorSpawner& PreserveLoadout(bool value = true) {
            spec_.preserveLoadout = value;
            return *this;
        }

        ActorSpawner& Target(std::string key) {
            spec_.target = std::move(key);
            return *this;
        }

        ActorSpawner& At(float x, float y, float z = 0) {
            spec_.x = x;
            spec_.y = y;
            spec_.z = z;
            return *this;
        }

        ActorSpawner& Health(float value) {
            spec_.health = value;
            spec_.subjectHealthFraction = 0;
            return *this;
        }

        ActorSpawner& HealthFromSubject(float fraction, float minimum, float maximum) {
            spec_.subjectHealthFraction = fraction;
            spec_.minimumHealth = minimum;
            spec_.maximumHealth = maximum;
            return *this;
        }

        ActorSpawner& Using(Factory factory) {
            factory_ = std::move(factory);
            return *this;
        }

        const ActorSpec& Spec() const {
            return spec_;
        }

        Result Spawn(Context& context) const {
            const auto result = factory_ ? factory_(context, spec_) : context.Spawn(spec_);
            if (factory_ && result.state == State::Ready && !context.resources.Find(context.scope, spec_.key))
                return Result::Failed("Actor factory completed without registering its actor for cleanup.");
            return result;
        }

    private:
        ActorSpec spec_;
        Factory factory_;
    };

    struct Variant {
        std::uint32_t id;
        std::string name;
        std::function<bool(std::any&)> configure;
    };

    struct Phase {
        std::string name;
        float seconds;
        Hook enter, tick, leave;
        std::vector<Command> enterActions;

        Phase(const char* label, float duration) : name(label), seconds(duration) {}

        Phase& Prepare(Hook hook) {
            enter = std::move(hook);
            return *this;
        }

        Phase& OnEnter(Action action, float value) {
            enterActions.push_back({action, {}, value});
            return *this;
        }

        Phase& Observe(Hook hook) {
            tick = std::move(hook);
            return *this;
        }

        Phase& Cleanup(Hook hook) {
            leave = std::move(hook);
            return *this;
        }
    };

    struct TestCase {
        std::uint32_t id;
        std::string name;
        std::string description;
        std::string subjectTarget;
        std::vector<testing::Phase> phases;
        std::vector<ActorSpawner> actors;
        std::vector<Variant> variants;
        std::vector<testing::Tag> tags;
        std::uint32_t repeats = 0;
        bool alternateVariants = false;
        Hook prepare, cleanup;
        std::any expectations;

        TestCase(std::uint32_t key, const char* label, const char* detail = "")
            : id(key), name(label), description(detail) {}

        TestCase& Prepare(Hook hook) {
            prepare = std::move(hook);
            return *this;
        }

        TestCase& Tag(testing::Tag tag) {
            if (!HasTag(tag))
                tags.push_back(std::move(tag));
            return *this;
        }

        bool HasTag(const testing::Tag& tag) const {
            for (const auto& value : tags)
                if (value == tag)
                    return true;
            return false;
        }

        template <class T>
        TestCase& Expect(T requirements) {
            expectations = std::move(requirements);
            return *this;
        }

        template <class T>
        const T* Expectations() const {
            return std::any_cast<T>(&expectations);
        }

        std::uint32_t Interventions() const {
            std::uint32_t count = 0;
            for (const auto& phase : phases)
                count += static_cast<std::uint32_t>(phase.enterActions.size());
            return count;
        }

        TestCase& SubjectTarget(std::string key) {
            subjectTarget = std::move(key);
            return *this;
        }

        TestCase& Cleanup(Hook hook) {
            cleanup = std::move(hook);
            return *this;
        }

        TestCase& Actor(ActorSpawner actor) {
            actors.push_back(std::move(actor));
            return *this;
        }

        const ActorSpec* FindActor(std::string_view key) const {
            for (const auto& actor : actors)
                if (actor.Spec().key == key)
                    return &actor.Spec();
            return nullptr;
        }

        const ActorSpec* FirstActor(ActorRole role) const {
            for (const auto& actor : actors)
                if (actor.Spec().role == role)
                    return &actor.Spec();
            return nullptr;
        }

        TestCase& Repeat(std::uint32_t count) {
            repeats = count;
            return *this;
        }

        TestCase& Variation(std::uint32_t variantId, const char* label) {
            variants.push_back({variantId, label});
            return *this;
        }

        template <class Settings, class Configure>
        TestCase& Variation(std::uint32_t variantId, const char* label, Configure configure) {
            variants.push_back({variantId, label, [configure = std::move(configure)](std::any& configuration) {
                                    auto* settings = std::any_cast<Settings>(&configuration);
                                    if (!settings)
                                        return false;
                                    configure(*settings);
                                    return true;
                                }});
            return *this;
        }

        bool ConfigureVariant(std::optional<std::uint32_t> variantId, std::any& configuration) const {
            if (!variantId)
                return true;
            for (const auto& variant : variants)
                if (variant.id == *variantId)
                    return !variant.configure || variant.configure(configuration);
            return false;
        }

        TestCase& AlternateVariations() {
            alternateVariants = true;
            return *this;
        }

        TestCase& Phase(testing::Phase phase) {
            phases.push_back(std::move(phase));
            return *this;
        }

        float Seconds() const {
            float total = 0;
            for (const auto& phase : phases)
                total += phase.seconds;
            return total;
        }
    };

    struct Suite {
        std::string name;
        std::string configurationSection = "Configuration";
        std::vector<TestCase> cases;
        Hook prepare, cleanup;

        explicit Suite(const char* label) : name(label) {}

        Suite& Configure(const char* section) {
            configurationSection = section;
            return *this;
        }

        Suite& Prepare(Hook hook) {
            prepare = std::move(hook);
            return *this;
        }

        Suite& Cleanup(Hook hook) {
            cleanup = std::move(hook);
            return *this;
        }

        Suite& Case(TestCase test) {
            cases.push_back(std::move(test));
            return *this;
        }

        const TestCase* Find(std::uint32_t id) const {
            for (const auto& test : cases)
                if (test.id == id)
                    return &test;
            return nullptr;
        }

        bool Valid() const {
            if (name.empty() || configurationSection.empty() || cases.empty() || cases.size() > 32)
                return false;
            for (std::size_t i = 0; i < cases.size(); ++i) {
                const auto& test = cases[i];
                if (test.name.empty() || test.phases.empty() || test.phases.size() > 16)
                    return false;
                for (const auto& tag : test.tags)
                    if (tag.key.empty())
                        return false;
                if (!test.subjectTarget.empty()) {
                    const auto* target = test.FindActor(test.subjectTarget);
                    if (!target || target->role != ActorRole::Opponent)
                        return false;
                }
                if (test.repeats > 100 || test.variants.size() > 8 || test.actors.size() > 16 ||
                    !std::isfinite(test.Seconds()))
                    return false;
                for (std::size_t v = 0; v < test.variants.size(); ++v) {
                    if (test.variants[v].name.empty())
                        return false;
                    for (std::size_t p = 0; p < v; ++p)
                        if (test.variants[p].id == test.variants[v].id)
                            return false;
                }
                for (std::size_t j = 0; j < i; ++j)
                    if (cases[j].id == test.id)
                        return false;
                for (const auto& phase : test.phases) {
                    if (phase.name.empty() || !std::isfinite(phase.seconds) || phase.seconds <= 0)
                        return false;
                    for (const auto& command : phase.enterActions)
                        if ((command.action != Action::StaminaFraction && command.action != Action::Reposition &&
                             command.action != Action::InjurePlayer) ||
                            !std::isfinite(command.value) ||
                            ((command.action == Action::StaminaFraction || command.action == Action::InjurePlayer) &&
                             (command.value < 0 || command.value > 1)))
                            return false;
                }
                for (std::size_t a = 0; a < test.actors.size(); ++a) {
                    const auto& actor = test.actors[a].Spec();
                    if (actor.key.empty() || actor.key == "subject" ||
                        actor.key.find_first_not_of(
                            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos ||
                        (actor.role != ActorRole::Opponent && actor.role != ActorRole::Ally) ||
                        !std::isfinite(actor.health) || actor.health <= 0 || !std::isfinite(actor.x) ||
                        !std::isfinite(actor.y) || !std::isfinite(actor.z))
                        return false;
                    if (!std::isfinite(actor.subjectHealthFraction) || actor.subjectHealthFraction < 0 ||
                        !std::isfinite(actor.minimumHealth) || !std::isfinite(actor.maximumHealth) ||
                        actor.minimumHealth <= 0 || actor.maximumHealth < actor.minimumHealth)
                        return false;
                    if (!actor.target.empty()) {
                        const auto* target = test.FindActor(actor.target);
                        if (actor.target == actor.key ||
                            (actor.target != "subject" && (!target || target->role == actor.role)) ||
                            (actor.target == "subject" && actor.role == ActorRole::Ally))
                            return false;
                    }
                    for (std::size_t b = 0; b < a; ++b)
                        if (actor.key == test.actors[b].Spec().key)
                            return false;
                }
            }
            return true;
        }
    };
}
