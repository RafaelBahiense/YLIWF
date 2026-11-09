#include "ProjectileObservation.h"
#include "ProjectileRules.h"
#include <array>

namespace mod::projectile_observation {
    Snapshot Read(RE::Actor* actor) {
        Snapshot result;
        auto* manager = RE::Projectile::Manager::GetSingleton();
        if (!actor || !manager || !actor->Is3DLoaded())
            return result;
        std::array<RE::ProjectileHandle, 128> handles{};
        std::size_t count = 0;
        {
            RE::BSSpinLockGuard lock(manager->projectileLock);
            if (manager->unlimited.size() + manager->limited.size() + manager->pending.size() > handles.size())
                return result;
            for (const auto* list : {&manager->unlimited, &manager->limited, &manager->pending})
                for (const auto handle : *list)
                    handles[count++] = handle;
        }
        result.readable = true;
        for (std::size_t i = 0; i < count; ++i) {
            const auto projectile = handles[i].get();
            if (!projectile)
                continue;
            if (projectile->GetWorldspace() != actor->GetWorldspace() ||
                (!actor->GetWorldspace() && projectile->GetParentCell() != actor->GetParentCell()))
                continue;
            ++result.examined;
            const auto& data = projectile->GetProjectileRuntimeData();
            if (data.flags & static_cast<std::uint32_t>(RE::Projectile::Flags::kDestroyed))
                continue;
            const auto shooter = data.shooter.get();
            if (shooter.get() == actor)
                continue;
            const auto position = projectile->GetPosition(), origin = actor->GetPosition();
            const auto velocity = data.velocity;
            if (velocity.SqrLength() < 1)
                continue;
            const auto approach =
                projectile_rules::Observe({position.x - origin.x, position.y - origin.y, position.z - origin.z - 64},
                                          {velocity.x, velocity.y, velocity.z}, 160);
            if (!approach.readable)
                result.readable = false;
            if (approach.incoming) {
                // Unknown ownership cannot certify an archer's empty flight path.
                if (!shooter)
                    result.readable = false;
                result.incoming[result.count++] = {shooter ? shooter->GetFormID() : 0, approach.seconds};
            }
        }
        return result;
    }
}
