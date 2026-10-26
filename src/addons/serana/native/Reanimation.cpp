#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include "Reanimation.h"
#include "ReanimationRules.h"
#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace mod::serana_reanimation {
    namespace {
        constexpr std::uint32_t PluginID = 0x594C5352;  // YLSR
        constexpr std::uint32_t RecordID = 0x5245414E;  // REAN
        RE::Actor* serana = nullptr;
        std::mutex mutex;
        bool allowed = true, ready = false, invalid = false, serializationAvailable = false;
        std::atomic<std::uint32_t> blockedHandle = 0;

        struct Record {
            std::uint32_t type, version;
            std::vector<std::uint8_t> bytes;
        };

        std::vector<Record> originalRecords;

        using Item = RE::CombatInventoryItemMagic;
        using Caster = RE::CombatMagicCasterReanimate;
        using Controller = RE::CombatController;
        REL::Relocation<float (*)(Item*, Controller*)> originalScore;
        REL::Relocation<bool (*)(Caster*, Controller*)> originalStart;

        bool Blocked(const Controller* controller) {
            // Combat worker: compare handles against a main-thread snapshot only.
            return controller && Suppress(controller->attackerHandle.native_handle(), blockedHandle.load());
        }

        float Score(Item* item, Controller* controller) {
            return Blocked(controller) ? 0.0f : originalScore(item, controller);
        }

        bool Start(Caster* caster, Controller* controller) {
            // Only spellbook spells: scrolls, staves and scripted casts retain their behavior.
            if (Blocked(controller) && caster->magicItem && caster->magicItem->As<RE::SpellItem>())
                return false;
            return originalStart(caster, controller);
        }

        void Publish() {
            blockedHandle.store(ready && !allowed && serana ? serana->GetHandle().native_handle() : 0);
        }

        void Reset() {
            blockedHandle.store(0);
            ready = invalid = false;
            allowed = true;
            originalRecords.clear();
        }

        void Save(SKSE::SerializationInterface* serialization) {
            std::scoped_lock lock(mutex);
            if (invalid) {
                for (const auto& record : originalRecords)
                    if (!serialization->WriteRecord(record.type, record.version, record.bytes.data(),
                                                    static_cast<std::uint32_t>(record.bytes.size())))
                        SKSE::log::error("Serana: could not preserve unsupported settings record");
                return;
            }
            const std::array<std::uint8_t, 4> bytes{static_cast<std::uint8_t>(allowed), 0, 0, 0};
            if (!serialization->WriteRecord(RecordID, 1, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                SKSE::log::error("Serana: could not save reanimation preference");
        }

        void Load(SKSE::SerializationInterface* serialization) {
            std::scoped_lock lock(mutex);
            Reset();
            std::uint32_t type, version, length;
            while (serialization->GetNextRecordInfo(type, version, length)) {
                if (length > 1024 || originalRecords.size() >= 8) {
                    invalid = true;
                    continue;
                }
                Record record{type, version, std::vector<std::uint8_t>(length)};
                if (serialization->ReadRecordData(record.bytes.data(), length) != length) {
                    invalid = true;
                    continue;
                }
                originalRecords.push_back(std::move(record));
            }
            if (!originalRecords.empty()) {
                const auto& record = originalRecords.front();
                const auto value = DecodePreference(record.version, record.bytes);
                if (!invalid && originalRecords.size() == 1 && record.type == RecordID && value)
                    allowed = *value;
                else
                    invalid = true;
            }
            if (invalid)
                SKSE::log::warn("Serana: unsupported or invalid settings; reanimation control is unavailable");
        }

        void Revert(SKSE::SerializationInterface*) {
            std::scoped_lock lock(mutex);
            Reset();
        }
    }

    void Initialize(RE::Actor* actor) {
        std::scoped_lock lock(mutex);
        serana = actor;
        if (!serana || originalScore.address())
            return;
        REL::Relocation<std::uintptr_t> inventoryTable{
            RE::VTABLE_CombatInventoryItemMagicT_CombatInventoryItemMagic_CombatMagicCasterReanimate_[0]};
        REL::Relocation<std::uintptr_t> casterTable{RE::VTABLE_CombatMagicCasterReanimate[0]};
        originalScore = inventoryTable.write_vfunc(0x0C, Score);
        originalStart = casterTable.write_vfunc(0x06, Start);
        SKSE::log::info("Serana: combat reanimation hooks installed");
    }

    void RegisterSerialization() {
        if (auto* serialization = SKSE::GetSerializationInterface()) {
            serializationAvailable = true;
            serialization->SetUniqueID(PluginID);
            serialization->SetSaveCallback(Save);
            serialization->SetLoadCallback(Load);
            serialization->SetRevertCallback(Revert);
        } else {
            std::scoped_lock lock(mutex);
            invalid = true;
        }
    }

    void Invalidate() {
        std::scoped_lock lock(mutex);
        ready = false;
        blockedHandle.store(0);
    }

    void NewGame() {
        std::scoped_lock lock(mutex);
        Reset();
    }

    void GameLoaded() {
        std::scoped_lock lock(mutex);
        ready = true;
        Publish();
    }

    bool Available() {
        std::scoped_lock lock(mutex);
        return ready && !invalid && serializationAvailable && serana && originalScore.address() &&
               originalStart.address();
    }

    bool Allowed() {
        std::scoped_lock lock(mutex);
        return allowed;
    }

    bool SetAllowed(bool value) {
        std::scoped_lock lock(mutex);
        if (!ready || invalid || !serializationAvailable || !serana)
            return false;
        allowed = value;
        Publish();
        if (!allowed) {
            for (const auto source :
                 {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand}) {
                auto* caster = serana->GetMagicCaster(source);
                if (caster && caster->currentSpell &&
                    std::ranges::any_of(caster->currentSpell->effects, [](const auto* effect) {
                        return effect && effect->baseEffect &&
                               effect->baseEffect->data.archetype == RE::EffectArchetypes::ArchetypeID::kReanimate;
                    }))
                    caster->InterruptCast(true);
            }
        }
        return true;
    }
}
