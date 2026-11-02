#include "ExternalAttacks.h"
#include "ExternalAttackRules.h"
#include "RE/Skyrim.h"
#include <mutex>
#include <sstream>

namespace {
    namespace bridge = mod::combat_tests::interference::external_attacks;
    using Commitment = yliwf::testing::AttackCommitment;
    std::mutex mutex;
    std::uint32_t actorID = 0, token = 0, sequence = 0, active = 0;
    bridge::Reserve reserve = nullptr;
    bridge::Snapshot snapshot;
    mod::combat_tests::interference::external_attack_rules::State state;
    auto source = mod::combat_tests::interference::external_attack_rules::Source::None;
    bool handoffPublished = false;

    // Caller holds mutex; the host callback is engine-free.
    void PublishHandoff() {
        if (active && state.handingOff && !handoffPublished && reserve &&
            reserve(actorID, token, active, Commitment::Finishing)) {
            handoffPublished = true;
            ++snapshot.handoffs;
        }
    }
}

void mod::combat_tests::interference::external_attacks::Begin(std::uint32_t actor, std::uint32_t generation,
                                                              Reserve callback) {
    End();
    std::scoped_lock lock(mutex);
    actorID = actor;
    token = generation;
    reserve = callback;
    snapshot = {};
    state = {};
    handoffPublished = false;
    source = external_attack_rules::Source::None;
}

void mod::combat_tests::interference::external_attacks::End() {
    std::scoped_lock lock(mutex);
    if (active) {
        ++snapshot.endedActive;
        if (reserve)
            reserve(actorID, token, active, Commitment::Clear);
    }
    active = actorID = token = 0;
    reserve = nullptr;
}

bool mod::combat_tests::interference::external_attacks::Watching(std::uint32_t actor) {
    std::scoped_lock lock(mutex);
    return actor && actor == actorID && reserve;
}

std::uint32_t mod::combat_tests::interference::external_attacks::Before(std::uint32_t actor, std::string_view module,
                                                                        std::string_view event) {
    const auto requestedSource = external_attack_rules::Classify(module, event);
    if (requestedSource == external_attack_rules::Source::None)
        return 0;
    std::scoped_lock lock(mutex);
    if (!actor || actor != actorID || !reserve || sequence == UINT32_MAX)
        return 0;
    const auto request = ++sequence;
    if (!reserve(actorID, token, request, Commitment::Active))
        return 0;
    if (active)
        ++snapshot.superseded;
    active = request;
    source = requestedSource;
    state = {};
    handoffPublished = false;
    ++snapshot.proposals;
    if (source == external_attack_rules::Source::PermaBlock)
        ++snapshot.permaProposals;
    return request;
}

void mod::combat_tests::interference::external_attacks::After(std::uint32_t request, bool accepted) {
    std::scoped_lock lock(mutex);
    if (!request || request != active)
        return;
    state.returned = true;
    state.accepted = accepted;
    if (accepted)
        ++snapshot.accepted;
    else
        ++snapshot.rejected;
    if (source == external_attack_rules::Source::PermaBlock) {
        if (accepted)
            ++snapshot.permaAccepted;
        else
            ++snapshot.permaRejected;
    }
}

void mod::combat_tests::interference::external_attacks::Event(std::uint32_t actor, std::string_view event) {
    std::scoped_lock lock(mutex);
    if (actor == actorID && active) {
        state.Event(event);
        PublishHandoff();
    }
}

void mod::combat_tests::interference::external_attacks::Sample(RE::Actor* actor) {
    if (!actor)
        return;
    const bool attacking =
        actor->IsAttacking() || actor->AsActorState()->GetAttackState() != RE::ATTACK_STATE_ENUM::kNone;
    bool recovering = false;
    const bool readable = actor->GetGraphVariableBool("MCO_IsInRecovery", recovering);
    std::scoped_lock lock(mutex);
    if (actor->GetFormID() != actorID || !active)
        return;
    const bool complete = state.Sample(attacking, readable, recovering);
    PublishHandoff();
    if (!complete)
        return;
    if (!reserve || !reserve(actorID, token, active, Commitment::Clear))
        return;
    ++snapshot.cleared;
    switch (state.finish) {
        case external_attack_rules::State::Finish::Animation:
            ++snapshot.animationClears;
            break;
        case external_attack_rules::State::Finish::RecoveryCycle:
            ++snapshot.recoveryClears;
            break;
        case external_attack_rules::State::Finish::Rejected:
            ++snapshot.rejectionClears;
            break;
        default:
            break;
    }
    active = 0;
}

mod::combat_tests::interference::external_attacks::Snapshot mod::combat_tests::interference::external_attacks::Read() {
    std::scoped_lock lock(mutex);
    auto result = snapshot;
    result.active = active != 0;
    result.finishing = active && handoffPublished;
    return result;
}

std::string mod::combat_tests::interference::external_attacks::Report() {
    const auto status = Read();
    std::ostringstream text;
    text << "\nExternal attack handoff: proposed " << status.proposals << "; accepted " << status.accepted
         << "; returned false " << status.rejected << "; reservations cleared " << status.cleared << "; superseded "
         << status.superseded << "; active at scope end " << status.endedActive
         << "\nPermaBlock attack handoff: proposed " << status.permaProposals << "; accepted " << status.permaAccepted
         << "; returned false " << status.permaRejected;
    text << "\nReservation clears by evidence: final animation " << status.animationClears << "; native recovery cycle "
         << status.recoveryClears << "; rejected without start " << status.rejectionClears;
    text << "\nExternal attack handoffs requested " << status.handoffs << "; finishing " << status.finishing;
    return text.str();
}
