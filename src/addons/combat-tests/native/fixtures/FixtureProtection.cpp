#include "FixtureProtection.h"
#include "FixtureProtectionRules.h"
#include "../integrations/HealthWrites.h"
#include "RE/Skyrim.h"
#include <atomic>
#include <memory>
#include <vector>
#include <algorithm>

namespace mod::combat_tests::fixture_protection {
    namespace {
        struct Scope {
            std::vector<std::uint32_t> actors;
        };

        std::atomic<std::shared_ptr<const Scope>> published;
        std::atomic_uint32_t blocked{0};
        std::atomic<float> damage{0};
        std::atomic_uint32_t publicWrites{0}, combatWrites{0}, otherWrites{0};

        float ChangeValue(std::uint32_t actor, bool damageModifier, bool health, float amount,
                          health_writes::Path path) {
            const auto scope = published.load();
            const bool scoped = scope && actor && std::ranges::find(scope->actors, actor) != scope->actors.end();
            if (fixture_protection_rules::Suppress(scoped, damageModifier, health, amount)) {
                ++blocked;
                damage.fetch_add(-amount);
                if (path == health_writes::Path::Combat)
                    ++combatWrites;
                else if (path == health_writes::Path::Public)
                    ++publicWrites;
                else
                    ++otherWrites;
                amount = 0;
            }
            return amount;
        }
    }

    void Install() {
        health_writes::Install(ChangeValue);
    }

    bool Begin(std::uint32_t primary) {
        auto* actor = primary ? RE::TESForm::LookupByID<RE::Actor>(primary) : nullptr;
        if (!actor || !health_writes::Ready() || published.load())
            return false;
        blocked = 0;
        damage = 0;
        publicWrites = combatWrites = otherWrites = 0;
        published = std::make_shared<Scope>(Scope{{primary}});
        return true;
    }

    bool Add(std::uint32_t secondary) {
        const auto scope = published.load();
        auto* actor = secondary ? RE::TESForm::LookupByID<RE::Actor>(secondary) : nullptr;
        if (!scope || !actor || scope->actors.size() >= 16 ||
            std::ranges::find(scope->actors, secondary) != scope->actors.end())
            return false;
        auto updated = std::make_shared<Scope>(*scope);
        updated->actors.push_back(secondary);
        published = std::move(updated);
        return true;
    }

    bool Protected(std::uint32_t actor) {
        const auto scope = published.load();
        return scope && health_writes::Ready() && actor &&
               std::ranges::find(scope->actors, actor) != scope->actors.end();
    }

    Snapshot Read() {
        return {blocked.load(), damage.load(), publicWrites.load(), combatWrites.load(), otherWrites.load()};
    }

    const char* Status() {
        return health_writes::Status();
    }

    bool ProbeCombat(std::uint32_t id, float amount) {
        auto* actor = id ? RE::TESForm::LookupByID<RE::Actor>(id) : nullptr;
        return actor && Protected(id) && std::isfinite(amount) && amount > 0 &&
               health_writes::ProbeCombat(actor, -amount);
    }

    void End() {
        published = nullptr;
    }

    void Reset() {
        End();
        blocked = 0;
        damage = 0;
        publicWrites = combatWrites = otherWrites = 0;
    }
}
