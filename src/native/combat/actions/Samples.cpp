#include "Samples.h"

namespace mod::combat_actions {
    AttackSample ReadAttack(RE::Actor* actor) {
        AttackSample sample;
        if (!actor)
            return sample;
        const auto* state = actor->AsActorState();
        const auto attack = state->GetAttackState();
        const auto* process = actor->GetActorRuntimeData().currentProcess;
        const auto data = process && process->high ? process->high->attackData : nullptr;
        using Flag = RE::AttackData::AttackFlag;
        const bool active = attack == RE::ATTACK_STATE_ENUM::kSwing || attack == RE::ATTACK_STATE_ENUM::kHit ||
                            attack == RE::ATTACK_STATE_ENUM::kFollowThrough;
        const bool otherKind =
            data &&
            (data->IsLeftAttack() || data->data.flags.any(Flag::kPowerAttack, Flag::kBashAttack, Flag::kChargeAttack));
        sample.normalMelee = active && data && !otherKind;
        sample.conflicting = attack == RE::ATTACK_STATE_ENUM::kBash || (active && otherKind);
        bool dodging = false;
        actor->GetGraphVariableBool("bIsDodging", dodging);
        sample.interrupted = dodging || state->actorState2.staggered || state->actorState2.recoil;
        sample.idle = !actor->IsAttacking() && attack == RE::ATTACK_STATE_ENUM::kNone;
        sample.recoveryReadable = actor->GetGraphVariableBool("MCO_IsInRecovery", sample.recovering);
        return sample;
    }

    GuardSample ReadGuard(RE::Actor* actor) {
        GuardSample sample;
        if (!actor)
            return sample;
        sample.blockReadable = actor->GetGraphVariableBool("IsBlocking", sample.blocking);
        sample.wantsBlock = actor->AsActorState()->actorState2.wantBlocking;
        sample.reactionReadable = actor->GetGraphVariableBool("IsBlockHit", sample.reacting);
        sample.graphIntentReadable = actor->GetGraphVariableInt("iWantBlock", sample.graphIntent);
        return sample;
    }

    BashSample ReadBash(RE::Actor* actor) {
        BashSample sample;
        if (!actor)
            return sample;
        const auto* state = actor->AsActorState();
        const auto attack = state->GetAttackState();
        sample.bashing = attack == RE::ATTACK_STATE_ENUM::kBash;
        bool dodging = false;
        actor->GetGraphVariableBool("bIsDodging", dodging);
        sample.interrupted = dodging || state->actorState2.staggered || state->actorState2.recoil;
        sample.idle = !actor->IsAttacking() && attack == RE::ATTACK_STATE_ENUM::kNone;
        actor->GetGraphVariableBool("MCO_IsInRecovery", sample.recovering);
        return sample;
    }
}
