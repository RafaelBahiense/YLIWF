#include "formations/Formation.h"
#include "combat/CombatOrders.h"
#include "formations/FormationRules.h"
#include "formations/FormationPath.h"
#include "formations/FormationGeometry.h"
#include "formations/FormationProgress.h"
#include "formations/FormationGround.h"
#include "formations/FormationLocationRules.h"
#include "followers/ControllerExecutor.h"
#include "followers/ControllerRuntime.h"
#include "followers/FollowDistance.h"
#include "adapters/Adapters.h"
#include "diagnostics/Debug.h"
#include "settings/Settings.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <random>

namespace mod::formation {
    namespace {
        struct Request {
            int ticket;
            std::uint32_t resetRevision;
        };

        struct Follower {
            RE::ActorHandle actor;
            int slot = 0;
            std::optional<formation_rules::Point> destination;
            std::optional<formation_rules::Point> geometricDestination;
            std::optional<Request> pending;
            bool reset = false, failed = false, present = false, active = false;
            std::uint32_t resetRevision = 0;
            float waitingSeconds = 0;
            float refreshSeconds = 0;
            bool targetMoved = false;
            formation_rules::Progress progress;
        };

        std::map<RE::FormID, Follower> followers;
        RE::TESFaction* marker = nullptr;
        std::array<RE::BGSKeyword*, formation_rules::SettlementKeywordForms.size()> settlementKeywords{};
        std::array<RE::TESPackage*, 3> followPackages{};
        std::array<RE::TESPackage*, formation_rules::Slots * 2> travelPackages{};
        std::array<RE::TESObjectREFR*, formation_rules::Slots> targets{};
        formation_rules::Path path;
        std::optional<formation_rules::Point> playerPosition;
        bool holdingAnchor = false;
        RE::TESWorldSpace* world = nullptr;
        bool loaded = false;
        int nextTicket = 0;  // Never reuse a ticket, including across save loads.
        auto lastTick = std::chrono::steady_clock::now();
        std::string status = "No game loaded.";

        std::uint64_t Session() {
            // Saved VM arguments outlive the process-local ticket counter.
            // A fresh session nonce prevents an old stack matching a new request
            // after quitting and relaunching Skyrim. Initialize outside DLL loading.
            static const auto nonce = [] {
                std::random_device random;
                return (std::uint64_t{random()} << 32) | random();
            }();
            return nonce;
        }

        bool SameSession(int high, int low) {
            return ((std::uint64_t{std::bit_cast<std::uint32_t>(high)} << 32) | std::bit_cast<std::uint32_t>(low)) ==
                   Session();
        }

        int Rank(RE::Actor* actor) {
            return actor && marker ? actor->GetFactionRank(marker, false) : -1;
        }

        void Mark(RE::Actor* actor, int slot, bool owned) {
            const auto rank = formation_rules::Rank(slot, owned);
            if (Rank(actor) != rank) {
                actor->AddToFaction(marker, rank);
                actor->EvaluatePackage();
            }
        }

        bool InSettlement(RE::Actor* player) {
            if (!player)
                return false;
            const auto* location = player->GetCurrentLocation();
            if (!location)
                if (auto* cell = player->GetParentCell())
                    location = cell->GetLocation();
            return formation_rules::InSettlement(location, [](const auto* current) {
                return std::ranges::any_of(settlementKeywords, [current](const auto* keyword) {
                    return keyword && current->HasKeyword(keyword);
                });
            });
        }

        bool TravelAllowed(RE::Actor* player) {
            const auto* cell = player ? player->GetParentCell() : nullptr;
            auto* ui = RE::UI::GetSingleton();
            return player && player->Is3DLoaded() && cell && !cell->IsInteriorCell() && !InSettlement(player) &&
                   !player->IsInCombat() && !player->IsOnMount() &&
                   player->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
                   !player->AsActorState()->IsSwimming() && !player->GetCurrentScene() && ui &&
                   !ui->IsMenuOpen("Dialogue Menu") && controller::storage::Available() &&
                   !controller::storage::Get().active && controller::storage::Get().queue.empty();
        }

        bool Eligible(RE::Actor* actor, RE::Actor* player) {
            const auto& context = controller::detail::GetContext();
            const auto* package = actor ? actor->GetCurrentPackage() : nullptr;
            const bool external = actor && adapters::Owns(actor->GetFormID());
            const bool controllerAllows =
                external ? adapters::CanTravelInFormation(actor->GetFormID())
                         : actor && context.currentFaction && actor->IsInFaction(context.currentFaction) && package &&
                               (std::ranges::find(followPackages, package) != followPackages.end() ||
                                std::ranges::find(travelPackages, package) != travelPackages.end());
            return actor && !actor->IsDead() && !actor->IsDisabled() && actor->Is3DLoaded() &&
                   actor->IsPlayerTeammate() && controllerAllows && !actor->IsInCombat() && !actor->IsOnMount() &&
                   !actor->AsActorState()->IsSwimming() &&
                   actor->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
                   !actor->GetCurrentScene() && actor->GetWorldspace() &&
                   actor->GetWorldspace() == player->GetWorldspace() &&
                   actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) == 0;
        }

        void Recover(RE::Actor* actor) {
            const auto rank = Rank(actor);
            if (!formation_rules::OwnsOffset(rank))
                return;
            auto [entry, inserted] = followers.try_emplace(actor->GetFormID());
            if (inserted) {
                entry->second.actor = actor->GetHandle();
                entry->second.slot = formation_rules::Slot(rank);
            }
            // The actor can already be cached when saved ownership reappears.
            entry->second.reset = true;
        }

        void SetTravelRank(RE::Actor* actor, std::int8_t rank) {
            if (adapters::Owns(actor->GetFormID())) {
                const bool changed = Rank(actor) != rank;
                if (Rank(actor) != rank)
                    actor->AddToFaction(marker, rank);
                const auto* package =
                    formation_rules::Active(rank)
                        ? travelPackages.at(static_cast<std::size_t>(rank - formation_rules::Slots * 2))
                        : nullptr;
                if (!adapters::SetFormationPackage(actor->GetFormID(), package ? package->GetFormID() : 0) && package) {
                    actor->AddToFaction(marker, formation_rules::Rank(formation_rules::Slot(rank), false));
                    adapters::SetFormationPackage(actor->GetFormID(), 0);
                    debug::Trace("Formation.Package", actor, "adapter rejected rank={} slot={}", rank,
                                 formation_rules::Slot(rank));
                } else if (changed) {
                    debug::Trace("Formation.Package", actor, "adapter rank={} slot={}", rank,
                                 formation_rules::Slot(rank));
                }
                return;
            }
            if (Rank(actor) == rank)
                return;
            actor->AddToFaction(marker, rank);
            actor->EvaluatePackage(true, false);
            debug::Trace("Formation.Package", actor, "rank={} slot={}", rank, formation_rules::Slot(rank));
        }

        void SetActive(RE::Actor* actor, int slot, bool active) {
            if (active && formation_rules::Active(Rank(actor)) && formation_rules::Slot(Rank(actor)) == slot)
                return;
            SetTravelRank(actor, static_cast<std::int8_t>(slot + (active ? formation_rules::Slots * 2 : 0)));
        }

        void ResetSaved(RE::Actor* actor) {
            if (adapters::Owns(actor->GetFormID()))
                adapters::SetFormationPackage(actor->GetFormID(), 0);
            Recover(actor);
            if (formation_rules::Active(Rank(actor)))
                SetActive(actor, formation_rules::Slot(Rank(actor)), false);
        }

        std::vector<RE::Actor*> Roster() {
            std::vector<RE::Actor*> result;
            const auto* quest = controller::detail::GetContext().quest;
            if (quest)
                for (auto* alias : quest->aliases) {
                    if (!alias || (alias->aliasID != 0 && (alias->aliasID < 2 || alias->aliasID > 8)))
                        continue;
                    const auto* reference = skyrim_cast<RE::BGSRefAlias*>(alias);
                    auto* actor = reference ? reference->GetActorReference() : nullptr;
                    if (actor && !adapters::Owns(actor->GetFormID()) &&
                        std::ranges::find(result, actor) == result.end())
                        result.push_back(actor);
                }
            std::ranges::sort(result, {}, [](const RE::Actor* actor) { return actor->GetFormID(); });
            // Core followers keep priority when all eight movement slots are
            // occupied. Adapter actors never consume recruitment aliases.
            for (const auto& follower : adapters::Followers())
                if (adapters::SupportsTravelFormation(follower.actor))
                    if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(follower.actor))
                        if (std::ranges::find(result, actor) == result.end())
                            result.push_back(actor);
            return result;
        }

        void Plan(RE::Actor* player) {
            for (auto& [id, follower] : followers) {
                follower.destination.reset();
                follower.geometricDestination.reset();
                follower.present = false;
            }
            const auto roster = Roster();
            std::array<int, formation_rules::Slots> preferences{}, presets{};
            preferences.fill(-1);
            presets.fill(settings::FollowDistance);
            const auto count = std::min(roster.size(), preferences.size());
            for (std::size_t i = 0; i < count; ++i) {
                Recover(roster[i]);
                preferences[i] = formation_rules::Slot(Rank(roster[i]));
            }
            const auto slots = formation_rules::AssignSlots(preferences, static_cast<int>(count));
            for (std::size_t i = 0; i < count; ++i)
                presets[slots[i]] = follow_distance::Read(roster[i]).preset;
            const bool active = !combat_orders::Enabled && (settings::Formation != formation_geometry::Style::Off) &&
                                TravelAllowed(player);
            if (!active || world != (player ? player->GetWorldspace() : nullptr)) {
                path.Clear();
                formation_ground::Clear();
                playerPosition.reset();
                holdingAnchor = false;
                world = player ? player->GetWorldspace() : nullptr;
            }
            if (active) {
                const auto position = player->GetPosition();
                const formation_rules::Point current{position.x, position.y, position.z};
                if (playerPosition && (formation_rules::Distance(current, *playerPosition) > 1024 ||
                                       std::abs(current.z - playerPosition->z) > 256)) {
                    path.Clear();
                    formation_ground::Clear();
                }
                playerPosition = current;
                formation_ground::Refresh(player);
                const auto anchor = formation_ground::Anchor(current);
                holdingAnchor = !anchor && path.Behind(0).has_value();
                if (anchor)
                    path.Observe(*anchor);
            }
            auto geometric =
                formation_geometry::Project(path, formation_geometry::Arrange(settings::Formation, presets));
            std::array<bool, formation_rules::Slots> eligible{};
            for (std::size_t i = 0; i < count; ++i)
                eligible[slots[i]] = active && Eligible(roster[i], player);
            for (std::size_t slot = 0; slot < geometric.size(); ++slot)
                if (!eligible[slot])
                    geometric[slot].reset();
            const auto destinations = formation_ground::Adjust(geometric);
            for (std::size_t i = 0; i < count; ++i) {
                auto* actor = roster[i];
                auto& follower = followers[actor->GetFormID()];
                follower.present = true;
                follower.actor = actor->GetHandle();
                follower.slot = slots[i];
                if (Rank(actor) < formation_rules::Slots * 2)
                    Mark(actor, follower.slot, formation_rules::OwnsOffset(Rank(actor)));
                follower.geometricDestination = geometric.at(static_cast<std::size_t>(follower.slot));
                follower.destination = destinations.at(static_cast<std::size_t>(follower.slot));
            }
        }

        bool MoveTarget(int slot, formation_rules::Point destination, RE::Actor* player) {
            auto* target = targets.at(static_cast<std::size_t>(slot));
            const auto position = target->GetPosition();
            const bool movedWorld = target->GetWorldspace() != player->GetWorldspace();
            const bool moved = movedWorld ||
                               formation_rules::Distance({position.x, position.y, position.z}, destination) >= 32 ||
                               std::abs(position.z - destination.z) >= 32;
            if (moved) {
                if (movedWorld)
                    target->MoveTo(player);
                // Only the invisible target moves. Followers use normal AI navigation.
                target->SetPosition({destination.x, destination.y, destination.z});
            }
            return moved;
        }

        void UpdateMovement(RE::Actor* actor, Follower& follower, RE::Actor* player, float elapsed) {
            const bool available = follower.destination && !follower.reset && !follower.pending && !follower.failed;
            if (!available) {
                follower.progress.Suspend();
                if (!formation_rules::OwnsOffset(Rank(actor)))
                    SetActive(actor, follower.slot, false);
                return;
            }
            const auto position = actor->GetPosition();
            const auto leaderPosition = player->GetPosition();
            const auto previousReason = follower.progress.Reason();
            const bool permitted =
                follower.progress.Observe({position.x, position.y, position.z}, *follower.destination,
                                          {leaderPosition.x, leaderPosition.y, leaderPosition.z}, elapsed);
            if (previousReason != follower.progress.Reason())
                debug::Trace("Formation.Recovery", actor, "reason={} retryAfter={}",
                             formation_rules::RecoveryName(follower.progress.Reason()), follower.progress.RetryAfter());
            if (!permitted) {
                SetActive(actor, follower.slot, false);
                follower.refreshSeconds = 0;
                follower.targetMoved = false;
                return;
            }
            follower.refreshSeconds += elapsed;
            follower.targetMoved |= MoveTarget(follower.slot, *follower.destination, player);
            const bool starting = !formation_rules::Active(Rank(actor));
            SetActive(actor, follower.slot, true);
            if (starting) {
                follower.refreshSeconds = 0;
                follower.targetMoved = false;
            } else if (follower.targetMoved && follower.refreshSeconds >= 1) {
                // A different package starts a fresh leg after a completed Travel procedure.
                SetTravelRank(actor, formation_rules::NextTravelRank(follower.slot, Rank(actor)));
                follower.refreshSeconds = 0;
                follower.targetMoved = false;
            }
        }

        // The only formation VM call clears offsets left by the previous preview.
        bool ClearSavedOffset(RE::Actor* actor, Follower& follower) {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!vm || vm->IsCompletelyFrozen() || !actor || !player || nextTicket == std::numeric_limits<int>::max())
                return false;
            const int ticket = ++nextTicket;
            follower.pending = Request{ticket, follower.resetRevision};
            follower.waitingSeconds = 0;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            const bool accepted = vm->DispatchStaticCall(
                mod::info::EngineScript, "Formation",
                RE::MakeFunctionArguments(static_cast<RE::Actor*>(actor), static_cast<RE::Actor*>(player), int{ticket},
                                          std::bit_cast<int>(static_cast<std::uint32_t>(Session() >> 32)),
                                          std::bit_cast<int>(static_cast<std::uint32_t>(Session())), false, 0.0f),
                callback);
            if (!accepted)
                follower.pending.reset();
            else
                logger::info("Clearing formation offset for {} ({:08X}), ticket={}", actor->GetName(),
                             actor->GetFormID(), ticket);
            return accepted;
        }

        bool Current(RE::StaticFunctionTag*, RE::Actor* actor, int ticket, int sessionHigh, int sessionLow) {
            std::scoped_lock lock(settings::Mutex);
            const auto found = actor ? followers.find(actor->GetFormID()) : followers.end();
            if (found == followers.end() || !found->second.pending)
                return false;
            const auto session = (std::uint64_t{std::bit_cast<std::uint32_t>(sessionHigh)} << 32) |
                                 std::bit_cast<std::uint32_t>(sessionLow);
            const auto& request = *found->second.pending;
            return formation_rules::CanExecute({Session(), found->first, request.ticket},
                                               {session, found->first, ticket}, loaded, true);
        }

        void Returned(RE::StaticFunctionTag*, RE::Actor* actor, int ticket, int sessionHigh, int sessionLow,
                      bool executed) {
            auto* tasks = SKSE::GetTaskInterface();
            if (!actor || !tasks)
                return;
            const auto handle = actor->GetHandle();
            tasks->AddTask([handle, ticket, sessionHigh, sessionLow, executed] {
                std::scoped_lock lock(settings::Mutex);
                auto reference = handle.get();
                if (!loaded || !reference || !marker)
                    return;
                auto* actor = reference.get();
                auto found = followers.find(actor->GetFormID());
                if (!SameSession(sessionHigh, sessionLow) || found == followers.end() || !found->second.pending ||
                    found->second.pending->ticket != ticket) {
                    // A saved VM stack can return after its native request was
                    // invalidated by loading. Reconcile its possible side effect
                    // against the current game's desired state, without replaying it.
                    if (executed) {
                        auto& follower = followers[actor->GetFormID()];
                        follower.actor = handle;
                        follower.slot = std::max(0, formation_rules::Slot(Rank(actor)));
                        follower.reset = true;
                        ++follower.resetRevision;
                        Mark(actor, follower.slot, true);
                    }
                } else {
                    auto& follower = found->second;
                    const auto request = *follower.pending;
                    follower.pending.reset();
                    follower.failed = false;
                    if (executed) {
                        // Preserve a reconciliation requested by a different stale completion.
                        if (follower.resetRevision == request.resetRevision)
                            follower.reset = false;
                        Mark(actor, follower.slot, follower.reset);
                    }
                    debug::Trace("Formation.ClearOffset", actor, "ticket={} executed={}", ticket, executed);
                    logger::info("Formation offset cleanup for {} ({:08X}) returned: executed={}", actor->GetName(),
                                 actor->GetFormID(), executed);
                }
                controller::executor::Wake();
            });
        }
    }

    void Configure(RE::TESFaction* value) {
        marker = value;
        auto* data = RE::TESDataHandler::GetSingleton();
        if (data)
            for (std::size_t i = 0; i < settlementKeywords.size(); ++i)
                settlementKeywords[i] =
                    data->LookupForm<RE::BGSKeyword>(formation_rules::SettlementKeywordForms[i], "Skyrim.esm");
        if (data)
            for (std::size_t i = 0; i < followPackages.size(); ++i)
                followPackages[i] =
                    data->LookupForm<RE::TESPackage>(static_cast<RE::FormID>(0x990 + i), mod::info::PluginFile);
        if (data)
            for (std::size_t i = 0; i < targets.size(); ++i) {
                targets[i] =
                    data->LookupForm<RE::TESObjectREFR>(static_cast<RE::FormID>(0x9B0 + i), mod::info::PluginFile);
            }
        if (data)
            for (std::size_t i = 0; i < travelPackages.size(); ++i) {
                const auto start = i < formation_rules::Slots ? 0x9A0 : 0x9C0;
                travelPackages[i] = data->LookupForm<RE::TESPackage>(
                    static_cast<RE::FormID>(start + i % formation_rules::Slots), mod::info::PluginFile);
            }
    }

    void Loaded() {
        std::scoped_lock lock(settings::Mutex);
        loaded = true;
        lastTick = std::chrono::steady_clock::now();
        if (auto* actors = RE::ProcessLists::GetSingleton())
            for (const auto& handle : actors->highActorHandles)
                if (const auto actor = handle.get())
                    ResetSaved(actor.get());
        for (auto* actor : Roster())
            ResetSaved(actor);
        controller::executor::Wake();
    }

    void Invalidate() {
        loaded = false;
        followers.clear();
        status = "No game loaded.";
        path.Clear();
        formation_ground::Clear();
        playerPosition.reset();
        holdingAnchor = false;
        world = nullptr;
    }

    void Changed() {
        std::scoped_lock lock(settings::Mutex);
        for (auto& [id, follower] : followers) {
            follower.progress.Suspend();
            if (!follower.pending)
                follower.failed = false;
        }
        controller::executor::Wake();
    }

    void Observe(RE::Actor* actor) {
        if (!actor)
            return;
        const auto handle = actor->GetHandle();
        const auto generation = debug::Generation();
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([handle, generation] {
                std::scoped_lock lock(settings::Mutex);
                if (!loaded || !debug::IsCurrentGame(generation))
                    return;
                if (auto actor = handle.get())
                    ResetSaved(actor.get());
                controller::executor::Wake();
            });
    }

    bool HasWork() {
        if (!loaded || !marker)
            return false;
        return (settings::Formation != formation_geometry::Style::Off) ||
               std::ranges::any_of(followers, [](const auto& entry) {
                   return entry.second.pending || entry.second.active || entry.second.reset;
               });
    }

    void RequestOffsetCleanup(RE::FormID actorID, std::uint64_t generation) {
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([actorID, generation] {
                std::scoped_lock lock(settings::Mutex);
                if (!loaded || !marker || !debug::IsCurrentGame(generation) || !debug::OptionsEnabled.load())
                    return;
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
                const auto roster = Roster();
                if (!actor || actor->IsDead() || std::ranges::find(roster, actor) == roster.end())
                    return;
                auto& follower = followers[actorID];
                if (follower.pending)
                    return;  // Do not replay an accepted cleanup, even after a timeout.
                follower.actor = actor->GetHandle();
                follower.slot = std::max(0, formation_rules::Slot(Rank(actor)));
                follower.reset = true;
                follower.failed = false;
                follower.destination.reset();
                ++follower.resetRevision;
                Mark(actor, follower.slot, true);
                controller::executor::Wake();
            });
    }

    void Tick() {
        std::scoped_lock lock(settings::Mutex);
        if (!loaded)
            return;
        if (!marker || std::ranges::find(targets, nullptr) != targets.end() ||
            std::ranges::find(travelPackages, nullptr) != travelPackages.end()) {
            status = "Formation records unavailable; install the matching ESP.";
            return;
        }
        auto* ui = RE::UI::GetSingleton();
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;
        if (!ui || ui->GameIsPaused())
            return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        Plan(player);
        std::size_t active = 0, pending = 0, failed = 0, recovering = 0, ungrounded = 0;
        for (auto& [id, follower] : followers) {
            auto actor = follower.actor.get();
            if (!actor)
                continue;
            if (follower.pending) {
                follower.waitingSeconds += elapsed;
                if (follower.waitingSeconds >= 30 && !follower.failed) {
                    follower.failed = true;
                    logger::warn("Formation request {} has not returned for actor {:08X}; it will not be replayed",
                                 follower.pending->ticket, id);
                }
            } else if (follower.reset && !follower.failed) {
                auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                if (vm && !vm->IsCompletelyFrozen() && !ClearSavedOffset(actor.get(), follower))
                    follower.failed = true;
            }
            pending += follower.pending.has_value();
            UpdateMovement(actor.get(), follower, player, elapsed);
            follower.active = formation_rules::Active(Rank(actor.get()));
            active += follower.active;
            failed += follower.failed;
            recovering += follower.progress.Reason() != formation_rules::RecoveryReason::None;
            ungrounded += follower.geometricDestination.has_value() && !follower.destination;
        }
        std::erase_if(followers, [](const auto& entry) {
            const auto& follower = entry.second;
            return !follower.present && !follower.pending && !follower.active && !follower.reset;
        });
        status = fmt::format(
            "{} | {} travel targets, {} recovering, {} without nearby ground, {} cleanup requests, {} blocked{}",
            formation_geometry::Name(settings::Formation), active, recovering, ungrounded, pending, failed,
            holdingAnchor ? " | holding last ground anchor" : "");
        if (InSettlement(player))
            status += " | suspended in settlement";
    }

    std::string Status() {
        std::scoped_lock lock(settings::Mutex);
        return status;
    }

    void WriteDiagnostics(std::ostream& out) {
        std::scoped_lock lock(settings::Mutex);
        const auto tickAge = std::chrono::duration<float>(std::chrono::steady_clock::now() - lastTick).count();
        auto* player = RE::PlayerCharacter::GetSingleton();
        out << "Formation: style=" << formation_geometry::Name(settings::Formation)
            << " enabled=" << (settings::Formation != formation_geometry::Style::Off) << " loaded=" << loaded
            << " travelAllowed=" << TravelAllowed(player) << " settlement=" << InSettlement(player)
            << " lastTickSeconds=" << tickAge << " status=" << status << '\n';
        out << "  navmeshTriangles=" << formation_ground::TriangleCount() << " holdingLastAnchor=" << holdingAnchor
            << '\n';
        for (auto* actor : Roster()) {
            const auto found = followers.find(actor->GetFormID());
            const int rank = Rank(actor);
            const int slot = formation_rules::Slot(rank);
            out << "  formation actor=" << fmt::format("{:08X}", actor->GetFormID()) << " name=" << actor->GetName()
                << " rank=" << rank << " slot=" << slot << " adapter=" << adapters::Owns(actor->GetFormID())
                << " eligible=" << (player && Eligible(actor, player)) << '\n';
            if (found != followers.end()) {
                const auto& follower = found->second;
                if (follower.geometricDestination) {
                    const auto point = *follower.geometricDestination;
                    out << "    geometric=" << point.x << ',' << point.y << ',' << point.z << '\n';
                }
                out << "    cachedSlot=" << follower.slot << " active=" << follower.active
                    << " cleanup=" << follower.reset << " failed=" << follower.failed
                    << " pending=" << (follower.pending ? follower.pending->ticket : 0)
                    << " recovery=" << formation_rules::RecoveryName(follower.progress.Reason())
                    << " retryAfter=" << follower.progress.RetryAfter()
                    << " stalledSeconds=" << follower.progress.StalledSeconds()
                    << " targetMoved=" << follower.targetMoved << " refreshSeconds=" << follower.refreshSeconds << '\n';
                if (follower.destination) {
                    const auto destination = *follower.destination;
                    out << "    desired=" << destination.x << ',' << destination.y << ',' << destination.z << '\n';
                } else {
                    out << "    desired=None\n";
                }
            } else {
                out << "    cached=None\n";
            }
            auto* target = slot >= 0 ? targets.at(static_cast<std::size_t>(slot)) : nullptr;
            if (!target) {
                out << "    target=None\n";
                continue;
            }
            const auto position = target->GetPosition();
            const auto actorPosition = actor->GetPosition();
            const auto* cell = target->GetParentCell();
            const auto* targetWorld = target->GetWorldspace();
            out << "    target=" << fmt::format("{:08X}", target->GetFormID()) << " position=" << position.x << ','
                << position.y << ',' << position.z << " cell=" << fmt::format("{:08X}", cell ? cell->GetFormID() : 0)
                << " world=" << fmt::format("{:08X}", targetWorld ? targetWorld->GetFormID() : 0) << " distanceXY="
                << formation_rules::Distance({actorPosition.x, actorPosition.y, actorPosition.z},
                                             {position.x, position.y, position.z})
                << " heightDifference=" << position.z - actorPosition.z << '\n';
        }
    }

    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("SetNativeFormationCurrent", mod::info::NativeScript, Current);
        vm->RegisterFunction("SetNativeFormationReturned", mod::info::NativeScript, Returned);
    }
}
