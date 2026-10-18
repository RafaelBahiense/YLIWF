#include "combat/HealerMovement.h"
#include "combat/HealingRules.h"
#include "combat/HealerMovementRules.h"
#include "combat/HealingGeometry.h"
#include "combat/CombatOrderRules.h"

#include <numbers>
#include <chrono>

namespace mod::healer_movement {
    namespace {
        constexpr std::size_t MaximumTriangles = 150000;
        std::vector<formation_ground::Triangle> triangles;
        RE::TESObjectCELL* area = nullptr;
        RE::TESWorldSpace* currentWorld = nullptr;
        auto refreshed = std::chrono::steady_clock::time_point::min();

        Point Position(RE::Actor* actor) {
            const auto p = actor->GetPosition();
            return {p.x, p.y, p.z};
        }

        void Collect(RE::TESObjectCELL* cell, RE::TESWorldSpace* world) {
            if (!cell || !cell->IsAttached())
                return;
            auto& runtime = cell->GetRuntimeData();
            RE::BSSpinLockGuard lock(runtime.spinLock);
            if (!runtime.navMeshes || runtime.worldSpace != world)
                return;
            for (const auto& mesh : runtime.navMeshes->navMeshes) {
                if (!mesh || mesh->IsDeleted())
                    continue;
                for (const auto& face : mesh->triangles) {
                    if (triangles.size() >= MaximumTriangles)
                        return;
                    if (face.triangleFlags.any(RE::BSNavmeshTriangle::TriangleFlag::kDeleted) ||
                        face.vertices[0] >= mesh->vertices.size() || face.vertices[1] >= mesh->vertices.size() ||
                        face.vertices[2] >= mesh->vertices.size())
                        continue;
                    formation_ground::Triangle triangle{};
                    for (int i = 0; i < 3; ++i) {
                        const auto& p = mesh->vertices[face.vertices[i]].location;
                        triangle.vertices[i] = {p.x, p.y, p.z};
                    }
                    triangles.push_back(triangle);
                }
            }
        }

        void Refresh(RE::Actor* healer) {
            const auto now = std::chrono::steady_clock::now();
            auto* world = healer->GetWorldspace();
            if (currentWorld == world && (world || area == healer->GetParentCell()) &&
                refreshed != std::chrono::steady_clock::time_point::min() && now - refreshed < std::chrono::seconds(1))
                return;
            area = healer->GetParentCell();
            currentWorld = world;
            refreshed = now;
            triangles.clear();
            if (!world) {
                Collect(area, nullptr);
                return;
            }
            auto* game = RE::TES::GetSingleton();
            auto* grid = game ? game->gridCells : nullptr;
            if (grid && grid->cells)
                for (std::uint32_t x = 0; x < grid->length; ++x)
                    for (std::uint32_t y = 0; y < grid->length; ++y)
                        Collect(grid->GetCell(x, y), world);
            if (triangles.size() >= MaximumTriangles)
                triangles.clear();
        }
    }

    std::vector<Point> Threats(RE::Actor* healer, bool includePlayerEnemies) {
        if (!healer)
            return {};
        std::vector<RE::ActorHandle> handles;
        std::vector<RE::ActorHandle> playerTargets;
        auto* combat = healer ? healer->GetActorRuntimeData().combatController : nullptr;
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* playerCombat = player ? player->GetActorRuntimeData().combatController : nullptr;
        for (auto* controller : {combat, playerCombat}) {
            auto* group = controller ? controller->combatGroup : nullptr;
            if (!group)
                continue;
            RE::BSReadLockGuard lock(group->lock);
            for (const auto& target : group->targets) {
                if (std::ranges::find(handles, target.targetHandle) == handles.end())
                    handles.push_back(target.targetHandle);
                if (includePlayerEnemies && controller == playerCombat)
                    playerTargets.push_back(target.targetHandle);
            }
        }
        if (includePlayerEnemies)
            if (auto* processes = RE::ProcessLists::GetSingleton())
                for (const auto& handle : processes->highActorHandles)
                    if (std::ranges::find(handles, handle) == handles.end())
                        handles.push_back(handle);
        std::vector<Point> result;
        for (const auto& handle : handles) {
            auto enemy = handle.get();
            if (enemy && !enemy->IsDead() && !enemy->IsDisabled() && enemy->Is3DLoaded() &&
                ((healer->GetWorldspace() && healer->GetWorldspace() == enemy->GetWorldspace()) ||
                 healer->GetParentCell() == enemy->GetParentCell()) &&
                (healer->IsHostileToActor(enemy.get()) || enemy->IsHostileToActor(healer) ||
                 (includePlayerEnemies && player &&
                  (player->IsHostileToActor(enemy.get()) || enemy->IsHostileToActor(player) ||
                   (!enemy->IsPlayerTeammate() && std::ranges::find(playerTargets, handle) != playerTargets.end())))) &&
                enemy->GetPosition().GetDistance(healer->GetPosition()) <= healing_rules::SearchRange)
                result.push_back(Position(enemy.get()));
        }
        return result;
    }

    std::optional<Point> PassiveDestination(RE::Actor* actor, RE::Actor* player, float followRadius,
                                            std::span<const Point> threats) {
        if (!actor || !player || threats.empty())
            return {};
        Refresh(actor);
        const auto origin = Position(actor), center = Position(player);
        std::optional<Point> selected;
        float best = std::numeric_limits<float>::max();
        for (const float radius : {followRadius, std::max(followRadius, 640.0f)})
            for (int direction = 0; direction < 12; ++direction) {
                const float angle = direction * std::numbers::pi_v<float> / 6;
                const Point desired{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius, center.z};
                const auto candidate = formation_ground::Project(desired, triangles);
                if (!candidate)
                    continue;
                const auto score =
                    combat_order_rules::AvoidanceScore(origin, center, *candidate, threats, followRadius);
                if (score && *score < best) {
                    best = *score;
                    selected = candidate;
                }
            }
        return selected;
    }

    float ThreatDistance(Point position, std::span<const Point> threats) {
        return healing_rules::NearestThreat(position, threats);
    }

    std::optional<Point> Destination(RE::Actor* healer, RE::Actor* ally, float range, std::span<const Point> threats,
                                     Purpose purpose) {
        if (!healer || !ally || !std::isfinite(range) || range <= 0)
            return {};
        Refresh(healer);
        const auto origin = Position(healer), center = Position(ally);
        const bool cautious = purpose != Purpose::Seek;
        const float radius = cautious ? std::min(range * 0.9f, healing_rules::SafeDistance * 1.25f)
                                      : std::min(range * 0.7f, healing_rules::SafeDistance);
        std::optional<Point> selected;
        float best = std::numeric_limits<float>::max();
        // Sample around the ally, then project onto loaded navmesh surfaces.
        // Skyrim performs actual route finding; projection is not a reachability proof.
        for (int i = 0; i < 8; ++i) {
            const float angle = i * std::numbers::pi_v<float> / 4;
            const Point desired{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius, center.z};
            const auto candidate = formation_ground::Project(desired, triangles);
            if (!candidate)
                continue;
            if (purpose == Purpose::Retreat && !healing_rules::EscapeDirection(origin, *candidate, threats))
                continue;
            const auto score =
                healing_rules::MovementScore(origin, center, *candidate, range, threats,
                                             cautious ? healing_rules::SafeDistance : healing_rules::ThreatDistance);
            if (!score)
                continue;
            const auto sight = healing_geometry::From(
                healer, {candidate->x, candidate->y, candidate->z + healer->GetHeight() * 0.8f}, ally);
            if (!sight.available || !sight.clear)
                continue;
            if (*score < best) {
                best = *score;
                selected = candidate;
            }
        }
        return selected;
    }

    void Clear() {
        triangles.clear();
        area = nullptr;
        currentWorld = nullptr;
        refreshed = std::chrono::steady_clock::time_point::min();
    }
}
