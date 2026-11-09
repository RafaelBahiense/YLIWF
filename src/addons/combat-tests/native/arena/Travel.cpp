#include "Travel.h"
#include "TravelRules.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <atomic>
#include <cstdio>
#include <vector>

namespace {
    using namespace mod::combat_tests::travel;
    Bookmark bookmark;
    Snapshot snapshot;
    Move queued = Move::None;
    std::atomic_bool pending{false};
    bool invalid = false;
    float elapsed = 0;

    struct RawRecord {
        std::uint32_t version;
        std::vector<std::uint8_t> bytes;
    };

    std::vector<RawRecord> records;

    void Status(const char* message) {
        std::snprintf(snapshot.status, sizeof(snapshot.status), "%s", message);
    }

    RE::TESObjectREFR* Origin() {
        auto* origin = RE::TESForm::LookupByID<RE::TESObjectREFR>(bookmark.origin);
        return origin && (origin->GetFormID() & 0xFF000000) == 0xFF000000 && origin->GetBaseObject() &&
                       origin->GetBaseObject()->GetFormID() == 0x3B && !origin->IsDisabled()
                   ? origin
                   : nullptr;
    }
}

void mod::combat_tests::travel::Update(bool ready, bool testBusy) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* data = RE::TESDataHandler::GetSingleton();
    auto* arena = data ? data->LookupForm<RE::TESObjectREFR>(0x810, "YouLeadIWillFollow.CombatArena.esp") : nullptr;
    const bool atArena = player && arena && player->GetParentCell() == arena->GetParentCell();
    const bool unavailable = !player || !player->Is3DLoaded() || player->IsDead() || player->IsDisabled() ||
                             player->IsInCombat() || player->GetCurrentScene() || player->IsOnMount() ||
                             player->AsActorState()->IsUnconscious();
    const bool allowed = CanTravel(ready, testBusy, unavailable || invalid || !arena, false);
    snapshot.atArena = atArena;
    snapshot.hasReturn = bookmark.origin != 0;
    snapshot.pending = pending;
    const auto* cell = player ? player->GetParentCell() : nullptr;
    std::snprintf(snapshot.location, sizeof(snapshot.location), "%s", cell && cell->GetName() ? cell->GetName() : "");

    if (queued != Move::None) {
        const auto command = queued;
        queued = Move::None;
        if (!player || !data || !arena || !allowed || (command == Move::Enter && (atArena || bookmark.origin)) ||
            (command == Move::Return && !bookmark.origin)) {
            pending = false;
            Status("Travel request rejected: player, test or return location changed.");
        } else {
            auto* target = command == Move::Return ? Origin() : arena;
            if (command == Move::Enter) {
                auto* base = data->LookupForm<RE::TESObjectSTAT>(0x3B, "Skyrim.esm");
                auto origin = base ? player->PlaceObjectAtMe(base, true) : RE::NiPointer<RE::TESObjectREFR>{};
                if (origin) {
                    origin->data.angle = player->GetAngle();
                    bookmark.origin = origin->GetFormID();
                } else
                    target = nullptr;
            }
            if (target) {
                bookmark.moving = command;
                elapsed = 0;
                Status(command == Move::Enter ? "Entering arena; waiting for loading."
                                              : "Returning; waiting for loading.");
                player->MoveTo(target);
            } else {
                pending = false;
                Status("Return marker unavailable; travel was not issued.");
            }
        }
    }

    if (bookmark.moving != Move::None && player) {
        auto* target = bookmark.moving == Move::Enter ? arena : Origin();
        if (target && player->Is3DLoaded() && player->GetParentCell() == target->GetParentCell() &&
            player->GetPosition().GetDistance(target->GetPosition()) < 96) {
            if (bookmark.moving == Move::Return) {
                target->Disable();
                target->SetDelete(true);
                bookmark.origin = 0;
                Status("Returned to previous location.");
            } else
                Status("Arena ready.");
            bookmark.moving = Move::None;
            pending = false;
        } else {
            elapsed += 0.1f;
            if (elapsed >= 30)
                Status("Travel is still unconfirmed. No teleport will be repeated automatically.");
        }
    }

    snapshot.pending = pending;
    snapshot.hasReturn = bookmark.origin != 0;
    snapshot.atArena = player && arena && player->GetParentCell() == arena->GetParentCell();
    const bool canAct = allowed && !pending;
    const bool missingReturn = bookmark.origin && !Origin();
    snapshot.canEnter = CanEnter(canAct, snapshot.atArena != 0, snapshot.hasReturn != 0);
    snapshot.canReturn = CanReturn(canAct && !missingReturn, snapshot.hasReturn != 0);
    const char* blocked = invalid         ? "Unsupported arena travel data retained; travel is disabled."
                          : pending       ? "Travel is pending. Close paused menus to finish loading."
                          : testBusy      ? "Finish the test and its restoration before travelling."
                          : !arena        ? "Arena plugin or observer marker is unavailable."
                          : missingReturn ? "Saved return marker is unavailable; load the pre-travel save."
                          : !allowed      ? "Player must be loaded, outside combat and scenes, and dismounted."
                                          : "";
    std::snprintf(snapshot.blocked, sizeof(snapshot.blocked), "%s", blocked);
}

bool mod::combat_tests::travel::Request(bool returnToOrigin) {
    if (!(returnToOrigin ? snapshot.canReturn : snapshot.canEnter) || pending.exchange(true))
        return false;
    queued = returnToOrigin ? Move::Return : Move::Enter;
    snapshot.canEnter = snapshot.canReturn = 0;
    snapshot.pending = 1;
    Status("Travel requested. Close paused menus to continue.");
    return true;
}

mod::combat_tests::travel::Snapshot mod::combat_tests::travel::Read() {
    return snapshot;
}

bool mod::combat_tests::travel::NeedsUpdate() {
    return pending;
}

bool mod::combat_tests::travel::InvalidRecord() {
    return invalid;
}

void mod::combat_tests::travel::Reset() {
    bookmark = {};
    snapshot = {};
    queued = Move::None;
    pending = false;
    invalid = false;
    elapsed = 0;
    records.clear();
}

void mod::combat_tests::travel::Save(SKSE::SerializationInterface* serialization) {
    if (invalid) {
        for (const auto& record : records)
            serialization->WriteRecord(Record, record.version, record.bytes.data(),
                                       static_cast<std::uint32_t>(record.bytes.size()));
    } else if (bookmark.origin && !serialization->WriteRecord(Record, 1, &bookmark, sizeof(bookmark)))
        SKSE::log::error("Combat tests: could not save the arena return location.");
}

void mod::combat_tests::travel::Load(SKSE::SerializationInterface* serialization, std::uint32_t version,
                                     std::span<const std::uint8_t> bytes) {
    records.push_back({version, {bytes.begin(), bytes.end()}});
    const auto decoded = Decode(version, bytes);
    std::uint32_t resolved = 0;
    if (invalid || records.size() != 1 || !decoded || !serialization->ResolveFormID(decoded->origin, resolved) ||
        (resolved & 0xFF000000) != 0xFF000000) {
        invalid = true;
        bookmark = {};
        pending = false;
        return;
    }
    bookmark = *decoded;
    bookmark.origin = resolved;
    pending = bookmark.moving != Move::None;
}
