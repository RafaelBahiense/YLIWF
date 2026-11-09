#include "ParticipantActors.h"
#include "../testing/Actors.h"
#include "../fixtures/FixtureBases.h"
#include "../fixtures/FixtureSpells.h"
#include "../fixtures/FixtureProtection.h"
#include "../fixtures/Archetypes.h"
#include "../fixtures/NaturalOpponent.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <algorithm>
#include <array>

namespace mod::combat_tests {
    namespace {
        constexpr auto Plugin = "YouLeadIWillFollow.CombatArena.esp";
        constexpr std::array<std::uint32_t, 6> Items{0x12EB7, 0x1359D, 0x13985, 0x12FD0, 0x12FCD, 0x1C789};

        RE::Actor* Actor(std::uint32_t id) {
            return RE::TESForm::LookupByID<RE::Actor>(id);
        }

        std::uint32_t Loadout(const testing::ActorSpec& spec) {
            return spec.archetype == 6 ? 2 : spec.archetype;
        }
    }

    class ParticipantActors::Callback final : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit Callback(std::shared_ptr<Receipt> receipt) : receipt_(std::move(receipt)) {}

        void operator()(RE::BSScript::Variable value) override {
            receipt_->result.store(value.IsInt() ? value.GetSInt() : -1);
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    private:
        std::shared_ptr<Receipt> receipt_;
    };

    testing::Result ParticipantActors::Spawn(testing::Context& context, const testing::ActorSpec& requested,
                                             RE::TESObjectREFR* anchor) {
        using namespace testing;
        const bool naturalActor = requested.preserveLoadout &&
                                  requested.archetype == static_cast<std::uint32_t>(fixtures::Archetype::LeveledMage);
        if (context.scope != Scope::Case || (!naturalActor && requested.archetype >= fixture_bases::Count) ||
            (requested.preserveLoadout && (!naturalActor || requested.protectedHealth)) ||
            !std::isfinite(requested.x) || !std::isfinite(requested.y) || !std::isfinite(requested.z) ||
            !std::isfinite(requested.health) || requested.health <= 0 || requested.health >= 1000000 ||
            !std::isfinite(requested.subjectHealthFraction) || requested.subjectHealthFraction < 0 ||
            !std::isfinite(requested.minimumHealth) || !std::isfinite(requested.maximumHealth) ||
            requested.minimumHealth <= 0 || requested.maximumHealth < requested.minimumHealth ||
            (requested.role != ActorRole::Opponent && requested.role != ActorRole::Ally))
            return Result::Failed("Unsupported participant scope or archetype.");
        if (const auto* entry = participants.Find(requested.key))
            return Owns(Actor(entry->id)) ? Result{} : Result::Failed("Participant identity changed.");
        if (participants.Entries().size() >= 16)
            return Result::Failed("Participant capacity exceeded.");
        auto spec = requested;
        if (naturalActor) {
            auto* actor = natural::Spawn(anchor, context.generation);
            if (!actor)
                return Result::Failed("Could not resolve a safe leveled fire mage.");
            const auto base = actor->GetActorBase()->GetFormID();
            if (!context.resources.Own(context.scope, spec.key + "-natural", actor->GetFormID(), [] {
                    natural::CleanupSummons();
                    natural::Release();
                    return Result{};
                })) {
                actor->Disable();
                actor->SetDelete(true);
                natural::Release();
                return Result::Failed("Natural participant ownership could not be registered.");
            }
            const auto adopted = testing::AdoptActor(context.resources, context.scope, spec.key, actor, base);
            if (adopted.state != State::Ready) {
                actor->Disable();
                actor->SetDelete(true);
                return adopted;
            }
            return participants.Track(spec, actor->GetFormID(), base)
                       ? Result{}
                       : Result::Failed("Natural participant registry is full; scoped cleanup remains active.");
        }
        if (spec.subjectHealthFraction > 0) {
            auto* subject = Actor(context.subject);
            const auto health =
                subject
                    ? (subject->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kHealth) +
                       subject->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth))
                    : 0;
            spec.health = testing::ParticipantHealth(spec, health);
            if (!spec.health)
                return Result::Failed("Subject health is unavailable for participant scaling.");
        }
        if (!std::isfinite(spec.health) || spec.health <= 0 || spec.health >= 1000000)
            return Result::Failed("Participant health is outside the supported normalization range.");
        auto* base = fixture_bases::Prepare(spec.archetype);
        if (!base || !fixture_bases::Owns(base) || base->IsInvulnerable())
            return Result::Failed("Participant's private fixture base is unavailable.");
        const auto loadout = Loadout(spec);
        auto* spell = loadout >= 3 ? RE::TESForm::LookupByID<RE::SpellItem>(Items[loadout]) : nullptr;
        if (loadout >= 3 && !spell)
            return Result::Failed("Participant spell is unavailable.");
        const auto isolation = fixture_spells::RestrictBase(base, spell);
        if (!isolation.owned || !isolation.bounded || !isolation.remaining.empty())
            return Result::Failed("Participant base spell isolation failed.");
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return Result::Failed("Game data is unavailable for participant combat style setup.");
        auto* melee = data ? data->LookupForm<RE::TESNPC>(0xC3CA0, "Skyrim.esm") : nullptr;
        base->combatStyle = loadout >= 3   ? data->LookupForm<RE::TESCombatStyle>(0x831, Plugin)
                            : loadout == 2 ? data->LookupForm<RE::TESCombatStyle>(0x830, Plugin)
                            : melee        ? melee->combatStyle
                                           : nullptr;
        auto* actor = testing::SpawnActor(context.resources, context.scope, spec.key, base, anchor);
        if (!actor || !participants.Track(spec, actor->GetFormID(), base->GetFormID()))
            return Result::Failed("Could not register a participant; scoped cleanup remains active.");
        if (spec.protectedHealth) {
            const bool first = std::ranges::none_of(participants.Entries(), [&](const auto& entry) {
                return entry.id != actor->GetFormID() && entry.spec.protectedHealth;
            });
            if (!(first ? fixture_protection::Begin(actor->GetFormID()) : fixture_protection::Add(actor->GetFormID())))
                return Result::Failed("Participant health protection could not be acquired.");
        }
        actor->AsActorValueOwner()->SetBaseActorValue(RE::ActorValue::kAggression, 0);
        return {};
    }

    testing::Result ParticipantActors::Poll(Operation operation, std::uint32_t subject, std::uint32_t ticket) {
        using namespace testing;
        if (operation_ != operation) {
            if (receipt_ && receipt_->result.load() == PendingResult)
                return Result::Failed("A participant operation is still pending; it was not replayed.");
            operation_ = operation;
            cursor_ = 0;
            receipt_.reset();
        }
        if (receipt_) {
            const auto result = receipt_->result.load();
            if (result == PendingResult)
                return Result::Pending("Waiting for the participant command receipt.");
            if (result != 1)
                return Result::Failed("Participant helper rejected setup, normalization or combat start.");
            receipt_.reset();
            ++cursor_;
        }
        const auto count = participants.Entries().size() + (operation == Operation::Start ? 1 : 0);
        if (cursor_ >= count)
            return {};
        if (operation != Operation::Start && participants.Entries()[cursor_].spec.preserveLoadout) {
            ++cursor_;
            return cursor_ >= count ? Result{} : Result::Pending("Preparing next participant.");
        }
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        receipt_ = std::make_shared<Receipt>();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{new Callback(receipt_)};
        bool dispatched = false;
        if (operation == Operation::Start) {
            const auto* entry = cursor_ < participants.Entries().size() ? &participants.Entries()[cursor_] : nullptr;
            const auto source = entry ? entry->id : subject;
            const auto* selected = participants.Find(subjectTarget_);
            const auto target = entry      ? participants.Target(*entry, subject)
                                : selected ? selected->id
                                           : participants.First(ActorRole::Opponent);
            if (!Actor(source) || !Actor(target)) {
                receipt_->result = -1;
                return Result::Failed("Declared combat target is unavailable.");
            }
            if (entry && entry->spec.preserveLoadout)
                Actor(source)->EnableAI(true);
            dispatched = vm && vm->DispatchStaticCall(
                                   "YLIWF_CombatTests", "StartParticipant",
                                   RE::MakeFunctionArguments(Actor(source), Actor(target), static_cast<int>(ticket),
                                                             entry && !entry->spec.preserveLoadout),
                                   callback);
        } else {
            const auto& entry = participants.Entries()[cursor_];
            auto* actor = Actor(entry.id);
            if (!Owns(actor)) {
                receipt_->result = -1;
                return Result::Failed("Participant identity changed before setup.");
            }
            if (operation == Operation::Prepare)
                dispatched = vm && vm->DispatchStaticCall(
                                       "YLIWF_CombatTests", "ConfigureParticipant",
                                       RE::MakeFunctionArguments(
                                           static_cast<RE::Actor*>(actor), static_cast<int>(ticket),
                                           static_cast<int>(Loadout(entry.spec)), entry.spec.role == ActorRole::Ally,
                                           static_cast<float>(entry.spec.health)),
                                       callback);
            else
                dispatched = vm && vm->DispatchStaticCall("YLIWF_CombatTests", "NormalizeParticipant",
                                                          RE::MakeFunctionArguments(
                                                              static_cast<RE::Actor*>(actor), static_cast<int>(ticket),
                                                              static_cast<float>(entry.spec.health)),
                                                          callback);
        }
        if (!dispatched) {
            receipt_->result = -1;
            return Result::Failed("Participant helper dispatch failed; verify the matching PEX.");
        }
        return Result::Pending("Waiting for the participant command receipt.");
    }

    testing::Result ParticipantActors::Prepare(std::uint32_t ticket) {
        if (prepared_)
            return {};
        auto result = Poll(Operation::Prepare, 0, ticket);
        if (result.state == testing::State::Ready) {
            for (const auto& entry : participants.Entries()) {
                if (entry.spec.preserveLoadout)
                    continue;
                auto* actor = Actor(entry.id);
                const auto loadout = Loadout(entry.spec);
                auto* spell = loadout >= 3 ? RE::TESForm::LookupByID<RE::SpellItem>(Items[loadout]) : nullptr;
                const auto isolation = fixture_spells::Restrict(actor, actor ? actor->GetActorBase() : nullptr, spell);
                if (!isolation.owned || !isolation.bounded || !isolation.remaining.empty())
                    return testing::Result::Failed(entry.spec.key + ": participant spell isolation failed.");
            }
        }
        prepared_ = result.state == testing::State::Ready;
        return result;
    }

    testing::Result ParticipantActors::Normalize(std::uint32_t ticket) {
        if (normalized_)
            return {};
        auto result = Poll(Operation::Normalize, 0, ticket);
        normalized_ = result.state == testing::State::Ready;
        return result;
    }

    testing::Result ParticipantActors::Start(std::uint32_t subject, std::uint32_t ticket) {
        if (started_)
            return {};
        auto result = Poll(Operation::Start, subject, ticket);
        started_ = result.state == testing::State::Ready;
        return result;
    }

    testing::Result ParticipantActors::Pending() const {
        return receipt_ && receipt_->result.load() == PendingResult
                   ? testing::Result::Pending("Waiting for participant work before cleanup.")
                   : testing::Result{};
    }

    bool ParticipantActors::Owns(RE::Actor* actor) const {
        const auto* entry = actor ? participants.Find(actor->GetFormID()) : nullptr;
        const auto* base = actor ? actor->GetActorBase() : nullptr;
        return entry && base && base->GetFormID() == entry->base &&
               (entry->spec.preserveLoadout || fixture_bases::Owns(base)) && (entry->id & 0xFF000000) == 0xFF000000;
    }

    bool ParticipantActors::Loaded(RE::TESObjectREFR* marker) const {
        for (const auto& entry : participants.Entries()) {
            auto* actor = Actor(entry.id);
            if (!Owns(actor) || !actor->Is3DLoaded() || !marker || actor->GetParentCell() != marker->GetParentCell())
                return false;
        }
        return true;
    }

    std::string ParticipantActors::Validate(bool running) {
        for (auto& entry : participants.Entries()) {
            auto* actor = Actor(entry.id);
            if (!Owns(actor) || actor->IsDead() || actor->IsDisabled() || actor->AsActorState()->IsBleedingOut() ||
                !actor->Is3DLoaded() || actor->GetActorBase()->IsInvulnerable())
                return entry.spec.key + ": participant unavailable or downed.";
            const auto loadout = Loadout(entry.spec);
            if (entry.spec.preserveLoadout) {
                if (fixture_protection::Protected(entry.id))
                    return entry.spec.key + ": natural participant unexpectedly has health protection.";
            } else {
                const auto* equipped = actor->GetEquippedObject(false);
                if (!equipped || equipped->GetFormID() != Items[loadout])
                    return entry.spec.key + ": declared equipment is not equipped.";
                auto* expected = loadout >= 3 ? RE::TESForm::LookupByID<RE::SpellItem>(Items[loadout]) : nullptr;
                const auto spells = fixture_spells::Inspect(actor, actor->GetActorBase(), expected);
                if (!spells.owned || !spells.bounded || !spells.remaining.empty())
                    return entry.spec.key + ": unexpected spells remain.";
                if (fixture_protection::Protected(entry.id) != entry.spec.protectedHealth)
                    return entry.spec.key + ": health protection does not match its declaration.";
            }
            const auto health = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
            const auto damage =
                actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth);
            if (running) {
                const auto attacking = actor->IsAttacking();
                entry.attackStarts += attacking && !entry.wasAttacking;
                entry.wasAttacking = attacking;
                entry.healthLost += std::max(entry.previousHealth - health, 0.0f);
                if (entry.spec.protectedHealth && std::abs(damage - entry.damageModifier) > .1f)
                    return entry.spec.key + ": protected health changed.";
            }
            entry.previousHealth = health;
        }
        return {};
    }

    void ParticipantActors::Capture() {
        for (auto& entry : participants.Entries()) {
            auto* actor = Actor(entry.id);
            if (!Owns(actor))
                continue;
            entry.previousHealth = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
            entry.damageModifier =
                actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth);
        }
    }

    std::string ParticipantActors::ProbeProtection() {
        for (const auto& entry : participants.Entries()) {
            if (!entry.spec.protectedHealth)
                continue;
            auto* actor = Actor(entry.id);
            if (!Owns(actor))
                return entry.spec.key + ": protection probe ownership changed.";
            const auto health = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
            for (const bool combat : {false, true}) {
                const auto before = fixture_protection::Read();
                bool invoked = true;
                if (combat)
                    invoked = fixture_protection::ProbeCombat(entry.id, 1000000);
                else
                    actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                                                  RE::ActorValue::kHealth, -1);
                const auto after = fixture_protection::Read();
                if (!invoked || after.blocked <= before.blocked ||
                    (combat ? after.combatWrites <= before.combatWrites : after.publicWrites <= before.publicWrites) ||
                    std::abs(actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) - health) > .1f)
                    return entry.spec.key + ": health-write protection probe failed.";
            }
        }
        return {};
    }

    void ParticipantActors::Position(RE::Actor* subject) {
        if (!subject)
            return;
        for (const auto& entry : participants.Entries()) {
            auto* actor = Actor(entry.id);
            if (!Owns(actor))
                continue;
            actor->MoveTo(subject);
            auto position = subject->GetPosition();
            position.x += entry.spec.x;
            position.y += entry.spec.y;
            position.z += entry.spec.z;
            actor->SetPosition(position, true);
        }
    }

    void ParticipantActors::StopCombat() {
        for (const auto& entry : participants.Entries())
            if (auto* actor = Actor(entry.id); Owns(actor)) {
                actor->StopCombat();
                actor->InterruptCast(true);
            }
    }

    void ParticipantActors::Reset() {
        participants.Clear();
        receipt_.reset();
        operation_ = Operation::None;
        cursor_ = 0;
        prepared_ = normalized_ = started_ = false;
        subjectTarget_.clear();
    }

    std::vector<std::uint32_t> ParticipantActors::IDs(testing::ActorRole role) const {
        std::vector<std::uint32_t> result;
        for (const auto& entry : participants.Entries())
            if (entry.spec.role == role)
                result.push_back(entry.id);
        return result;
    }
}
