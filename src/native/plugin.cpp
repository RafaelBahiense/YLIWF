#include "RecordNames.h"
#include <Windows.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Settings.h"
#include "UI.h"
#include "Debug.h"
#include "Controller.h"
#include "ControllerStorage.h"
#include "Adapters.h"
#include "FollowDistance.h"
#include "PartyRules.h"

namespace {
    RE::TESGlobal* g_playerFollowerCount = nullptr;
    RE::TESGlobal* g_canRecruitMore = nullptr;
    RE::TESGlobal* g_currentFollowerCount = nullptr;
    RE::TESGlobal* g_followerSandbox = nullptr;
    RE::TESGlobal* g_followerHomes = nullptr;
    RE::TESGlobal* g_followDistance = nullptr;
    RE::TESFaction* g_currentFollowerFaction = nullptr;
    RE::TESFaction* g_potentialFollowerFaction = nullptr;
    RE::TESQuest* g_dialogueFollower = nullptr;
    RE::SpellItem* g_friendlyFireSpell = nullptr;
    RE::ActorHandle g_dialogueSpeaker;
    bool g_dialogueOpen = false;

    std::unordered_map<RE::FormID, std::uint8_t> g_essOrig{};
    std::int32_t g_slotCapacity = 8;

    constexpr const char* kRequiredPluginName = mod::info::PluginFile;
    std::int32_t GetEffectiveFollowerCap();
    std::int32_t GetTotalFollowerCapFromSettings();
    void SyncParty(bool pull);
    void ApplyFollowerDialogueGate(RE::Actor* speaker, std::int32_t count = -1);
    bool ApplyFollowerEssential(RE::StaticFunctionTag*, RE::Actor* actor);
    bool RestoreFollowerEssential(RE::StaticFunctionTag*, RE::Actor* actor);

    template <class T>
    T* ResolveForm(RE::FormID localID, std::string_view plugin, std::string_view editorID) {
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (auto* form = dh ? dh->LookupForm<T>(localID, plugin) : nullptr) {
            mod::debug::Trace("Native.ResolveForm", nullptr, "{}|{:08X} {} -> {:08X}", plugin, localID, editorID,
                              form->GetFormID());
            return form;
        }
        auto* form = RE::TESForm::LookupByEditorID(editorID);
        mod::debug::Trace("Native.ResolveForm.fallback", nullptr, "{}|{:08X} {} -> {:08X}", plugin, localID, editorID,
                          form ? form->GetFormID() : 0);
        return form ? form->As<T>() : nullptr;
    }

    void ResolveForms() {
        mod::debug::Trace("Native.ResolveForms.begin");
        g_playerFollowerCount =
            ResolveForm<RE::TESGlobal>(0x0BCC98, mod::record_names::SkyrimMaster, "PlayerFollowerCount");
        g_currentFollowerFaction =
            ResolveForm<RE::TESFaction>(0x05C84E, mod::record_names::SkyrimMaster, "CurrentFollowerFaction");
        g_potentialFollowerFaction =
            ResolveForm<RE::TESFaction>(0x05C84D, mod::record_names::SkyrimMaster, "PotentialFollowerFaction");
        g_dialogueFollower = ResolveForm<RE::TESQuest>(0x0750BA, mod::record_names::SkyrimMaster, "DialogueFollower");
        g_canRecruitMore = ResolveForm<RE::TESGlobal>(0x001, kRequiredPluginName, mod::record_names::CanRecruitMore);
        g_currentFollowerCount =
            ResolveForm<RE::TESGlobal>(0x002, kRequiredPluginName, mod::record_names::CurrentFollowerCount);
        g_followerSandbox = ResolveForm<RE::TESGlobal>(0x805, kRequiredPluginName, mod::record_names::FollowerSandbox);
        g_followerHomes = ResolveForm<RE::TESGlobal>(0x986, kRequiredPluginName, mod::record_names::FollowerHomes);
        g_followDistance = ResolveForm<RE::TESGlobal>(0x993, kRequiredPluginName, mod::record_names::FollowDistance);
        mod::follow_distance::Configure(
            ResolveForm<RE::TESFaction>(0x994, kRequiredPluginName, mod::record_names::FollowDistanceChoice));
        g_friendlyFireSpell =
            ResolveForm<RE::SpellItem>(0x800, kRequiredPluginName, mod::record_names::ProtectionSpell);
        mod::adapters::ConfigureCombatProtection(g_friendlyFireSpell);
        mod::debug::Configure(
            {g_dialogueFollower, g_currentFollowerFaction, g_potentialFollowerFaction, g_friendlyFireSpell,
             g_playerFollowerCount, g_currentFollowerCount, g_canRecruitMore, GetEffectiveFollowerCap,
             []() {
                 SyncParty(false);
                 ApplyFollowerDialogueGate(nullptr);
             },
             []() { return std::vector<std::pair<std::uint32_t, std::uint8_t>>(g_essOrig.begin(), g_essOrig.end()); },
             GetTotalFollowerCapFromSettings, mod::ui::IsRefreshVisible});
        mod::controller::Configure(
            {{g_dialogueFollower, g_currentFollowerFaction, g_potentialFollowerFaction,
              [](RE::Actor* actor) { ApplyFollowerEssential(nullptr, actor); }},
             [](RE::Actor* actor) { RestoreFollowerEssential(nullptr, actor); },
             GetEffectiveFollowerCap,
             ResolveForm<RE::TESQuest>(0x961, kRequiredPluginName, mod::record_names::HomeQuest),
             ResolveForm<RE::TESFaction>(0x960, kRequiredPluginName, mod::record_names::HomeFaction),
             [](std::int32_t count) { ApplyFollowerDialogueGate(nullptr, count); }});
        mod::debug::Trace("Native.ResolveForms.end");
    }

    void SetAbility(RE::Actor* a, bool want) {
        if (!a || !g_friendlyFireSpell) {
            mod::debug::Trace("Native.SetProtectionSpell.skip", a, "missing actor/spell");
            return;
        }
        if (a->HasSpell(g_friendlyFireSpell) == want) {
            mod::debug::Trace("Native.SetProtectionSpell.skip", a, "already matches setting");
            return;
        }
        if (want) {
            a->AddSpell(g_friendlyFireSpell);
        } else {
            a->RemoveSpell(g_friendlyFireSpell);
        }
        mod::debug::Trace("Native.SetProtectionSpell", a, want ? "added" : "removed");
    }

    void ApplyFriendlyFire() {
        std::scoped_lock lock(mod::settings::Mutex);
        mod::debug::Trace("Native.ApplyFriendlyFire", nullptr, mod::settings::FriendlyFire ? "on" : "off");
        SetAbility(RE::PlayerCharacter::GetSingleton(), mod::settings::FriendlyFire);
    }

    void ApplyCrossfireForActor(RE::Actor* a, bool want) {
        if (a != RE::PlayerCharacter::GetSingleton())
            SetAbility(a, want);
    }

    bool IsValidActor(RE::Actor* a) {
        return a && a != RE::PlayerCharacter::GetSingleton() && !a->IsDead();
    }

    bool FileExistsA(const char* path) {
        DWORD attrs = GetFileAttributesA(path);
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    [[noreturn]] void MessageAndExit(const char* msg) {
        MessageBoxA(nullptr, msg, mod::info::DllFile, MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
        ExitProcess(1);
    }

    void EarlyPreflightCheck() {
        std::string espPath = std::string("Data\\") + kRequiredPluginName;
        if (!FileExistsA(espPath.c_str()))
            MessageAndExit(
                fmt::format("Missing required file:\n\n{}\nInstall it (or fix your mod manager / VFS), then relaunch.",
                            espPath)
                    .c_str());
    }

    void ApplySandbox() {
        std::scoped_lock lock(mod::settings::Mutex);
        mod::debug::Trace("Native.ApplySandbox", nullptr, mod::settings::FollowerSandbox ? "on" : "off");
        if (g_followerSandbox)
            g_followerSandbox->value = mod::settings::FollowerSandbox ? 1.0f : 0.0f;
    }

    void ApplyHomes() {
        std::scoped_lock lock(mod::settings::Mutex);
        mod::debug::Trace("Native.ApplyHomes", nullptr, mod::settings::FollowerHomes ? "on" : "off");
        if (g_followerHomes)
            g_followerHomes->value = mod::settings::FollowerHomes ? 1.0f : 0.0f;
    }

    void ApplyFollowDistance() {
        std::scoped_lock lock(mod::settings::Mutex);
        if (g_followDistance)
            g_followDistance->value = static_cast<float>(mod::settings::FollowDistance);
        mod::follow_distance::ApplyAll();
    }

    bool HasPerkFromSpec(const std::string& file, std::uint32_t localID) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return false;
        auto* dh = RE::TESDataHandler::GetSingleton();
        if (!dh)
            return false;
        auto* mod = dh->LookupModByName(file);
        if (!mod)
            return false;
        auto* perk = dh->LookupForm<RE::BGSPerk>(localID & (mod->IsLight() ? 0xFFFu : 0xFFFFFFu), file);
        return perk && player->HasPerk(perk);
    }

    std::int32_t CountOwnedPerksFromList() {
        std::scoped_lock lock(mod::settings::Mutex);
        std::int32_t owned = 0;
        for (std::size_t i = 0; i < mod::settings::PerkSpecCount; ++i) {
            const auto& p = mod::settings::PerkSpecs[i];
            if (HasPerkFromSpec(p.file, p.localID))
                ++owned;
        }
        return owned;
    }

    std::int32_t GetSpeechBasedFollowerCap() {
        std::scoped_lock lock(mod::settings::Mutex);
        constexpr std::int32_t kBase = 1, kMaxExtras = 7;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return kBase;

        const auto speech =
            static_cast<std::int32_t>(player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeech));
        const std::int32_t levelsPerSlot = std::max(mod::settings::SpeechLevelsPerSlot, 1);
        std::int32_t total = kBase + (speech / levelsPerSlot);
        return std::clamp(total, 1, kBase + kMaxExtras);
    }

    std::int32_t GetTotalFollowerCapFromSettings() {
        std::scoped_lock lock(mod::settings::Mutex);
        constexpr std::int32_t kBase = 1, kMaxExtras = 7;

        if (mod::settings::FollowerPerkOption == 0)
            return kBase + std::clamp(static_cast<int>(mod::settings::MaxExtraFollowers), 0, kMaxExtras);
        if (mod::settings::FollowerPerkOption == 1)
            return std::clamp(kBase + CountOwnedPerksFromList(), 1, kBase + kMaxExtras);
        return GetSpeechBasedFollowerCap();
    }

    std::int32_t GetEffectiveFollowerCap() {
        return std::min(GetTotalFollowerCapFromSettings(), g_slotCapacity);
    }

    void CountFollowerSlots() {
        if (!g_dialogueFollower) {
            return;
        }
        std::int32_t slots = 1;
        for (auto* alias : g_dialogueFollower->aliases) {
            if (alias &&
                std::string_view(alias->aliasName.c_str()).starts_with(mod::record_names::aliases::ExtraPrefix)) {
                ++slots;
            }
        }
        g_slotCapacity = slots;
        mod::debug::Trace("Native.CountFollowerSlots", nullptr, "slots={}", slots);
    }

    void ApplyFollowerDialogueGate(RE::Actor* speaker, std::int32_t count) {
        std::scoped_lock lock(mod::settings::Mutex);
        if (!g_playerFollowerCount)
            return;

        const auto currentSpeaker = g_dialogueSpeaker.get();
        if (!speaker && g_dialogueOpen)
            speaker = currentSpeaker.get();
        if (count < 0)
            count = g_currentFollowerCount ? std::max(static_cast<int>(g_currentFollowerCount->value), 0) : 0;
        const bool canRecruitMore = count < GetEffectiveFollowerCap();
        const bool hireable = speaker && !speaker->IsDead() && g_potentialFollowerFaction &&
                              speaker->IsInFaction(g_potentialFollowerFaction) && !speaker->IsPlayerTeammate() &&
                              !(g_currentFollowerFaction && speaker->IsInFaction(g_currentFollowerFaction)) &&
                              !mod::adapters::Owns(speaker->GetFormID());
        const bool adapterRecruitable = speaker && !speaker->IsDead() && !speaker->IsPlayerTeammate() &&
                                        mod::adapters::CanRecruitThroughDialogue(speaker->GetFormID());
        g_playerFollowerCount->value = mod::party_rules::DialogueFollowerCount(
            count, GetEffectiveFollowerCap(), hireable, adapterRecruitable, mod::adapters::HasFollowers());
        if (g_canRecruitMore)
            g_canRecruitMore->value = canRecruitMore ? 1.0f : 0.0f;
        mod::debug::TraceLazy("Native.ApplyFollowerDialogueGate", speaker, [&] {
            return fmt::format("count={} cap={} hireable={} adapterRecruitable={} recruit={} vanillaGate={}", count,
                               GetEffectiveFollowerCap(), hireable, adapterRecruitable, canRecruitMore,
                               g_playerFollowerCount->value);
        });
    }

    bool IsInServiceEssential(RE::Actor* a) {
        return a && g_currentFollowerFaction && a->IsInFaction(g_currentFollowerFaction) && a->IsPlayerTeammate();
    }

    void SetBaseFlag(RE::TESNPC* base, RE::ACTOR_BASE_DATA::Flag flag, bool on) {
        if (on) {
            base->actorData.actorBaseFlags.set(flag);
        } else {
            base->actorData.actorBaseFlags.reset(flag);
        }
    }

    void ApplyOriginalFlags(RE::TESNPC* base, std::uint8_t orig) {
        SetBaseFlag(base, RE::ACTOR_BASE_DATA::Flag::kEssential, orig & 1);
        SetBaseFlag(base, RE::ACTOR_BASE_DATA::Flag::kProtected, orig & 2);
    }

    bool RestoreEssentialFlags(RE::TESNPC* base) {
        auto it = g_essOrig.find(base->GetFormID());
        if (it == g_essOrig.end())
            return false;
        ApplyOriginalFlags(base, it->second);
        g_essOrig.erase(it);
        mod::debug::Trace("Native.RestoreEssentialFlags", nullptr, "base={:08X}", base->GetFormID());
        return true;
    }

    void RestoreAllEssentialFlags() {
        mod::debug::Trace("Native.RestoreAllEssentialFlags");
        for (const auto& [formID, orig] : g_essOrig) {
            if (auto* base = RE::TESForm::LookupByID<RE::TESNPC>(formID)) {
                ApplyOriginalFlags(base, orig);
            }
        }
        g_essOrig.clear();
    }

    void UpdateEssentialForActor(RE::Actor* a) {
        std::scoped_lock lock(mod::settings::Mutex);
        mod::debug::Trace("Native.UpdateEssentialForActor", a);
        auto* base = a->GetActorBase();
        if (!base)
            return;
        if (!mod::settings::FollowerEssential || !IsInServiceEssential(a)) {
            RestoreEssentialFlags(base);
            return;
        }
        auto& flags = base->actorData.actorBaseFlags;
        g_essOrig.try_emplace(base->GetFormID(),
                              static_cast<std::uint8_t>((flags.any(RE::ACTOR_BASE_DATA::Flag::kEssential) ? 1 : 0) |
                                                        (flags.any(RE::ACTOR_BASE_DATA::Flag::kProtected) ? 2 : 0)));
        flags.set(RE::ACTOR_BASE_DATA::Flag::kEssential);
        flags.reset(RE::ACTOR_BASE_DATA::Flag::kProtected);
    }

    void SyncParty(bool pull) {
        std::scoped_lock lock(mod::settings::Mutex);
        mod::debug::Trace("Native.SyncParty.begin", nullptr, pull ? "pull=true" : "pull=false");
        mod::adapters::ApplyCombatProtection();
        if (!g_dialogueFollower)
            return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return;
        for (auto* alias : g_dialogueFollower->aliases) {
            if (!alias || alias->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID)
                continue;
            auto* a = static_cast<RE::BGSRefAlias*>(alias)->GetActorReference();
            if (!a)
                continue;
            const bool inService = IsInServiceEssential(a);
            UpdateEssentialForActor(a);
            ApplyCrossfireForActor(a, mod::settings::FollowerCrossfire && inService);
            if (pull && inService && !a->IsOnMount() &&
                a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) == 0.0f &&
                a->GetParentCell() != player->GetParentCell()) {
                mod::debug::Trace("Native.FastTravelPull", a);
                a->MoveTo(player);
            }
        }
        mod::debug::Trace("Native.SyncParty.end");
    }

    template <class Fn>
    void QueueGameTask(Fn fn) {
        const auto generation = mod::debug::Generation();
        if (auto* task = SKSE::GetTaskInterface())
            task->AddTask([fn, generation]() {
                if (mod::debug::IsCurrentGame(generation)) {
                    fn();
                    mod::debug::NotifyStateChanged();
                }
            });
    }

    void DeferSyncParty(bool pull = false) {
        mod::debug::Trace("Native.DeferSyncParty", nullptr, pull ? "pull=true" : "pull=false");
        QueueGameTask([pull]() { SyncParty(pull); });
    }

    bool ApplyFollowerEssential(RE::StaticFunctionTag*, RE::Actor* a) {
        std::scoped_lock lock(mod::settings::Mutex);
        mod::debug::Trace("Native.ApplyFollowerEssential", a);
        if (!IsValidActor(a)) {
            mod::debug::Trace("Native.ApplyFollowerEssential.result", a, "rejected invalid/player/dead actor");
            return false;
        }
        UpdateEssentialForActor(a);
        ApplyCrossfireForActor(a, mod::settings::FollowerCrossfire && IsInServiceEssential(a));
        mod::debug::Trace("Native.ApplyFollowerEssential.result", a, "success=true");
        return true;
    }

    bool RestoreFollowerEssential(RE::StaticFunctionTag*, RE::Actor* a) {
        mod::debug::Trace("Native.RestoreFollowerEssential", a);
        if (!a)
            return false;
        ApplyCrossfireForActor(a, false);
        auto* base = a->GetActorBase();
        const bool restored = base && RestoreEssentialFlags(base);
        mod::debug::Trace("Native.RestoreFollowerEssential.result", a,
                          restored ? "original flags restored" : "original flags not cached");
        return restored;
    }

    bool RegisterPapyrus(RE::BSScript::IVirtualMachine* vm) {
        mod::debug::RegisterPapyrus(vm);
        mod::controller::RegisterPapyrus(vm);
        return true;
    }

    class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static MenuSink* GetSingleton() {
            static MenuSink s;
            return &s;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* e,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
            if (e->menuName == "Main Menu" && e->opening) {
                std::scoped_lock lock(mod::settings::Mutex);
                g_dialogueSpeaker.reset();
                g_dialogueOpen = false;
                mod::controller::Invalidate();
                ApplyFollowerDialogueGate(nullptr);
            }
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([] { mod::controller::ObservePause(); });
            if (e->menuName == "Dialogue Menu") {
                std::scoped_lock lock(mod::settings::Mutex);
                mod::debug::Trace("Native.DialogueMenu", nullptr, e->opening ? "opened" : "closed");
                auto* topics = RE::MenuTopicManager::GetSingleton();
                auto speaker = (e->opening && topics) ? topics->speaker.get() : nullptr;
                g_dialogueOpen = e->opening;
                g_dialogueSpeaker =
                    speaker && speaker->As<RE::Actor>() ? speaker->As<RE::Actor>()->GetHandle() : RE::ActorHandle{};
                ApplyFollowerDialogueGate(speaker ? speaker->As<RE::Actor>() : nullptr);
                // Event delivery may be off-thread. Refresh optional adapter
                // eligibility on the main thread before the player selects a reply.
                QueueGameTask([] { ApplyFollowerDialogueGate(nullptr); });
                if (e->opening && mod::settings::FollowerCrossfire)
                    DeferSyncParty();
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class ActivateSink final : public RE::BSTEventSink<RE::TESActivateEvent> {
    public:
        static ActivateSink* GetSingleton() {
            static ActivateSink s;
            return &s;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* e,
                                              RE::BSTEventSource<RE::TESActivateEvent>*) override {
            if (e->actionRef.get() != RE::PlayerCharacter::GetSingleton() || !e->objectActivated)
                return RE::BSEventNotifyControl::kContinue;
            if (auto* actor = e->objectActivated->As<RE::Actor>()) {
                mod::debug::Trace("Native.PlayerActivate", actor);
                ApplyFollowerDialogueGate(actor);
                QueueGameTask([] { ApplyFollowerDialogueGate(nullptr); });
                mod::controller::Activated(actor);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class TravelSink final : public RE::BSTEventSink<RE::TESFastTravelEndEvent> {
    public:
        static TravelSink* GetSingleton() {
            static TravelSink s;
            return &s;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESFastTravelEndEvent*,
                                              RE::BSTEventSource<RE::TESFastTravelEndEvent>*) override {
            mod::debug::Trace("Native.FastTravelEnd");
            DeferSyncParty(true);
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    class FollowerEvents final : public RE::BSTEventSink<RE::TESDeathEvent>,
                                 public RE::BSTEventSink<RE::TESCombatEvent>,
                                 public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
    public:
        static FollowerEvents* GetSingleton() {
            static FollowerEvents sink;
            return &sink;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event,
                                              RE::BSTEventSource<RE::TESDeathEvent>*) override {
            if (event && event->dead && event->actorDying)
                mod::controller::Died(event->actorDying->As<RE::Actor>());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* event,
                                              RE::BSTEventSource<RE::TESCombatEvent>*) override {
            if (event && event->actor && event->targetActor)
                mod::controller::CombatChanged(event->actor->As<RE::Actor>(), event->targetActor->As<RE::Actor>());
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,
                                              RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
            if (event && !event->loaded)
                mod::controller::Unloaded(RE::TESForm::LookupByID<RE::Actor>(event->formID));
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void Install() {
        mod::debug::Trace("Native.InstallEventSinks");
        if (auto* ui = RE::UI::GetSingleton())
            ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
        auto* events = RE::ScriptEventSourceHolder::GetSingleton();
        if (!events)
            return;
        events->AddEventSink<RE::TESActivateEvent>(ActivateSink::GetSingleton());
        events->AddEventSink<RE::TESDeathEvent>(FollowerEvents::GetSingleton());
        events->AddEventSink<RE::TESCombatEvent>(FollowerEvents::GetSingleton());
        events->AddEventSink<RE::TESObjectLoadedEvent>(FollowerEvents::GetSingleton());
        if (events->GetEventSource<RE::TESFastTravelEndEvent>())
            events->AddEventSink<RE::TESFastTravelEndEvent>(TravelSink::GetSingleton());
    }

    void InitializeGameData() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data || (!data->LookupLoadedModByName(kRequiredPluginName) &&
                      !data->LookupLoadedLightModByName(kRequiredPluginName)))
            MessageAndExit(fmt::format("Missing required plugin in load order:\n{}\nEnable it in your active "
                                       "mod-manager profile, then relaunch.",
                                       kRequiredPluginName)
                               .c_str());
        ResolveForms();
        if (!g_currentFollowerCount)
            MessageAndExit(
                fmt::format("Missing required plugin in load order:\n{}\nEnable it in your load order, then relaunch.",
                            mod::info::PluginFile)
                    .c_str());
        CountFollowerSlots();
        ApplyFollowerDialogueGate(nullptr);
        Install();
    }

    void InvalidateGameState() {
        std::scoped_lock lock(mod::settings::Mutex);
        g_dialogueSpeaker.reset();
        g_dialogueOpen = false;
        mod::controller::Invalidate();
        mod::debug::Invalidate();
        RestoreAllEssentialFlags();
    }

    void ApplyLoadedGameState() {
        mod::settings::Load(true);
        ApplyFollowerDialogueGate(nullptr);
        ApplyFriendlyFire();
        ApplySandbox();
        ApplyHomes();
        DeferSyncParty();
        mod::debug::GameLoaded();
        mod::controller::GameLoaded();
        mod::follow_distance::Loaded();
    }

    void OnMessage(SKSE::MessagingInterface::Message* msg) {
        mod::debug::Trace("Native.SKSEMessage", nullptr, "type={}", msg->type);
        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            InitializeGameData();
        } else if (msg->type == SKSE::MessagingInterface::kPreLoadGame) {
            InvalidateGameState();
        } else if (msg->type == SKSE::MessagingInterface::kPostLoadGame ||
                   msg->type == SKSE::MessagingInterface::kNewGame) {
            // SKSE passes the success bool as the pointer value, not a bool pointer.
            if (msg->type == SKSE::MessagingInterface::kPostLoadGame && !msg->data) {
                logger::warn("Save load failed; debug controls remain unavailable.");
                return;
            }
            if (msg->type == SKSE::MessagingInterface::kNewGame)
                InvalidateGameState();
            ApplyLoadedGameState();
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    EarlyPreflightCheck();
    SKSE::Init(skse);
    mod::debug::InitializeLog();
    mod::settings::Load();
    mod::controller::storage::RegisterSerialization();

    mod::settings::ApplyGateCallback = []() { QueueGameTask([]() { ApplyFollowerDialogueGate(nullptr); }); };
    mod::settings::FriendlyFireCallback = []() { QueueGameTask(ApplyFriendlyFire); };
    mod::settings::SandboxCallback = []() { QueueGameTask(ApplySandbox); };
    mod::settings::HomesCallback = []() { QueueGameTask(ApplyHomes); };
    mod::settings::FollowDistanceCallback = []() { QueueGameTask(ApplyFollowDistance); };
    mod::settings::EssentialCallback = []() { DeferSyncParty(); };
    mod::settings::CrossfireCallback = []() { DeferSyncParty(); };

    if (auto* papyrus = SKSE::GetPapyrusInterface())
        papyrus->Register(RegisterPapyrus);
    if (auto* messaging = SKSE::GetMessagingInterface())
        messaging->RegisterListener(OnMessage);

    mod::ui::Register();
    return true;
}
