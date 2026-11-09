#include "NaturalOpponent.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace mod::combat_tests::natural {
    namespace {
        RE::ActorHandle receipt;
        std::uint32_t generation = 0, pinnedBase = 0, level = 0;
        std::string description, details;
        std::vector<RE::ActorHandle> summons;

        bool Dynamic(const RE::TESForm* form) {
            return form && (form->GetFormID() & 0xFF000000) == 0xFF000000;
        }

        RE::TESNPC* Resolve(RE::TESDataHandler* data) {
            if (pinnedBase) {
                auto* npc = RE::TESForm::LookupByID<RE::TESNPC>(pinnedBase);
                if (!npc)
                    details = "Pinned mage NPC record is unavailable.\n";
                return npc;
            }
            auto* list = data ? data->LookupForm<RE::TESLevCharacter>(FireMageList, "Skyrim.esm") : nullptr;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!list || !player) {
                details = "Fire mage leveled list or player is unavailable.\n";
                return nullptr;
            }
            // Resolve the list before creating a reference. The reference factory
            // accepts concrete NPC bases; it does not perform leveled-list rolls.
            RE::BSScrapArray<RE::CALCED_OBJECT> resolved;
            list->CalculateCurrentFormList(player->GetLevel(), 1, resolved, 0, true);
            auto* npc = resolved.size() == 1 && resolved[0].form ? resolved[0].form->As<RE::TESNPC>() : nullptr;
            if (!SingleActor(static_cast<std::uint32_t>(resolved.size()), resolved.empty() ? 0 : resolved[0].count,
                             npc != nullptr)) {
                std::ostringstream message;
                message << "Fire mage list resolution: " << resolved.size()
                        << " entries; expected one NPC with count 1.\n";
                for (const auto& entry : resolved)
                    if (entry.form)
                        message << "  Form " << std::hex << entry.form->GetFormID() << std::dec << "; type "
                                << static_cast<std::uint32_t>(entry.form->GetFormType()) << "; count " << entry.count
                                << '\n';
                details = message.str();
                return nullptr;
            }
            return npc;
        }

        class Spells final : public RE::Actor::ForEachSpellVisitor {
        public:
            RE::BSContainer::ForEachResult Visit(RE::SpellItem* spell) override {
                if (++visited > 512)
                    return RE::BSContainer::ForEachResult::kStop;
                if (spell && ids.size() < 32 && std::ranges::find(ids, spell->GetFormID()) == ids.end()) {
                    ids.push_back(spell->GetFormID());
                    text << "  " << std::hex << spell->GetFormID() << std::dec << ' '
                         << (spell->GetName() ? spell->GetName() : "<unnamed>") << '\n';
                }
                return RE::BSContainer::ForEachResult::kContinue;
            }

            std::uint32_t visited = 0;
            std::vector<RE::FormID> ids;
            std::ostringstream text;
        };
    }

    void BeginRun() {
        details.clear();
        level = 0;
    }

    RE::Actor* Spawn(RE::TESObjectREFR* marker, std::uint32_t token) {
        if (!marker || !token || receipt.get()) {
            details = "Mage spawn rejected: missing marker/token or an existing live placement receipt.\n";
            return nullptr;
        }
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* source = Resolve(data);
        if (!source)
            return nullptr;
        const auto spawned = source ? marker->PlaceObjectAtMe(source, true) : RE::NiPointer<RE::TESObjectREFR>{};
        auto* actor = spawned ? spawned->As<RE::Actor>() : nullptr;
        auto* base = actor ? actor->GetActorBase() : nullptr;
        if (!actor || !Dynamic(actor) || !base || base->IsUnique() || actor->IsEssential() || actor->IsProtected() ||
            base->IsInvulnerable()) {
            std::ostringstream message;
            message << "Mage placement rejected: source " << std::hex << source->GetFormID() << "; returned reference "
                    << (spawned ? spawned->GetFormID() : 0) << "; actor base " << (base ? base->GetFormID() : 0)
                    << std::dec << "; actor " << (actor != nullptr) << "; generated " << Dynamic(actor) << "; unique "
                    << (base && base->IsUnique()) << "; essential " << (actor && actor->IsEssential()) << "; protected "
                    << (actor && actor->IsProtected()) << "; invulnerable " << (base && base->IsInvulnerable()) << '\n';
            details = message.str();
            // Only the reference returned by this placement is ours to discard.
            if (spawned) {
                spawned->Disable();
                spawned->SetDelete(true);
            }
            return nullptr;
        }
        actor->EnableAI(false);
        actor->StopCombat();
        receipt = actor->GetHandle();
        generation = token;
        pinnedBase = source->GetFormID();
        summons.clear();
        details.clear();
        Capture();
        return actor;
    }

    bool Owns(RE::Actor* actor, std::uint32_t base, std::uint32_t token) {
        const auto owned = receipt.get();
        return Owned(Dynamic(actor), owned && owned.get() == actor,
                     actor && actor->GetActorBase() && actor->GetActorBase()->GetFormID() == base,
                     token && generation == token);
    }

    void Capture() {
        const auto actor = receipt.get();
        if (!actor || !actor->GetActorBase())
            return;
        const auto* base = actor->GetActorBase();
        level = actor->GetLevel();
        std::ostringstream label;
        label << (actor->GetName() ? actor->GetName() : "Mage") << " | level " << level << " | base " << std::hex
              << base->GetFormID();
        description = label.str();
        const auto value = [&](RE::ActorValue name) { return actor->AsActorValueOwner()->GetActorValue(name); };
        std::ostringstream text;
        const auto* source = base->GetFile(0);
        const auto* winning = base->GetFile(-1);
        text << "Natural opponent: " << description << '\n'
             << "Resolved from Skyrim.esm LCharWarlockFire (0001E7D1); pinned for repeat comparisons.\n"
             << "Defining plugin: " << (source ? source->fileName : "<runtime>") << '\n'
             << "Winning NPC override: " << (winning ? winning->fileName : "<runtime>") << '\n'
             << "Player level: "
             << (RE::PlayerCharacter::GetSingleton() ? RE::PlayerCharacter::GetSingleton()->GetLevel() : 0) << '\n'
             << "Initial health " << value(RE::ActorValue::kHealth) << "; magicka " << value(RE::ActorValue::kMagicka)
             << "; stamina " << value(RE::ActorValue::kStamina) << "; Destruction "
             << value(RE::ActorValue::kDestruction) << "; magicka regeneration " << value(RE::ActorValue::kMagickaRate)
             << "; regeneration multiplier " << value(RE::ActorValue::kMagickaRateMult) << '\n';
        for (const bool left : {false, true}) {
            const auto* item = actor->GetEquippedObject(left);
            text << (left ? "Initial left hand: " : "Initial right hand: ");
            if (item)
                text << std::hex << item->GetFormID() << std::dec << ' ' << item->GetName();
            else
                text << "None";
            text << '\n';
        }
        Spells spells;
        actor->VisitSpells(spells);
        text << "Observed starting spells (up to 32):\n" << spells.text.str();
        details = text.str();
    }

    const std::string& Description() {
        return description;
    }

    const std::string& Details() {
        return details;
    }

    std::uint32_t Level() {
        return level;
    }

    std::uint32_t Participant(std::uint32_t id, std::uint32_t subject, std::uint32_t enemy) {
        if (!id || id == subject || id == enemy)
            return id;
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
        if (!actor || !Dynamic(actor) || !actor->IsSummoned())
            return id;
        auto owner = actor->GetCommandingActor();
        for (std::uint32_t depth = 0; owner && depth < 4; ++depth) {
            if (owner->GetFormID() == subject || owner->GetFormID() == enemy) {
                const auto handle = actor->GetHandle();
                if (std::ranges::find(summons, handle) == summons.end()) {
                    if (summons.size() >= 32)
                        return id;
                    summons.push_back(handle);
                }
                return owner->GetFormID();
            }
            owner = owner->GetCommandingActor();
        }
        return id;
    }

    void CleanupSummons() {
        const auto opponent = receipt.get();
        if (opponent)
            if (auto* processes = RE::ProcessLists::GetSingleton())
                for (const auto& handle : processes->highActorHandles)
                    if (const auto actor = handle.get(); actor && actor->GetParentCell() == opponent->GetParentCell())
                        Participant(actor->GetFormID(), 0, opponent->GetFormID());
        for (const auto& handle : summons)
            if (const auto actor = handle.get(); actor && Dynamic(actor.get())) {
                actor->StopCombat();
                actor->InterruptCast(true);
                actor->Disable();
                actor->SetDelete(true);
            }
        summons.clear();
    }

    void Release() {
        receipt = {};
        generation = 0;
    }

    void Reroll() {
        pinnedBase = 0;
        description.clear();
        details.clear();
        level = 0;
    }

    void Reset() {
        Release();
        Reroll();
        summons.clear();
    }
}
