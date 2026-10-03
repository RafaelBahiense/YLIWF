#include "Debug.h"

#include "DebugRules.h"
#include "Controller.h"
#include "ControllerStorage.h"
#include "Refresh.h"
#include "FollowerView.h"
#include "Settings.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_set>

// Windows GDI's macro collides with BSScript::Variable::GetObject.
#ifdef GetObject
#undef GetObject
#endif

namespace mod::debug {
    namespace {
        Context context;
        std::mutex snapshotMutex;
        Snapshot snapshot;
        std::filesystem::path logDirectory;
        bool fileLogAvailable = false;
        std::atomic_uint64_t epoch{1}, sequence{0};
        std::atomic_bool gameReady{false};
        std::mutex refreshMutex;
        mod::refresh::State refresh;
        std::atomic_int32_t activeTicket{0};
        std::atomic_int32_t ticketSequence{static_cast<std::int32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count() & 0x3FFFFFFF)};

        std::string Form(RE::TESForm* form) {
            if (!form) return "None";
            auto* file = form->GetFile(0);
            auto* winner = form->GetFile(-1);
            return fmt::format("{:08X} [origin={} last={}] {} {}", form->GetFormID(), file ? file->fileName : "dynamic", winner ? winner->fileName : "dynamic",
                form->GetFormEditorID(), form->GetName());
        }

        bool PartyAlias(const RE::BGSBaseAlias* alias) {
            if (!alias || alias->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID) return false;
            const std::string_view name{alias->aliasName.c_str()};
            return alias->aliasID == 0 || name.starts_with("ExtraFollower");
        }

        mod::follower_view::Space ActorSpace(RE::Actor* actor) {
            auto* cell = actor ? actor->GetParentCell() : nullptr;
            auto* world = actor ? actor->GetWorldspace() : nullptr;
            return {cell ? cell->GetFormID() : 0, world ? world->GetFormID() : 0,
                cell && cell->IsInteriorCell(), actor && actor->Is3DLoaded() && !actor->IsDisabled()};
        }
        void CapturePosition(Follower& row, RE::Actor* actor) {
            auto* location = actor->GetCurrentLocation();
            auto* cell = actor->GetParentCell();
            auto* world = actor->GetWorldspace();
            for (auto* form : {static_cast<RE::TESForm*>(location), static_cast<RE::TESForm*>(cell), static_cast<RE::TESForm*>(world)}) {
                if (form && form->GetName() && *form->GetName()) { row.location = form->GetName(); break; }
            }
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) { row.distanceUnavailable = "Player position unavailable"; return; }
            const auto actorSpace = ActorSpace(actor), playerSpace = ActorSpace(player);
            if (!actorSpace.loaded || !playerSpace.loaded) row.distanceUnavailable = "Actor position is not loaded";
            else if (!mod::follower_view::Comparable(actorSpace, playerSpace)) row.distanceUnavailable = "Different area or unknown coordinate space";
            const auto position = actor->GetPosition(), playerPosition = player->GetPosition();
            row.distanceMeters = mod::follower_view::DistanceMeters(actorSpace, playerSpace,
                {position.x, position.y, position.z}, {playerPosition.x, playerPosition.y, playerPosition.z});
            if (!row.distanceMeters && row.distanceUnavailable.empty()) row.distanceUnavailable = "Position unavailable";
        }
        Follower ActorState(RE::Actor* actor) {
            Follower row;
            if (!actor) return row;
            row.formID = actor->GetFormID();
            row.name = actor->GetName();
            auto* base = actor->GetActorBase();
            auto* cell = actor->GetParentCell();
            auto* package = actor->GetCurrentPackage();
            row.baseID = base ? base->GetFormID() : 0;
            row.cellID = cell ? cell->GetFormID() : 0;
            row.packageID = package ? package->GetFormID() : 0;
            row.dead = actor->IsDead();
            row.teammate = actor->IsPlayerTeammate();
            row.currentFaction = context.currentFaction && actor->IsInFaction(context.currentFaction);
            row.potentialFaction = context.potentialFaction && actor->IsInFaction(context.potentialFaction);
            row.loaded = actor->Is3DLoaded();
            row.essential = actor->IsEssential();
            row.protectedActor = actor->IsProtected();
            row.crossfire = context.protectionSpell && actor->HasSpell(context.protectionSpell);
            row.waiting = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer);
            CapturePosition(row, actor);
            return row;
        }

        void WriteActor(std::ostream& out, RE::Actor* actor) {
            if (!actor) return;
            const auto row = ActorState(actor);
            out << "    actor=" << Form(actor) << " base=" << Form(actor->GetActorBase()) << '\n'
                << "    dead=" << row.dead << " teammate=" << row.teammate << " currentFaction=" << row.currentFaction
                << " potentialFaction=" << row.potentialFaction << " loaded=" << row.loaded << " waiting=" << row.waiting
                << " essential=" << row.essential << " protected=" << row.protectedActor << " protectionSpell=" << row.crossfire << '\n'
                << "    cell=" << Form(actor->GetParentCell()) << " package=" << Form(actor->GetCurrentPackage()) << '\n';
            auto pos = actor->GetPosition();
            out << "    position=" << pos.x << ',' << pos.y << ',' << pos.z << " combat=" << actor->IsInCombat()
                << " disabled=" << actor->IsDisabled() << " mounted=" << actor->IsOnMount()
                << " worldspace=" << Form(actor->GetWorldspace()) << '\n';
            out << "    health=" << actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth)
                << " speech=" << actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeech) << '\n';
            // Include every faction on the NPC base and changes in the reference extra data.
            if (auto* base = actor->GetActorBase()) {
                out << "    baseFlags=" << base->actorData.actorBaseFlags.underlying() << '\n';
                for (const auto& entry : base->factions) {
                    out << "    faction=" << Form(entry.faction) << " baseRank=" << static_cast<int>(entry.rank)
                        << " effectiveRank=" << actor->GetFactionRank(entry.faction, false) << '\n';
                }
            }
            if (auto* changes = actor->extraList.GetByType<RE::ExtraFactionChanges>()) {
                for (const auto& entry : changes->factionChanges) {
                    out << "    changedFaction=" << Form(entry.faction) << " rank=" << static_cast<int>(entry.rank) << '\n';
                }
            }
        }

        using ScriptObject = RE::BSTSmartPointer<RE::BSScript::Object>;
        RE::TESForm* ResolveScriptForm(const ScriptObject& object) {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm || !object || !object->GetTypeInfo()) return nullptr;
            // Resolve the concrete engine type before walking to generic Form.
            // A GlobalVariable handle cannot be resolved using Form's type ID.
            for (auto* type = object->GetTypeInfo(); type; type = type->GetParent()) {
                RE::VMTypeID id = 0;
                if (vm->GetTypeIDForScriptObject(type->name, id) && id > 0 && id < static_cast<RE::VMTypeID>(RE::FormType::Max)) {
                    if (auto* form = object->Resolve(id)) return static_cast<RE::TESForm*>(form);
                }
            }
            return nullptr;
        }
        std::vector<ScriptObject> BoundObjects(RE::VMTypeID type, const void* object) {
            std::vector<ScriptObject> objects;
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
            if (!policy || !object) return objects;
            const auto handle = policy->GetHandleForObject(type, object);
            // Copy smart pointers while holding the engine lock; release it before VM calls.
            RE::BSSpinLockGuard lock(vm->attachedScriptsLock);
            const auto found = vm->attachedScripts.find(handle);
            if (found != vm->attachedScripts.end()) {
                for (const auto& attached : found->second) {
                    if (attached) objects.emplace_back(attached.get());
                }
            }
            return objects;
        }

        void WriteVariable(std::ostream& out, const RE::BSScript::Variable& value) {
            if (value.IsArray()) {
                const auto array = value.GetArray();
                out << '[';
                if (array) {
                    bool first = true;
                    for (const auto& element : *array) {
                        if (!first) out << ", ";
                        first = false;
                        WriteVariable(out, element);
                    }
                }
                out << ']';
            } else if (value.IsBool()) out << value.GetBool();
            else if (value.IsInt()) out << value.GetSInt();
            else if (value.IsFloat()) out << value.GetFloat();
            else if (value.IsString()) out << std::quoted(std::string(value.GetString()));
            else if (value.IsObject()) {
                auto object = value.GetObject();
                if (!object) { out << "None"; return; }
                out << object->GetTypeInfo()->GetName() << " handle=" << object->GetHandle();
                // Resolve only known native types. Referenced objects are summarized, not recursively walked.
                if (auto* form = ResolveScriptForm(object)) {
                    out << " {" << Form(form);
                    if (auto* global = form->As<RE::TESGlobal>()) out << " value=" << global->value;
                    out << '}';
                } else if (auto* alias = static_cast<RE::BGSBaseAlias*>(object->Resolve(RE::BGSRefAlias::VMTYPEID))) {
                    out << " {quest=" << Form(alias->owningQuest) << " alias=" << alias->aliasID << " name=" << alias->aliasName.c_str() << '}';
                }
            } else out << "None";
        }

        template <class Visitor>
        void VisitVariables(const ScriptObject& object, Visitor visitor) {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm || !object || !object->IsInitialized()) return;
            std::vector<const RE::BSScript::ObjectTypeInfo*> types;
            for (auto* type = object->GetTypeInfo(); type; type = type->GetParent()) types.push_back(type);
            std::uint32_t index = 0;
            // Papyrus stores inherited variables first.
            for (auto it = types.rbegin(); it != types.rend(); ++it) {
                const auto* vars = (*it)->GetVariableIter();
                for (std::uint32_t i = 0; vars && i < (*it)->GetNumVariables(); ++i, ++index) {
                    RE::BSScript::Variable value;
                    const bool available = vm->GetVariableValue(object, index, value);
                    visitor(vars[i].name.c_str(), value, available);
                }
            }
        }

        void CheckInstallation(Snapshot& result) {
            result.configuredCap = context.getConfiguredCap ? context.getConfiguredCap() : result.cap;
            auto* winner = context.quest ? context.quest->GetFile(-1) : nullptr;
            result.winningPlugin = winner ? winner->fileName : "unknown";
            std::array<RE::BGSRefAlias*, 8> slots{};
            std::array<bool, 8> seen{};
            std::string structureIssues;
            bool invalidAlias = false;
            if (!context.quest) mod::debug_rules::AddIssue(structureIssues, "DialogueFollower quest unavailable");
            if (context.quest) for (auto* alias : context.quest->aliases) {
                if (!alias) continue;
                const auto id = alias->aliasID;
                if (id != 0 && (id < 2 || id > 8)) continue;
                const auto index = id == 0 ? 0 : id - 1;
                const auto expected = id == 0 ? std::string("Follower") : fmt::format("ExtraFollower{:02}", id - 1);
                std::string issues;
                if (alias->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID)
                    mod::debug_rules::AddIssue(issues, fmt::format("type '{}' is not ReferenceAlias", alias->GetTypeString().c_str()).c_str());
                if (alias->owningQuest != context.quest)
                    mod::debug_rules::AddIssue(issues, fmt::format("owning quest {} differs from DialogueFollower", Form(alias->owningQuest)).c_str());
                // BSFixedString comparisons ignore case, as the engine does. Comparing
                // c_str() as string_view incorrectly rejects names such as "follower".
                if (alias->aliasName != std::string_view(expected))
                    mod::debug_rules::AddIssue(issues, fmt::format("name '{}' differs from '{}'", alias->aliasName.c_str(), expected).c_str());
                if (seen[index]) mod::debug_rules::AddIssue(issues, "duplicate alias ID");
                seen[index] = true;
                if (!issues.empty()) {
                    invalidAlias = true;
                    mod::debug_rules::AddIssue(structureIssues, fmt::format("alias {}: {}", id, issues).c_str());
                    continue;
                }
                slots[index] = static_cast<RE::BGSRefAlias*>(alias);
                ++result.slotCapacity;
            }
            if (context.quest) for (std::size_t index = 0; index < seen.size(); ++index)
                if (!seen[index]) mod::debug_rules::AddIssue(structureIssues,
                    fmt::format("missing alias {} ({})", index == 0 ? 0 : index + 1,
                        index == 0 ? std::string("Follower") : fmt::format("ExtraFollower{:02}", index)).c_str());
            if (!context.vanillaCount) mod::debug_rules::AddIssue(structureIssues, "PlayerFollowerCount global unavailable");
            if (!context.modCount) mod::debug_rules::AddIssue(structureIssues, "YLIWF_CurrentFollowerCount global unavailable");
            if (!context.recruitGate) mod::debug_rules::AddIssue(structureIssues, "YLIWF_CanRecruitMore global unavailable");
            result.structureReady = !invalidAlias && result.slotCapacity == 8 && context.vanillaCount && context.modCount && context.recruitGate;
            bool primary = false, vanilla = false;
            for (std::size_t i = 1; i < slots.size(); ++i) if (slots[i]) ++result.boundExtraAliases;
            if (context.quest) for (const auto& object : BoundObjects(static_cast<RE::VMTypeID>(RE::FormType::Quest), context.quest)) {
                bool followerScript = false;
                for (auto* type = object->GetTypeInfo(); type; type = type->GetParent())
                    if (mod::debug_rules::IsFollowerScript(type->GetName())) followerScript = true;
                if (!followerScript || !object->IsInitialized()) continue;
                result.scriptReady = true;
                VisitVariables(object, [&](const char* name, const RE::BSScript::Variable& value, bool available) {
                    if (!available) return;
                    const std::string_view variable(name);
                    if (value.IsObject()) {
                        const auto binding = value.GetObject();
                        if (!binding) return;
                        if (mod::strings::EqualsIgnoreCase(variable, "::pFollowerAlias_var")) primary = slots[0] && binding->Resolve(RE::BGSRefAlias::VMTYPEID) == slots[0];
                        else if (mod::strings::EqualsIgnoreCase(variable, "::pPlayerFollowerCount_var")) vanilla = context.vanillaCount && binding->Resolve(static_cast<RE::VMTypeID>(RE::FormType::Global)) == context.vanillaCount;
                    }
                });
            }
            result.bindingsReady = primary && vanilla && result.structureReady && mod::controller::storage::Available();
            result.installationIssues = mod::debug_rules::InstallationIssues(result.structureReady, result.scriptReady, result.bindingsReady, result.configuredCap, result.slotCapacity);
            if (!structureIssues.empty()) result.installationIssues += " (runtime validation: " + structureIssues + ')';
            if (result.scriptReady && !result.bindingsReady) {
                std::vector<std::string_view> invalid;
                if (!primary) invalid.emplace_back("pFollowerAlias");
                if (!vanilla) invalid.emplace_back("pPlayerFollowerCount");
                if (!mod::controller::storage::Available()) invalid.emplace_back("native controller state unavailable");
                result.installationIssues += " (invalid bindings: ";
                for (std::size_t i = 0; i < invalid.size(); ++i) {
                    if (i) result.installationIssues += "; ";
                    result.installationIssues += invalid[i];
                }
                result.installationIssues += ')';
            }
        }

        void WriteScripts(std::ostream& out, RE::VMTypeID type, const void* owner) {
            const auto objects = BoundObjects(type, owner);
            if (objects.empty()) out << "    scripts: none bound (or VM unavailable)\n";
            for (const auto& object : objects) {
                out << "    script=" << object->GetTypeInfo()->GetName() << " state=" << object->currentState.c_str()
                    << " initialized=" << object->IsInitialized() << '\n';
                VisitVariables(object, [&](const char* name, const RE::BSScript::Variable& value, bool available) {
                    out << "      " << name << " = ";
                    if (available) WriteVariable(out, value);
                    else out << "[VM value unavailable]";
                    out << '\n';
                });
            }
        }

        void CaptureFollowerRows(Snapshot& result) {
            for (auto* alias : context.quest->aliases) {
                if (!PartyAlias(alias)) continue;
                auto* ref = static_cast<RE::BGSRefAlias*>(alias)->GetReference();
                auto row = ActorState(ref ? ref->As<RE::Actor>() : nullptr);
                row.aliasID = alias->aliasID;
                row.aliasName = alias->aliasName.c_str();
                if (ref && !row.formID) {
                    row.formID = ref->GetFormID();
                    row.name = ref->GetName();
                    row.dead = true;
                }
                result.followers.push_back(std::move(row));
            }
        }

        void DiagnoseProtectionSettings(Snapshot& result) {
            for (auto& row : result.followers) {
                if (!row.formID || row.dead) continue;
                const bool inService = row.teammate && row.currentFaction;
                if (row.crossfire != (mod::settings::FollowerCrossfire && inService))
                    mod::debug_rules::AddIssue(row.issues, "protection spell differs from crossfire setting/service flags");
                if (mod::settings::FollowerEssential && inService && !row.essential)
                    mod::debug_rules::AddIssue(row.issues, "essential setting not applied");
            }
        }

        void CheckSavedFollowerCount(Snapshot& result) {
            const auto scripts = BoundObjects(static_cast<RE::VMTypeID>(RE::FormType::Quest), context.quest);
            if (scripts.empty()) mod::debug_rules::AddIssue(result.issues, "DialogueFollower has no bound scripts");
            for (const auto& object : scripts) {
                VisitVariables(object, [&](const char* name, const RE::BSScript::Variable& value, bool available) {
                    if (available && std::string_view(name) == "::FollowerCount_var" && value.IsInt() && value.GetSInt() != result.liveCount)
                        mod::debug_rules::AddIssue(result.issues, "Papyrus FollowerCount differs from unique live aliases");
                });
            }
        }

        void CaptureInspectedActor(Snapshot& result, std::uint32_t inspectID) {
            if (!inspectID) return;
            result.inspected = ActorState(RE::TESForm::LookupByID<RE::Actor>(inspectID));
            if (!result.inspected.formID) {
                result.status = "Reference ID did not resolve to an actor.";
                return;
            }
            bool managed = false;
            for (const auto& row : result.followers) {
                if (row.formID == inspectID) managed = true;
            }
            if (!managed && (result.inspected.teammate || result.inspected.currentFaction))
                result.inspected.issues = "service flags present without a party alias; check other follower mods before releasing";
        }

        Snapshot Capture(std::uint32_t inspectID = 0) {
            std::scoped_lock settingsLock(mod::settings::Mutex);
            Snapshot result;
            if (!inspectID) {
                std::scoped_lock lock(snapshotMutex);
                inspectID = snapshot.inspected.formID;
            }
            result.generation = epoch.load();
            result.ready = gameReady;
            if (!result.ready) return result;
            result.cap = context.getCap ? context.getCap() : 0;
            result.executionStatus = mod::controller::Status();
            CheckInstallation(result);
            if (!context.quest) {
                result.status = "DialogueFollower quest is unavailable.";
                return result;
            }
            result.vanillaCount = context.vanillaCount ? context.vanillaCount->value : -1;
            result.modCount = context.modCount ? context.modCount->value : -1;
            result.recruitGate = context.recruitGate ? context.recruitGate->value : -1;
            CaptureFollowerRows(result);
            result.liveCount = mod::debug_rules::Diagnose(result.followers);
            DiagnoseProtectionSettings(result);
            result.issues = mod::debug_rules::GlobalIssues(result.liveCount, result.cap, result.modCount, result.recruitGate);
            result.status = "Snapshot refreshed.";
            CheckSavedFollowerCount(result);
            CaptureInspectedActor(result, inspectID);
            return result;
        }

        void Publish(Snapshot value) {
            std::scoped_lock lock(snapshotMutex);
            if (value.generation != epoch.load()) return;
            if (value.ready && (!snapshot.ready || value.winningPlugin != snapshot.winningPlugin || value.installationIssues != snapshot.installationIssues ||
                value.configuredCap != snapshot.configuredCap || value.slotCapacity != snapshot.slotCapacity || value.boundExtraAliases != snapshot.boundExtraAliases)) {
                const auto message = fmt::format("{} installation: DialogueFollower winner={} configuredCap={} aliasCapacity={} resolvedExtraAliases={}/7 | {}", mod::info::ShortName,
                    value.winningPlugin, value.configuredCap, value.slotCapacity, value.boundExtraAliases,
                    value.installationIssues.empty() ? "required structure and bindings present" : value.installationIssues);
                if (value.installationIssues.empty()) logger::info("{}", message); else logger::warn("{}", message);
                spdlog::default_logger()->flush();
            }
            value.busy = snapshot.busy;
            value.commandState = snapshot.commandState;
            value.commandStatus = snapshot.commandStatus;
            value.status = snapshot.status;
            snapshot = std::move(value);
        }

        void Status(std::string message) {
            std::scoped_lock lock(snapshotMutex);
            snapshot.status = std::move(message);
        }

        bool Related(RE::TESQuest* quest, const std::unordered_set<RE::FormID>& actors) {
            std::string name = quest->GetFormEditorID();
            std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name.find("follower") != std::string::npos || name.find("hireling") != std::string::npos ||
                name.find("skyhaventemple") != std::string::npos || name.starts_with("yliwf")) return true;
            auto* file = quest->GetFile(0);
            if (file && std::string_view(file->fileName) == mod::info::PluginFile) return true;
            for (auto* alias : quest->aliases) {
                if (!alias || alias->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID) continue;
                auto* actor = static_cast<RE::BGSRefAlias*>(alias)->GetActorReference();
                if (actor && actors.contains(actor->GetFormID())) return true;
            }
            return false;
        }

        void WriteQuest(std::ostream& out, RE::TESQuest* quest) {
            out << "\nQUEST " << Form(quest) << " stage=" << quest->GetCurrentStageID() << " running=" << quest->IsRunning()
                << " active=" << quest->IsActive() << " completed=" << quest->IsCompleted() << " flags=" << quest->data.flags.underlying() << '\n';
            if (quest->executedStages) for (const auto& stage : *quest->executedStages)
                out << "  executedStage=" << stage.data.index << " flags=" << static_cast<int>(stage.data.flags.underlying()) << '\n';
            for (auto* objective : quest->objectives) if (objective)
                out << "  objective=" << objective->index << " state=" << static_cast<int>(objective->state.underlying())
                    << " text=" << std::quoted(std::string(objective->displayText.c_str())) << '\n';
            WriteScripts(out, static_cast<RE::VMTypeID>(RE::FormType::Quest), quest);
            for (auto* alias : quest->aliases) {
                if (!alias) continue;
                out << "  alias=" << alias->aliasID << " name=" << alias->aliasName.c_str() << " type=" << alias->GetTypeString().c_str()
                    << " flags=" << alias->flags.underlying() << " fill=" << alias->fillType.underlying() << '\n';
                if (alias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID) {
                    auto* ref = static_cast<RE::BGSRefAlias*>(alias)->GetReference();
                    out << "    reference=" << Form(ref) << '\n';
                    if (ref) { WriteActor(out, ref->As<RE::Actor>()); WriteScripts(out, static_cast<RE::VMTypeID>(ref->GetFormType()), ref); }
                }
                WriteScripts(out, alias->GetVMTypeID(), alias);
            }
        }

        void Dump() {
            std::scoped_lock settingsLock(mod::settings::Mutex);
            if (!gameReady || !context.quest) { Status("Load a game before dumping context."); return; }
            if (logDirectory.empty()) { Status("SKSE log directory unavailable; dump could not be written."); return; }
            const auto state = Capture();
            std::ostringstream out;
            out << std::boolalpha << mod::info::ShortName << " context dump v1\n";
            out << "Runtime=" << REL::Module::get().version().string() << " generation=" << state.generation << '\n';
            out << "Controller: " << state.executionStatus << '\n';
            {
                std::scoped_lock lock(mod::settings::Mutex);
                const auto& native = mod::controller::storage::Get();
                out << "Native checkpoint: ticket=" << native.ticket << " failure=" << std::quoted(native.failure) << '\n';
                if (native.active) {
                    const auto& active = *native.active;
                    out << "  active request=" << active.request << " phase=" << static_cast<int>(active.phase)
                        << " next=" << active.next << " delay=" << active.delay << '\n';
                    for (const auto& step : active.steps) out << "  step effect=" << static_cast<int>(step.effect)
                        << " alias=" << step.alias << " actorHandle=" << step.actor << " otherActorHandle=" << step.otherActor << " argument=" << step.argument << '\n';
                }
                for (const auto& command : native.queue) out << "  queued request=" << command.id << " operation=" << static_cast<int>(command.operation)
                    << " actorHandle=" << command.actor << " alias=" << command.selected << '\n';
                for (const auto& receipt : native.receipts) out << "  receipt request=" << receipt.request << " success=" << receipt.success
                    << " ticket=" << receipt.ticket << " cursor=" << receipt.cursor << '\n';
                out << "  deadlines:"; for (auto deadline : native.deadlines) out << ' ' << deadline; out << '\n';
                for (const auto& caller : native.callers) out << "  caller request=" << caller.request << " stack=" << caller.stack
                    << " self=" << caller.self << " result=" << caller.result << " function=" << caller.type << '.' << caller.function << '\n';
            }
            out << "Selection: mod quests; follower/hireling/SkyHavenTemple quests; quests containing current party actors; linked quest properties.\n"
                << "VM values are read individually: this is a diagnostic snapshot, not an atomic VM/save-game snapshot.\n"
                << "Referenced objects are summarized; arrays include every element.\n";
            out << "Settings: logging=" << Logging.load() << " mode=" << mod::settings::FollowerPerkOption << " max=" << mod::settings::MaxExtraFollowers + 1
                << " speechLevels=" << mod::settings::SpeechLevelsPerSlot << " perks=" << std::quoted(mod::settings::PerkListBuffer)
                << " essential=" << mod::settings::FollowerEssential << " friendlyFire=" << mod::settings::FriendlyFire
                << " crossfire=" << mod::settings::FollowerCrossfire << " sandbox=" << mod::settings::FollowerSandbox << " homes=" << mod::settings::FollowerHomes << '\n';
            out << "Party: uniqueLive=" << state.liveCount << " cap=" << state.cap << " vanillaCount=" << state.vanillaCount
                << " modCount=" << state.modCount << " recruitGate=" << state.recruitGate << " issues=" << state.issues << '\n';
            for (const auto& row : state.followers) out << "  slot=" << row.aliasID << " ref=" << std::hex << row.formID << std::dec << " issues=" << row.issues << '\n';
            WriteActor(out, RE::PlayerCharacter::GetSingleton());
            if (state.inspected.formID) {
                out << "\nINSPECTED ACTOR\n";
                WriteActor(out, RE::TESForm::LookupByID<RE::Actor>(state.inspected.formID));
            }
            if (context.originalFlags) for (const auto& [id, flags] : context.originalFlags())
                out << "originalBaseFlags: " << Form(RE::TESForm::LookupByID(id)) << " essential=" << bool(flags & 1) << " protected=" << bool(flags & 2) << '\n';
            std::unordered_set<RE::FormID> actors, selected;
            std::vector<RE::TESQuest*> quests;
            for (const auto& row : state.followers) if (row.formID) actors.insert(row.formID);
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!data) { Status("Data handler unavailable."); return; }
            out << "\nLOAD ORDER\n";
            for (auto* file : data->files) if (file && file->compileIndex != 0xFF)
                out << "  " << file->GetFilename() << " index=" << static_cast<int>(file->compileIndex)
                    << " lightIndex=" << file->smallFileCompileIndex << '\n';
            for (auto* quest : data->GetFormArray<RE::TESQuest>()) if (quest && Related(quest, actors)) {
                selected.insert(quest->GetFormID()); quests.push_back(quest);
            }
            // Follow quest references on selected scripts, including hireling rehire state.
            // The set prevents cycles; no recursive traversal of arbitrary world objects.
            for (std::size_t i = 0; i < quests.size(); ++i) {
                for (const auto& object : BoundObjects(static_cast<RE::VMTypeID>(RE::FormType::Quest), quests[i]))
                    VisitVariables(object, [&](const char*, const RE::BSScript::Variable& value, bool available) {
                        if (!available || !value.IsObject()) return;
                        auto linked = value.GetObject();
                        auto* quest = linked ? static_cast<RE::TESQuest*>(linked->Resolve(static_cast<RE::VMTypeID>(RE::FormType::Quest))) : nullptr;
                        if (quest && selected.insert(quest->GetFormID()).second) quests.push_back(quest);
                    });
            }
            for (auto* quest : quests) WriteQuest(out, quest);
            for (auto* global : data->GetFormArray<RE::TESGlobal>()) {
                if (!global) continue;
                std::string name = global->GetFormEditorID();
                std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                auto* file = global->GetFile(0);
                if (name.find("follower") != std::string::npos || name.find("hireling") != std::string::npos || name == "playeranimalcount" ||
                    (file && std::string_view(file->fileName) == mod::info::PluginFile)) out << "GLOBAL " << Form(global) << " value=" << global->value << '\n';
            }
            const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const auto path = logDirectory / fmt::format("{}-context-{}-{}.txt", mod::info::ShortName, millis, ++sequence);
            std::ofstream file(path);
            if (!file) { Status("Could not open context dump: " + path.string()); return; }
            file << out.str();
            file.close();
            if (!file) { Status("Context dump write failed: " + path.string()); return; }
            logger::info("Context dumped to {} ({} quests)", path.string(), quests.size());
            spdlog::default_logger()->flush();
            Status("Context dumped to " + path.string());
            NotifyStateChanged();
        }

        template <class Detail>
        void TraceDetail(std::string_view event, RE::TESForm* subject, Detail makeDetail) {
            if (!Logging.load()) return;
            // Tasklet-safe: read only stable form IDs, use an atomic sequence and the
            // thread-safe sink. Never query names, aliases, packages or actor state here.
            // The VM owns the arguments for the duration of this synchronous call.
            logger::info("FLOW #{} {} actor={:08X} {}", ++sequence, event,
                subject ? subject->GetFormID() : 0, makeDetail());
        }

        bool PapyrusLogging(RE::StaticFunctionTag*) { return Logging.load(); }
        void PapyrusDebug(RE::StaticFunctionTag*, RE::BSFixedString event, RE::TESForm* subject, RE::BSFixedString detail) {
            TraceDetail(event.c_str(), subject, [&] { return std::string_view(detail.c_str()); });
        }
        void PapyrusDebugInt(RE::StaticFunctionTag*, RE::BSFixedString event, RE::TESForm* subject,
            RE::BSFixedString name, std::int32_t value, RE::BSFixedString secondName, std::int32_t secondValue) {
            TraceDetail(event.c_str(), subject, [&] {
                return secondName.empty() ? fmt::format("{}={}", name.c_str(), value) :
                    fmt::format("{}={} {}={}", name.c_str(), value, secondName.c_str(), secondValue);
            });
        }
        void PapyrusDebugBool(RE::StaticFunctionTag*, RE::BSFixedString event, RE::TESForm* subject,
            RE::BSFixedString name, bool value) {
            TraceDetail(event.c_str(), subject, [&] { return fmt::format("{}={}", name.c_str(), value); });
        }
        void PapyrusDebugForm(RE::StaticFunctionTag*, RE::BSFixedString event, RE::TESForm* subject,
            RE::BSFixedString name, RE::TESForm* value) {
            TraceDetail(event.c_str(), subject, [&] {
                return value ? fmt::format("{}={:08X}", name.c_str(), value->GetFormID()) : fmt::format("{}=None", name.c_str());
            });
        }
    }

    void InitializeLog() {
        if (auto directory = SKSE::log::log_directory()) {
            logDirectory = *directory;
            try {
                auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>((logDirectory / fmt::format("{}.log", mod::info::BinaryName)).string(), 10 * 1024 * 1024, 5);
                auto log = std::make_shared<spdlog::logger>(mod::info::ShortName, std::move(sink));
                log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v");
                log->set_level(spdlog::level::info);
                log->flush_on(spdlog::level::warn);
                spdlog::set_default_logger(std::move(log));
                fileLogAvailable = true;
            } catch (const spdlog::spdlog_ex& error) {
                logger::error("Could not initialize {} file logging: {}", mod::info::ShortName, error.what());
            }
        }
        logger::info("{} loaded; flow logging is opt-in.", mod::info::DisplayName);
        // Capture VM diagnostics even when Papyrus.0.log is unavailable or flow
        // logging is disabled. Filtering keeps unrelated mods out of our log.
        SKSE::log::add_papyrus_sink(std::regex(
            fmt::format("DialogueFollowerScript|FollowerAliasScript|{}_", mod::info::ScriptPrefix),
            std::regex::icase));
        logger::info("Papyrus diagnostics for follower scripts are forwarded to {}.log.", mod::info::BinaryName);
    }

    void Configure(Context value) { context = std::move(value); }
    void SetLogging(bool enabled) {
        if (enabled && !fileLogAvailable) {
            Status("Flow log could not be opened. Check the SKSE log directory and its write permissions.");
            enabled = false;
        }
        Logging = enabled;
        logger::info("Flow logging {}", enabled ? "enabled" : "disabled");
        spdlog::default_logger()->flush();
    }
    void Trace(std::string_view event, RE::Actor* actor, std::string_view detail) {
        TraceDetail(event, actor, [detail] { return detail; });
    }
    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm) {
        // Logging returns on the tasklet when disabled and formats only after the
        // flag check.
        vm->RegisterFunction("IsDebugLoggingEnabled", mod::info::NativeScript, PapyrusLogging, true);
        vm->RegisterFunction("Debug", mod::info::NativeScript, PapyrusDebug, true);
        vm->RegisterFunction("DebugInt", mod::info::NativeScript, PapyrusDebugInt, true);
        vm->RegisterFunction("DebugBool", mod::info::NativeScript, PapyrusDebugBool, true);
        vm->RegisterFunction("DebugForm", mod::info::NativeScript, PapyrusDebugForm, true);
    }
    void Invalidate() {
        gameReady = false;
        activeTicket = 0;
        ++epoch;
        {
            std::scoped_lock lock(refreshMutex);
            refresh.Reset();
        }
        std::scoped_lock lock(snapshotMutex);
        snapshot = {};
        snapshot.generation = epoch.load();
    }
    void GameLoaded() { gameReady = true; NotifyStateChanged(); }
    std::uint64_t Generation() { return epoch.load(); }
    bool IsCurrentGame(std::uint64_t generation) { return mod::debug_rules::CurrentGame(generation, epoch.load(), gameReady); }
    bool IsCommandCurrent(std::int32_t ticket) { return mod::debug_rules::CurrentCommand(ticket, activeTicket.load(), gameReady); }
    void CompleteCommand(std::int32_t ticket, bool success, std::string detail) {
        const auto generation = epoch.load();
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([generation, ticket, success, detail = std::move(detail)] {
            if (!IsCurrentGame(generation) || !IsCommandCurrent(ticket)) return;
            activeTicket = 0;
            {
                std::scoped_lock lock(snapshotMutex);
                snapshot.busy = false;
                snapshot.commandState = success ? CommandState::Succeeded : CommandState::Failed;
                snapshot.status = snapshot.commandStatus = success ?
                    (detail.empty() ? "Follower command completed and verified." : detail) : "Follower command failed: " + detail;
            }
            logger::info("Debug command receipt: ticket={} verified={} {}", ticket, success, detail);
            NotifyStateChanged();
        });
    }
    Snapshot GetSnapshot() {
        std::scoped_lock lock(snapshotMutex);
        return snapshot;
    }
    void UpdateCommandProgress(std::int32_t ticket, std::string detail) {
        const auto generation = epoch.load();
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([generation, ticket, detail = std::move(detail)] {
            if (!IsCurrentGame(generation) || !IsCommandCurrent(ticket)) return;
            std::scoped_lock lock(snapshotMutex);
            snapshot.status = snapshot.commandStatus = detail + " Close the menu to let the game run.";
        });
    }
    void SetRefreshVisible(bool visible) {
        std::scoped_lock lock(refreshMutex);
        refresh.SetVisible(visible);
    }
    void NotifyStateChanged() {
        std::scoped_lock lock(refreshMutex);
        refresh.MarkDirty();
    }
    void RefreshVisiblePage() {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !gameReady || !context.isRefreshVisible || !context.isRefreshVisible()) return;
        const auto generation = epoch.load();
        std::uint64_t ticket;
        {
            std::scoped_lock lock(refreshMutex);
            ticket = refresh.Queue(mod::refresh::State::Clock::now());
        }
        if (!ticket) return;
        tasks->AddTask([generation, ticket]() {
            if (!IsCurrentGame(generation)) return;
            if (!context.isRefreshVisible || !context.isRefreshVisible()) {
                SetRefreshVisible(false);
                return;
            }
            std::optional<mod::refresh::State::Request> request;
            {
                std::scoped_lock lock(refreshMutex);
                request = refresh.Start(ticket);
            }
            if (!request) return;
            auto value = Capture(request->inspectID);
            {
                std::scoped_lock lock(snapshotMutex);
                if ((request->inspectID && !snapshot.busy) || !snapshot.ready) snapshot.status = value.status;
            }
            Publish(std::move(value));
            {
                std::scoped_lock lock(refreshMutex);
                refresh.Finish(ticket, request->revision);
            }
        });
    }
    void RequestRefresh(std::uint32_t inspectID) {
        {
            std::scoped_lock lock(refreshMutex);
            if (inspectID) refresh.Inspect(inspectID);
            else refresh.MarkDirty();
        }
        RefreshVisiblePage();
    }
    void RequestDump() {
        const auto generation = epoch.load();
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([generation]() {
            if (generation != epoch.load()) return;
            try { Dump(); } catch (const std::exception& error) { Status("Dump failed: " + std::string(error.what())); logger::error("Context dump failed: {}", error.what()); }
        });
    }
    void RequestPartyAction(Action action, std::uint64_t generation) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !IsCurrentGame(generation) || !mod::debug_rules::IsPartyAction(static_cast<std::int32_t>(action))) return;
        const auto label = PartyActionName(action);
        {
            std::scoped_lock lock(snapshotMutex);
            if (snapshot.generation != generation || snapshot.busy ||
                !mod::debug_rules::CanCommand(snapshot.structureReady, snapshot.scriptReady, snapshot.bindingsReady,
                    static_cast<std::int32_t>(action), OptionsEnabled.load())) return;
            snapshot.busy = true;
            snapshot.commandState = CommandState::Pending;
            snapshot.status = snapshot.commandStatus = fmt::format("{} queued. Close the menu to let the game run.", label);
        }
        tasks->AddTask([action, generation] {
            if (!IsCurrentGame(generation)) return;
            const auto ticket = ticketSequence.fetch_add(1) + 1;
            activeTicket = ticket;
            const auto submission = mod::controller::EnqueueParty(static_cast<std::int32_t>(action), ticket);
            if (!submission.targets) CompleteCommand(ticket, false, submission.failure);
            else UpdateCommandProgress(ticket, fmt::format("{}: {} followers queued.", PartyActionName(action), submission.targets));
        });
    }
    void RequestAction(Action action, std::uint32_t actorID, std::int32_t aliasID, std::uint64_t generation) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) { Status("SKSE task interface unavailable."); return; }
        std::string label = ActionName(action);
        {
            std::scoped_lock lock(snapshotMutex);
            if (snapshot.busy || !snapshot.ready || generation != epoch.load()) return;
            if (!mod::debug_rules::CanCommand(snapshot.structureReady, snapshot.scriptReady, snapshot.bindingsReady,
                static_cast<std::int32_t>(action), OptionsEnabled.load())) {
                snapshot.commandState = CommandState::Failed;
                snapshot.status = snapshot.commandStatus = "Command unavailable. Check installation diagnostics or enable debug options for repair and recruitment commands.";
                return;
            }
            snapshot.busy = true;
            snapshot.commandState = CommandState::Pending;
            if (actorID) {
                const auto found = std::find_if(snapshot.followers.begin(), snapshot.followers.end(), [actorID](const auto& row) { return row.formID == actorID; });
                const auto* row = found != snapshot.followers.end() ? &*found : snapshot.inspected.formID == actorID ? &snapshot.inspected : nullptr;
                label = fmt::format("{}: {}", label, row && !row->name.empty() ? row->name : "Unnamed follower");
            }
            snapshot.status = snapshot.commandStatus = fmt::format("{} queued. Close the menu to let the game run.", label);
        }
        tasks->AddTask([action, actorID, aliasID, generation, label = std::move(label)]() {
            if (!IsCurrentGame(generation)) return;
            auto* actor = actorID ? RE::TESForm::LookupByID<RE::Actor>(actorID) : nullptr;
            auto ticket = ticketSequence.fetch_add(1) + 1;
            activeTicket = ticket;
            auto command = static_cast<std::int32_t>(action);
            auto expected = aliasID;
            logger::info("Debug command requested: action={} actor={:08X} expectedAlias={}", command, actorID, expected);
            if ((actorID && !actor) || !mod::controller::SubmitDebug(command, actor, expected, ticket)) {
                activeTicket = 0;
                std::scoped_lock lock(snapshotMutex);
                snapshot.busy = false;
                snapshot.commandState = CommandState::Failed;
                snapshot.status = snapshot.commandStatus = "Could not run the command. Install matching DLL and scripts, then load a game.";
                logger::warn("Debug command dispatch failed");
            }
        });
    }
}
