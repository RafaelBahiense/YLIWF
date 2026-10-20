#include "followers/QuestPolicies.h"
#include "combat/CombatRoles.h"
#include "adapters/Adapters.h"
#include "followers/ControllerRuntime.h"
#include "diagnostics/Debug.h"
#include "settings/Settings.h"
#include "combat/Healer.h"
#include "combat/ShieldDefender.h"
#include "followers/ControllerExecutor.h"

#include <cmath>

namespace mod::combat_roles {
    namespace {
        RE::TESFaction* marker = nullptr;

        bool HumanSlot(std::uint32_t id) {
            return id == 0 || (id >= 2 && id <= 8);
        }

        bool Registered(RE::Actor* actor, std::uint32_t aliasID) {
            auto* quest = controller::detail::GetContext().quest;
            if (!quest || !HumanSlot(aliasID))
                return false;
            for (auto* slot : quest->aliases) {
                if (!slot || slot->aliasID != aliasID)
                    continue;
                auto* reference = skyrim_cast<RE::BGSRefAlias*>(slot);
                return reference && reference->GetActorReference() == actor;
            }
            return false;
        }

        void InspectEquipment(RE::Actor* actor, Capabilities& result) {
            for (const auto& [item, entry] : actor->GetInventory(
                     [](RE::TESBoundObject& form) { return form.IsWeapon() || form.IsArmor() || form.IsAmmo(); })) {
                if (!item || entry.first <= 0)
                    continue;
                if (auto* weapon = item->As<RE::TESObjectWEAP>()) {
                    const bool oneHanded = weapon->IsOneHandedSword() || weapon->IsOneHandedDagger() ||
                                           weapon->IsOneHandedAxe() || weapon->IsOneHandedMace();
                    result.oneHanded |= oneHanded;
                    result.melee |= oneHanded || weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe();
                    result.bow |= weapon->IsBow();
                    result.crossbow |= weapon->IsCrossbow();
                } else if (auto* ammo = item->As<RE::TESAmmo>()) {
                    result.bolts |= ammo->IsBolt();
                    result.arrows |= !ammo->IsBolt();
                } else if (auto* armor = item->As<RE::TESObjectARMO>())
                    result.shield |= armor->IsShield();
            }
        }

        SpellTraits DescribeSpell(RE::SpellItem* spell) {
            SpellTraits traits;
            if (!spell)
                return traits;
            using namespace RE::MagicSystem;
            traits.castable =
                spell->GetSpellType() == SpellType::kSpell && (spell->GetCastingType() == CastingType::kFireAndForget ||
                                                               spell->GetCastingType() == CastingType::kConcentration);
            const auto delivery = spell->GetDelivery();
            traits.targetsActor =
                delivery == Delivery::kTouch || delivery == Delivery::kAimed || delivery == Delivery::kTargetActor;
            using Archetype = RE::EffectArchetypes::ArchetypeID;
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect)
                    continue;
                const auto* base = effect->baseEffect;
                const auto archetype = base->data.archetype;
                const bool modifier =
                    archetype == Archetype::kValueModifier || archetype == Archetype::kDualValueModifier;
                const bool health = base->data.primaryAV == RE::ActorValue::kHealth;
                const bool positive = effect->GetMagnitude() > 0 && !base->IsDetrimental() && !base->IsHostile();
                traits.hostile |= base->IsHostile() || base->IsDetrimental();
                traits.restoresHealth |= modifier && health && positive &&
                                         !base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kRecover);
                traits.damagesHealth |= health && base->IsHostile() && (modifier || archetype == Archetype::kAbsorb);
                traits.allyBuff |= positive && ((modifier && !health) || archetype == Archetype::kPeakValueModifier ||
                                                archetype == Archetype::kRally);
                traits.crowdControl |= archetype == Archetype::kCalm || archetype == Archetype::kDemoralize ||
                                       archetype == Archetype::kParalysis || archetype == Archetype::kTurnUndead;
            }
            return traits;
        }

        class SpellVisitor final : public RE::Actor::ForEachSpellVisitor {
        public:
            SpellVisitor(RE::Actor* actor, Capabilities& result, std::vector<HealingSpell>* healing = nullptr)
                : actor_(actor),
                  result_(result),
                  healing_(healing),
                  magicka_(actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka)) {}

            RE::BSContainer::ForEachResult Visit(RE::SpellItem* spell) override {
                const auto traits = DescribeSpell(spell);
                const bool self = spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf;
                if (traits.castable && (traits.targetsActor || self)) {
                    const auto cost = spell->CalculateMagickaCost(actor_);
                    if (traits.targetsActor)
                        AddSpell(result_, traits, std::isfinite(cost) && cost >= 0 && magicka_ >= cost);
                    if (healing_ && traits.restoresHealth && !traits.hostile &&
                        spell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget &&
                        std::isfinite(cost) && cost >= 0)
                        healing_->push_back({spell, cost, self});
                }
                return RE::BSContainer::ForEachResult::kContinue;
            }

        private:
            RE::Actor* actor_;
            Capabilities& result_;
            std::vector<HealingSpell>* healing_;
            float magicka_;
        };
    }

    void Configure(RE::TESFaction* faction) {
        marker = faction;
        shield_defender::Configure();
    }

    Role Read(RE::Actor* actor) {
        return marker && actor ? Decode(actor->GetFactionRank(marker, false)) : Role::Automatic;
    }

    std::vector<HealingSpell> HealingSpells(RE::Actor* actor) {
        std::vector<HealingSpell> result;
        if (actor) {
            Capabilities ignored;
            SpellVisitor visitor(actor, ignored, &result);
            actor->VisitSpells(visitor);
        }
        return result;
    }

    Snapshot Inspect(RE::Actor* actor) {
        Snapshot result;
        result.assigned = Read(actor);
        if (actor && result.assigned == Role::Healer)
            result.behavior = healer::Status(actor->GetFormID());
        if (actor && result.assigned == Role::ShieldDefender)
            result.behavior = shield_defender::Status(actor->GetFormID());
        if (!marker)
            result.unavailable = "Combat role record is unavailable; install the matching ESP";
        else if (!actor || actor->IsDead())
            result.unavailable = "Follower is unavailable";
        else if (adapters::Owns(actor->GetFormID()))
            result.unavailable = "This quest controller does not expose combat roles";
        else if (!quest_policies::Allows(actor, quest_policies::Action::Combat))
            result.unavailable = quest_policies::Inspect(actor).reason;
        else if (!actor->Is3DLoaded())
            result.unavailable = "Load the follower to inspect equipment and spells";
        if (!result.unavailable.empty())
            return result;

        result.available = true;
        Capabilities capabilities;
        InspectEquipment(actor, capabilities);
        SpellVisitor visitor(actor, capabilities);
        actor->VisitSpells(visitor);
        for (std::size_t i = 0; i < RoleCount; ++i) {
            const auto eligibility = Evaluate(static_cast<Role>(i), capabilities);
            result.options[i] = {eligibility.eligible, eligibility.ready, std::string(eligibility.reason)};
        }
        return result;
    }

    void Request(std::uint32_t actorID, std::uint32_t aliasID, Role role, std::uint64_t generation) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !Valid(static_cast<std::int32_t>(role)) || !debug::IsCurrentGame(generation))
            return;
        tasks->AddTask([actorID, aliasID, role, generation] {
            std::scoped_lock lock(settings::Mutex);
            if (!debug::IsCurrentGame(generation))
                return;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
            if (!actor || !Registered(actor, aliasID))
                return;
            const auto current = Inspect(actor);
            if (!current.available || !current.options[static_cast<std::size_t>(role)].eligible)
                return;
            actor->AddToFaction(marker, static_cast<std::int8_t>(role));
            actor->EvaluatePackage();
            debug::Trace("CombatRoles.Assign", actor, "role={}", Name(role));
            debug::NotifyStateChanged();
            controller::executor::Kick();
        });
    }
}
