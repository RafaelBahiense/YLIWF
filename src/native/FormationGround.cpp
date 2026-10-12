#include "FormationGround.h"
#include "FormationGroundRules.h"
#include <chrono>
#include <vector>

namespace mod::formation_ground {
    namespace {
        constexpr std::size_t MaximumTriangles = 150000;
        std::vector<Triangle> triangles;

        struct Query {
            std::optional<Point> desired;
            std::optional<Point> result;

            std::optional<Point> Evaluate(Point point, std::span<const Point> occupied = {}) {
                if (desired && SquaredDistance(*desired, point) < StableRadius * StableRadius &&
                    (!result || Suitable(point, *result, occupied)))
                    return result;
                result = Project(point, triangles, occupied, result);
                desired = point;
                return result;
            }
        };

        std::array<Query, formation_geometry::Slots> slots{};
        Query anchor;
        RE::TESWorldSpace* world = nullptr;
        auto refreshed = std::chrono::steady_clock::time_point::min();

        void Collect(RE::TESObjectCELL* cell) {
            if (!cell || !cell->IsAttached() || cell->IsInteriorCell())
                return;
            auto& runtime = cell->GetRuntimeData();
            RE::BSSpinLockGuard lock(runtime.spinLock);
            if (runtime.worldSpace != world || !runtime.navMeshes)
                return;
            for (const auto& mesh : runtime.navMeshes->navMeshes) {
                if (!mesh || mesh->IsDeleted())
                    continue;
                for (const auto& triangle : mesh->triangles) {
                    if (triangle.triangleFlags.any(RE::BSNavmeshTriangle::TriangleFlag::kDeleted) ||
                        triangle.vertices[0] >= mesh->vertices.size() ||
                        triangle.vertices[1] >= mesh->vertices.size() || triangle.vertices[2] >= mesh->vertices.size())
                        continue;
                    if (triangles.size() == MaximumTriangles)
                        return;
                    Triangle surface{};
                    for (int i = 0; i < 3; ++i) {
                        const auto& vertex = mesh->vertices[triangle.vertices[i]].location;
                        surface.vertices[i] = {vertex.x, vertex.y, vertex.z};
                    }
                    triangles.push_back(surface);
                }
            }
        }
    }

    void Clear() {
        triangles.clear();
        slots = {};
        anchor = {};
        world = nullptr;
        refreshed = std::chrono::steady_clock::time_point::min();
    }

    void Refresh(RE::Actor* player) {
        auto* currentWorld = player ? player->GetWorldspace() : nullptr;
        if (currentWorld != world) {
            Clear();
            world = currentWorld;
        }
        const auto now = std::chrono::steady_clock::now();
        if (!world ||
            (refreshed != std::chrono::steady_clock::time_point::min() && now - refreshed < std::chrono::seconds(1)))
            return;
        triangles.clear();
        refreshed = now;
        // Recheck both accepted and rejected points against each new snapshot.
        // Between snapshots, query only when a goal moves meaningfully.
        anchor.desired.reset();
        for (auto& slot : slots)
            slot.desired.reset();
        const auto* game = RE::TES::GetSingleton();
        const auto* grid = game ? game->gridCells : nullptr;
        if (!grid || !grid->cells)
            return;
        for (std::uint32_t x = 0; x < grid->length; ++x)
            for (std::uint32_t y = 0; y < grid->length; ++y) {
                Collect(grid->GetCell(x, y));
                if (triangles.size() == MaximumTriangles) {
                    // A partial snapshot can favor an arbitrary cell. Fall back
                    // to ordinary following rather than searching incomplete data.
                    triangles.clear();
                    return;
                }
            }
    }

    std::optional<formation_rules::Point> Anchor(formation_rules::Point player) {
        const auto ground = anchor.Evaluate(player);
        if (!ground || !SuitableAnchor(player, *ground))
            return {};
        return ground;
    }

    formation_geometry::Destinations Adjust(const formation_geometry::Destinations& desired) {
        formation_geometry::Destinations result{};
        std::vector<Point> occupied;
        occupied.reserve(desired.size());
        for (std::size_t slot = 0; slot < desired.size(); ++slot) {
            if (desired[slot])
                result[slot] = slots[slot].Evaluate(*desired[slot], occupied);
            else
                slots[slot] = {};
            if (result[slot])
                occupied.push_back(*result[slot]);
        }
        return result;
    }

    std::size_t TriangleCount() {
        return triangles.size();
    }
}
