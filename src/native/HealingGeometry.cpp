#include "HealingGeometry.h"

#include <cmath>

namespace mod::healing_geometry {
    namespace {
        bool Finite(const RE::NiPoint3& point) {
            return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
        }

        RE::NiPoint3 TargetPoint(RE::Actor* actor) {
            if (auto* root = actor->Get3D())
                if (auto* chest = root->GetObjectByName("NPC Spine2 [Spn2]"))
                    return chest->world.translate;  // Follows the actual bleedout pose.
            auto position = actor->GetPosition();
            position.z += actor->GetHeight() * 0.5f;
            return position;
        }
    }

    Sight Check(RE::Actor* caster, RE::Actor* target) {
        if (!caster)
            return {};
        RE::NiPoint3 from, direction;
        caster->GetEyeVector(from, direction, false);
        return From(caster, from, target);
    }

    Sight From(RE::Actor* caster, const RE::NiPoint3& origin, RE::Actor* target) {
        auto* cell = caster ? caster->GetParentCell() : nullptr;
        auto* world = cell ? cell->GetbhkWorld() : nullptr;
        if (!world || !world->GetWorld1() || !target || !caster->Is3DLoaded() || !target->Is3DLoaded())
            return {};

        auto from = origin;
        const auto to = TargetPoint(target);
        const auto scale = RE::bhkWorld::GetWorldScale();
        if (!Finite(from) || !Finite(to) || !std::isfinite(scale) || scale <= 0)
            return {};

        RE::bhkPickData pick{};
        pick.rayInput.to = RE::hkVector4(to * scale);
        pick.rayInput.filterInfo = static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);
        pick.rayInput.enableShapeCollectionFilter = true;
        RE::BSReadLockGuard lock(world->worldLock);
        // Eye rays can hit the healer's own body when looking behind/down.
        // Skip only that actor, advancing just past its hit surface; every
        // subsequent wall or other reference still blocks the ray.
        for (int attempt = 0; attempt < 8; ++attempt) {
            pick.rayInput.from = RE::hkVector4(from * scale);
            pick.rayOutput.Reset();
            world->PickObject(pick);
            if (!pick.rayOutput.HasHit())
                return {true, true, 0};
            if (!pick.rayOutput.rootCollidable)
                return {true, false, 0};
            auto* hit = RE::TESHavokUtilities::FindCollidableRef(*pick.rayOutput.rootCollidable);
            if (hit != caster)
                return {true, hit == target, hit ? hit->GetFormID() : 0};
            const auto remaining = to - from;
            const float length = from.GetDistance(to);
            const float fraction = pick.rayOutput.hitFraction;
            if (!std::isfinite(fraction) || fraction < 0 || fraction >= 1 || length <= 1)
                return {true, false, caster->GetFormID()};
            from += remaining * std::min(1.0f, fraction + 1.0f / length);
        }
        return {true, false, caster->GetFormID()};
    }
}
