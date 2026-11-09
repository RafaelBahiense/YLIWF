#include "FixtureSpells.h"
#include "FixtureBases.h"
#include "FixtureSpellRules.h"
#include <algorithm>
#include <array>

namespace mod::combat_tests::fixture_spells {
    namespace {
        constexpr std::size_t MaximumSpells = 4096;
        std::array<const RE::EffectSetting*, 2> scriptedVision{};

        bool Dynamic(const RE::TESForm* form) {
            return form && (form->GetFormID() & 0xFF000000) == 0xFF000000;
        }

        bool Private(RE::TESNPC* base) {
            return fixture_bases::Owns(base);
        }

        Kind Classify(const RE::SpellItem* spell) {
            using Type = RE::MagicSystem::SpellType;
            switch (spell->GetSpellType()) {
                case Type::kSpell:
                    return Kind::Spell;
                case Type::kPower:
                    return Kind::Power;
                case Type::kLesserPower:
                    return Kind::LesserPower;
                case Type::kVoicePower:
                    return Kind::VoicePower;
                case Type::kScroll:
                    return Kind::Scroll;
                case Type::kStaffEnchantment:
                    return Kind::Staff;
                case Type::kAbility:
                case Type::kDisease:
                case Type::kAddiction:
                case Type::kPoison:
                    return Kind::Passive;
                default:
                    return Kind::Other;
            }
        }

        bool Unwanted(const RE::SpellItem* spell, const RE::SpellItem* expected) {
            if (!spell || !Remove(spell->GetFormID(), Classify(spell),
                                  spell->GetCastingType() == RE::MagicSystem::CastingType::kConstantEffect,
                                  expected ? expected->GetFormID() : 0))
                return false;
            if (spell->effects.size() > MaximumSpells)
                return true;
            std::vector<Effect> effects;
            effects.reserve(spell->effects.size());
            for (const auto* entry : spell->effects) {
                const auto* effect = entry ? entry->baseEffect : nullptr;
                const auto kind = !effect                                                    ? EffectKind::Other
                                  : effect->GetArchetype() == RE::EffectArchetype::kNightEye ? EffectKind::NightVision
                                  : std::ranges::find(scriptedVision, effect) != scriptedVision.end() &&
                                          effect->GetArchetype() == RE::EffectArchetype::kScript
                                      ? EffectKind::RegisteredScriptedVision
                                      : EffectKind::Other;
                effects.push_back({kind, !effect || effect->IsHostile()});
            }
            return !VisionOnly(spell->GetDelivery() == RE::MagicSystem::Delivery::kSelf, effects);
        }

        Spell Describe(RE::SpellItem* spell) {
            return {spell->GetFormID(), spell->GetName() ? std::string(spell->GetName()).substr(0, 80) : "<unnamed>"};
        }

        bool Add(std::vector<RE::SpellItem*>& spells, RE::SpellItem* spell) {
            if (spell && std::ranges::find(spells, spell) == spells.end()) {
                if (spells.size() >= MaximumSpells)
                    return false;
                spells.push_back(spell);
            }
            return true;
        }

        class Visitor final : public RE::Actor::ForEachSpellVisitor {
        public:
            RE::BSContainer::ForEachResult Visit(RE::SpellItem* spell) override {
                if (++visited > MaximumSpells || !Add(spells, spell)) {
                    bounded = false;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            }

            std::vector<RE::SpellItem*> spells;
            std::size_t visited = 0;
            bool bounded = true;
        };
    }

    void Configure() {
        // Both vanilla and Requiem's Khajiit Night Eye use magicNightEyeScript.
        // Register their effect identities; arbitrary scripts remain isolated.
        auto* data = RE::TESDataHandler::GetSingleton();
        scriptedVision = {data ? data->LookupForm<RE::EffectSetting>(0x6B10C, "Skyrim.esm") : nullptr,
                          data ? data->LookupForm<RE::EffectSetting>(0x1F46D5, "Requiem.esp") : nullptr};
    }

    bool DetachPassives(RE::TESNPC* destination, const RE::TESNPC* source) {
        if (!Private(destination) || !source || destination == source)
            return false;
        const auto* original = source->actorEffects;
        if (original && (original->numSpells > MaximumSpells || (original->numSpells && !original->spells)))
            return false;
        auto* spells = new RE::TESSpellList::SpellData();
        if (original)
            for (std::uint32_t i = 0; i < original->numSpells; ++i)
                if (auto* spell = original->spells[i]; spell && !Unwanted(spell, nullptr))
                    spells->AddSpell(spell);
        destination->actorEffects = spells;
        return true;
    }

    Result RestrictBase(RE::TESNPC* base, RE::SpellItem* expected) {
        Result result;
        result.owned = Private(base);
        if (!result.owned || !base->actorEffects)
            return result;
        auto* data = base->actorEffects;
        if (data->numSpells > MaximumSpells || (data->numSpells && !data->spells)) {
            result.bounded = false;
            return result;
        }
        std::vector<RE::SpellItem*> unwanted;
        for (std::uint32_t i = 0; i < data->numSpells; ++i)
            if (Unwanted(data->spells[i], expected))
                Add(unwanted, data->spells[i]);
        for (auto* spell : unwanted) {
            if (data->RemoveSpell(spell))
                result.removed.push_back(Describe(spell));
            else
                result.remaining.push_back(Describe(spell));
        }
        return result;
    }

    Result Inspect(RE::Actor* actor, RE::TESNPC* base, RE::SpellItem* expected) {
        Result result;
        result.owned = Owned(Dynamic(actor), fixture_bases::Owns(base), actor && actor->GetActorBase() == base);
        if (!result.owned)
            return result;
        const auto* data = base->actorEffects;
        const auto& added = actor->GetActorRuntimeData().addedSpells;
        if ((data && (data->numSpells > MaximumSpells || (data->numSpells && !data->spells))) ||
            added.size() > MaximumSpells) {
            result.bounded = false;
            return result;
        }
        Visitor visitor;
        actor->VisitSpells(visitor);
        for (auto* spell : added)
            visitor.bounded &= Add(visitor.spells, spell);
        if (data)
            for (std::uint32_t i = 0; i < data->numSpells; ++i)
                visitor.bounded &= Add(visitor.spells, data->spells[i]);
        result.bounded = visitor.bounded;
        for (auto* spell : visitor.spells)
            if (Unwanted(spell, expected))
                result.remaining.push_back(Describe(spell));
        for (const auto left : {false, true}) {
            auto* item = actor->GetEquippedObject(left);
            auto* spell = item ? item->As<RE::SpellItem>() : nullptr;
            if (Unwanted(spell, expected) && std::ranges::none_of(result.remaining, [spell](const auto& entry) {
                    return entry.id == spell->GetFormID();
                }))
                result.remaining.push_back(Describe(spell));
        }
        return result;
    }

    Result Restrict(RE::Actor* actor, RE::TESNPC* base, RE::SpellItem* expected) {
        Result result;
        if (!Owned(Dynamic(actor), fixture_bases::Owns(base), actor && actor->GetActorBase() == base))
            return result;
        // Copy before removals; the engine mutates these lists. Never mutate race
        // or template records to remove an inherited spell that cannot be isolated.
        auto& added = actor->GetActorRuntimeData().addedSpells;
        if (added.size() > MaximumSpells) {
            result.owned = true;
            result.bounded = false;
            return result;
        }
        std::vector<RE::SpellItem*> snapshot;
        for (auto* spell : added)
            Add(snapshot, spell);
        for (auto* spell : snapshot)
            if (Unwanted(spell, expected)) {
                actor->DeselectSpell(spell);
                if (actor->RemoveSpell(spell))
                    result.removed.push_back(Describe(spell));
            }
        if (base->actorEffects && base->actorEffects->spells && base->actorEffects->numSpells <= MaximumSpells)
            for (std::uint32_t i = 0; i < base->actorEffects->numSpells; ++i)
                if (Unwanted(base->actorEffects->spells[i], expected))
                    actor->DeselectSpell(base->actorEffects->spells[i]);
        auto baseResult = RestrictBase(base, expected);
        const auto inspection = Inspect(actor, base, expected);
        result.owned = inspection.owned;
        result.bounded = baseResult.bounded && inspection.bounded;
        result.removed.insert(result.removed.end(), baseResult.removed.begin(), baseResult.removed.end());
        result.remaining = inspection.remaining;
        return result;
    }
}
