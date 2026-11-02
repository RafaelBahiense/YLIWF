#include "Dodge.h"
#include "DodgeRules.h"
#include "BashRules.h"
#include "AttackRules.h"
#include "NotificationRules.h"
#include "../interference/GraphNotifications.h"
#include "../interference/ExternalAttacks.h"
#include "RE/Skyrim.h"
#include <Windows.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <sstream>

namespace {
    namespace telemetry = mod::combat_tests::telemetry;
    constexpr auto capacity = telemetry::notification_rules::Capacity;
    constexpr std::array names{"bIsDodging", "bUND_IsDodgeRoll", "MCO_IsInRecovery", "MSL_IsStaggerRecovery"};

    struct Context {
        std::chrono::steady_clock::time_point sampledAt = std::chrono::steady_clock::now();
        bool valid = false;
        float elapsed = 0, stamina = 0;
        std::uint32_t requests = 0, observed = 0, timeouts = 0, blocking = 0, readable = 0, flags = 0;
        std::uint32_t guardPhase = 0, releaseRequests = 0, releaseAccepted = 0, releaseObserved = 0, dispatch = 2,
                      validation = 0, processFlags = 0, actionRequests = 0, attackRequests = 0, rejectedAttacks = 0,
                      wantsBlock = 0, attacking = 0;
        std::int32_t processResult = -1, processReturned = -1;
        char status[192]{};
        std::uint32_t hitReadable = 0, hit = 0, graphWantReadable = 0;
        std::int32_t graphWant = -1;
        float releaseWait = 0;
        char phase[16] = "preparing";
    } context;

    struct Entry {
        Context sample;
        float millisecondsAfterSample = 0;
        char tag[64]{};
        bool callback = true;
        bool notification = false, suppressed = false;
        std::int32_t notifyResult = -1;
        std::uint32_t thread = 0;
        std::uint64_t ticket = 0;
        std::uintptr_t callerOffset = 0;
        char module[96]{};
    };

    std::array<Entry, capacity> entries;
    std::size_t written = 0;
    std::uint64_t notificationSequence = 0, generationStart = 0;
    telemetry::DodgeSnapshot snapshot;
    std::mutex mutex;
    std::atomic_uint32_t selected{0};
    RE::BSAnimationGraphManagerPtr bound;

    struct BashScope {
        std::uint32_t actor = 0, token = 0;
        decltype(yliwf::testing::Host::bashRequest) request = nullptr;
        decltype(yliwf::testing::Host::bashAnimation) event = nullptr;
    } bashScope;

    struct AttackScope {
        std::uint32_t actor = 0, token = 0;
        decltype(yliwf::testing::Host::attackRequest) request = nullptr;
        decltype(yliwf::testing::Host::attackAnimation) event = nullptr;
    } attackScope;

    Entry& Record(std::string_view tag, bool callback) {
        auto& entry = entries[written++ % capacity];
        entry = {};
        entry.sample = context;
        entry.millisecondsAfterSample =
            std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - context.sampledAt).count();
        entry.callback = callback;
        entry.thread = GetCurrentThreadId();
        std::snprintf(entry.tag, sizeof(entry.tag), "%.*s", static_cast<int>(tag.size()), tag.data());
        snapshot.overwritten = static_cast<std::uint32_t>(written > capacity ? written - capacity : 0);
        if (callback) {
            ++snapshot.events;
            snapshot.dodgeEvents += telemetry::rules::DodgeEvent(tag);
            snapshot.guardEvents += telemetry::rules::GuardEvent(tag);
            std::snprintf(snapshot.lastEvent, sizeof(snapshot.lastEvent), "%s", entry.tag);
        }
        return entry;
    }

    class GraphSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
            const auto actor = selected.load();
            if (!actor || !event || !event->holder || event->holder->GetFormID() != actor)
                return RE::BSEventNotifyControl::kContinue;
            const std::string_view tag = event->tag.c_str() ? event->tag.c_str() : "";
            if (const auto kind = telemetry::bash_rules::Classify(tag)) {
                BashScope scope;
                {
                    std::scoped_lock lock(mutex);
                    scope = bashScope;
                }
                // Actual graph callbacks only. Release our lock before calling the
                // engine-free host mailbox, and preserve the captured request key.
                if (scope.actor == actor && scope.request && scope.event) {
                    const auto request = scope.request(actor, scope.token);
                    if (request)
                        scope.event(actor, scope.token, request, *kind);
                }
            }
            if (const auto kind = telemetry::attack_rules::Classify(tag)) {
                AttackScope scope;
                {
                    std::scoped_lock lock(mutex);
                    scope = attackScope;
                }
                if (scope.actor == actor && scope.request && scope.event) {
                    const auto request = scope.request(actor, scope.token);
                    if (request)
                        scope.event(actor, scope.token, request, *kind);
                }
            }
            mod::combat_tests::interference::external_attacks::Event(actor, tag);
            if (telemetry::rules::Relevant(tag)) {
                std::scoped_lock lock(mutex);
                if (selected.load() == actor)
                    Record(tag, true);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    } sink;

    void Unbind() {
        selected = 0;
        if (bound) {
            for (const auto& graph : bound->graphs)
                if (graph)
                    graph->GetEventSource<RE::BSAnimationGraphEvent>()->RemoveEventSink(&sink);
            bound.reset();
        }
    }
}

void mod::combat_tests::telemetry::Attach(RE::Actor* actor) {
    if (!actor || !actor->Is3DLoaded())
        return;
    RE::BSAnimationGraphManagerPtr manager;
    if (!actor->GetAnimationGraphManager(manager) || !manager)
        return;
    if (bound == manager && selected.load() == actor->GetFormID()) {
        std::scoped_lock lock(mutex);
        snapshot.requestObserver = interference::graph_notifications::Verified(actor);
        return;
    }
    Unbind();
    bound = manager;
    selected = actor->GetFormID();
    bool attached = false;
    for (const auto& graph : bound->graphs) {
        if (!graph)
            continue;
        graph->GetEventSource<RE::BSAnimationGraphEvent>()->AddEventSink(&sink);
        attached = true;
    }
    std::scoped_lock lock(mutex);
    snapshot.attached = attached ? 1u : 0u;
    snapshot.requestObserver = interference::graph_notifications::Verified(actor);
    Record("graphBound", false);
}

void mod::combat_tests::telemetry::Sample(RE::Actor* actor, float elapsed, const char* phase,
                                          const yliwf::testing::DefenderObservation& action) {
    Attach(actor);
    Context next;
    next.valid = true;
    next.elapsed = elapsed;
    next.requests = action.guardRequests;
    next.observed = action.guardObserved;
    next.timeouts = action.actionTimeouts;
    next.guardPhase = action.guardPhase;
    next.releaseRequests = action.releaseRequests;
    next.releaseAccepted = action.releaseAccepted;
    next.releaseObserved = action.releaseObserved;
    next.dispatch = action.dispatch;
    next.validation = action.validationMask;
    next.processFlags = action.processFlags;
    next.processResult = action.processResult;
    next.processReturned = action.processReturned;
    next.actionRequests = action.actionRequests;
    next.attackRequests = action.attackRequests;
    next.rejectedAttacks = action.attacksRejected;
    next.hitReadable = action.blockHitReadable;
    next.hit = action.blockHit;
    next.graphWantReadable = action.graphWantReadable;
    next.graphWant = action.graphWantBlock;
    next.releaseWait = action.releaseWait;
    next.wantsBlock = actor->AsActorState()->actorState2.wantBlocking;
    next.attacking = actor->IsAttacking();
    std::snprintf(next.status, sizeof(next.status), "%s", action.actionStatus);
    next.stamina = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
    next.blocking = actor->IsBlocking();
    std::snprintf(next.phase, sizeof(next.phase), "%s", phase);
    for (std::uint32_t i = 0; i < names.size(); ++i) {
        bool value = false;
        if (actor->GetGraphVariableBool(names[i], value)) {
            next.readable |= 1u << i;
            if (value)
                next.flags |= 1u << i;
        }
    }
    std::scoped_lock lock(mutex);
    const auto previous = context;
    context = next;
    if (std::string_view(phase) == "running") {
        ++snapshot.samples;
        snapshot.readableSamples += (next.readable & 1) != 0;
    }
    if (std::string_view(phase) == "running" && std::string_view(previous.phase) == "running" &&
        rules::Active(next.readable, next.flags))
        snapshot.dodgingSeconds += (std::max)(0.0f, next.elapsed - previous.elapsed);
    snapshot.readable = next.readable;
    snapshot.flags = next.flags;
    if (next.requests != previous.requests)
        Record("guardRequestsChanged", false);
    if (next.observed != previous.observed)
        Record("guardObservedChanged", false);
    if (next.timeouts != previous.timeouts)
        Record("guardTimeoutsChanged", false);
    if (next.guardPhase != previous.guardPhase)
        Record("guardPhaseChanged", false);
    if (next.releaseRequests != previous.releaseRequests)
        Record("releaseRequestsChanged", false);
    if (std::string_view(next.status) != previous.status)
        Record("guardStatusChanged", false);
    if (next.wantsBlock != previous.wantsBlock || next.blocking != previous.blocking)
        Record("blockStateChanged", false);
    if (next.hitReadable != previous.hitReadable || next.hit != previous.hit ||
        next.graphWantReadable != previous.graphWantReadable || next.graphWant != previous.graphWant)
        Record("blockReactionStateChanged", false);
    if (next.attackRequests != previous.attackRequests || next.rejectedAttacks != previous.rejectedAttacks)
        Record("attackRequestsChanged", false);
    if (next.readable != previous.readable || next.flags != previous.flags)
        Record("graphFlagsChanged", false);
}

void mod::combat_tests::telemetry::End() {
    Unbind();
    std::scoped_lock lock(mutex);
    snapshot.attached = 0;
    snapshot.requestObserver = 0;
    bashScope = {};
    attackScope = {};
}

void mod::combat_tests::telemetry::ObserveBashes(std::uint32_t actor, std::uint32_t token,
                                                 const yliwf::testing::Host& host) {
    std::scoped_lock lock(mutex);
    bashScope = {actor, token, host.bashRequest, host.bashAnimation};
}

void mod::combat_tests::telemetry::ObserveAttacks(std::uint32_t actor, std::uint32_t token,
                                                  const yliwf::testing::Host& host) {
    std::scoped_lock lock(mutex);
    attackScope = {actor, token, host.attackRequest, host.attackAnimation};
}

void mod::combat_tests::telemetry::Reset() {
    End();
    std::scoped_lock lock(mutex);
    snapshot = {};
    context = {};
    written = 0;
    generationStart = notificationSequence;
}

mod::combat_tests::telemetry::DodgeSnapshot mod::combat_tests::telemetry::Read() {
    std::scoped_lock lock(mutex);
    return snapshot;
}

std::string mod::combat_tests::telemetry::Report(bool trace) {
    std::scoped_lock lock(mutex);
    std::ostringstream text;
    if (!trace) {
        text << "\nDodge telemetry: relevant graph callbacks " << snapshot.events << "; dodge-tagged "
             << snapshot.dodgeEvents << "; guard-tagged " << snapshot.guardEvents << "; sampled bIsDodging seconds "
             << snapshot.dodgingSeconds << "; bIsDodging readable samples " << snapshot.readableSamples << '/'
             << snapshot.samples << "; overwritten trace entries " << snapshot.overwritten;
        text << "\nAnimation notifications: " << snapshot.notifications << "; completed "
             << snapshot.notificationCompleted << "; returned false " << snapshot.notificationFalse << "; suppressed "
             << snapshot.notificationSuppressed;
        return text.str();
    }
    text << "\nGraph bit order: bIsDodging=1, bUND_IsDodgeRoll (style)=2, MCO_IsInRecovery=4, MSL_IsStaggerRecovery=8."
         << "\nTrace context is the latest sample, not exact callback-time state. Callbacks do not prove a completed "
            "dodge."
         << "\nMsAfterSample is wall-clock delay, including pauses; ContextValid=0 means no sample was available."
         << "\nNotification CallerModule/CallerOffset identify the immediate caller, not the original producer through "
            "wrappers."
         << " NotifyResult is the next handler's boolean result; -1 is incomplete or not a notification."
         << "\nElapsed,MsAfterSample,ContextValid,Phase,Kind,Event,SampledStamina,GuardRequests,GuardObserved,Timeouts,"
            "Blocking,Readable,Flags,Thread,CallerModule,CallerOffset,NotifyResult,Suppressed,GuardPhase,"
            "ReleaseRequests,ReleaseAccepted,ReleaseObserved,Dispatch,ProcessResult,ProcessReturned,ProcessFlags,"
            "ValidationMask,ActionRequests,AttackRequests,AttacksRejected,WantsBlock,Attacking,BlockHitReadable,"
            "BlockHit,"
            "GraphWantReadable,GraphWantBlock,ReleaseWait,ActionStatus\n";
    for (auto i = written > capacity ? written - capacity : 0; i < written; ++i) {
        const auto& row = entries[i % capacity];
        const auto& sample = row.sample;
        text << sample.elapsed << ',' << row.millisecondsAfterSample << ',' << sample.valid << ',' << sample.phase
             << ','
             << (row.notification ? "notify"
                 : row.callback   ? "graph"
                                  : "sample")
             << ',' << row.tag << ',' << sample.stamina << ',' << sample.requests << ',' << sample.observed << ','
             << sample.timeouts << ',' << sample.blocking << ',' << sample.readable << ',' << sample.flags << ','
             << row.thread << ',' << row.module << ',' << std::hex << row.callerOffset << std::dec << ','
             << row.notifyResult << ',' << row.suppressed << ',' << sample.guardPhase << ',' << sample.releaseRequests
             << ',' << sample.releaseAccepted << ',' << sample.releaseObserved << ',' << sample.dispatch << ','
             << sample.processResult << ',' << sample.processReturned << ',' << sample.processFlags << ','
             << sample.validation << ',' << sample.actionRequests << ',' << sample.attackRequests << ','
             << sample.rejectedAttacks << ',' << sample.wantsBlock << ',' << sample.attacking << ','
             << sample.hitReadable << ',' << sample.hit << ',' << sample.graphWantReadable << ',' << sample.graphWant
             << ',' << sample.releaseWait << ",\"";
        for (const char ch : std::string_view(sample.status)) {
            if (ch == '"')
                text << '"';
            text << ch;
        }
        text << "\"\n";
    }
    return text.str();
}

bool mod::combat_tests::telemetry::Watching(std::uint32_t actor) {
    return actor != 0 && selected.load() == actor;
}

std::uint64_t mod::combat_tests::telemetry::BeginNotification(std::uint32_t actor, std::string_view event,
                                                              std::string_view module, std::uintptr_t offset) {
    if (!Watching(actor) || !rules::Relevant(event))
        return 0;
    std::scoped_lock lock(mutex);
    if (!Watching(actor))
        return 0;
    auto& entry = Record(event, false);
    entry.notification = true;
    entry.callerOffset = offset;
    std::snprintf(entry.module, sizeof(entry.module), "%.*s", static_cast<int>(module.size()), module.data());
    entry.ticket = notification_rules::Ticket(++notificationSequence, written - 1);
    ++snapshot.notifications;
    return entry.ticket;
}

void mod::combat_tests::telemetry::EndNotification(std::uint64_t ticket, bool result, bool suppressed) {
    std::scoped_lock lock(mutex);
    if (!notification_rules::Current(ticket, generationStart))
        return;
    ++snapshot.notificationCompleted;
    snapshot.notificationFalse += !result;
    snapshot.notificationSuppressed += suppressed;
    auto& entry = entries[notification_rules::Slot(ticket)];
    if (entry.ticket != ticket)
        return;
    entry.notifyResult = result ? 1 : 0;
    entry.suppressed = suppressed;
}
