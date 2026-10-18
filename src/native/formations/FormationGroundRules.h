#pragma once

#include "formations/FormationPath.h"
#include <algorithm>
#include <array>
#include <limits>
#include <span>

namespace mod::formation_ground {
    using formation_rules::Point;

    struct Triangle {
        std::array<Point, 3> vertices;
    };

    inline constexpr float SearchRadius = 256;
    inline constexpr float MaximumHeightChange = 192;
    inline constexpr float MinimumSeparation = 96;
    inline constexpr float StableRadius = 32;
    inline constexpr float EdgeInset = 16;
    inline constexpr float AnchorRadius = 64;
    inline constexpr float AnchorHeight = 96;

    inline bool Finite(Point point) {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }

    inline float SquaredDistance(Point a, Point b) {
        const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
        return x * x + y * y + z * z;
    }

    inline Point EdgePoint(Point desired, Point a, Point b) {
        const float x = b.x - a.x, y = b.y - a.y;
        const float length = x * x + y * y;
        const float fraction =
            length > 0 ? std::clamp(((desired.x - a.x) * x + (desired.y - a.y) * y) / length, 0.0f, 1.0f) : 0;
        return {a.x + x * fraction, a.y + y * fraction, a.z + (b.z - a.z) * fraction};
    }

    // Project in XY, then interpolate the surface height. Vertical or
    // malformed triangles do not provide usable ground.
    inline std::optional<Point> OnTriangle(Point desired, const Triangle& triangle) {
        const auto [a, b, c] = triangle.vertices;
        if (!Finite(desired) || !Finite(a) || !Finite(b) || !Finite(c))
            return {};
        const double denominator = double(b.y - c.y) * (a.x - c.x) + double(c.x - b.x) * (a.y - c.y);
        if (std::abs(denominator) < 0.001)
            return {};
        const double first =
            (double(b.y - c.y) * (desired.x - c.x) + double(c.x - b.x) * (desired.y - c.y)) / denominator;
        const double second =
            (double(c.y - a.y) * (desired.x - c.x) + double(a.x - c.x) * (desired.y - c.y)) / denominator;
        const double third = 1 - first - second;
        if (first >= 0 && second >= 0 && third >= 0)
            return Point{desired.x, desired.y, static_cast<float>(first * a.z + second * b.z + third * c.z)};

        const std::array edges{EdgePoint(desired, a, b), EdgePoint(desired, b, c), EdgePoint(desired, c, a)};
        auto closest = *std::ranges::min_element(edges, [&](Point left, Point right) {
            return formation_rules::Distance(desired, left) < formation_rules::Distance(desired, right);
        });
        // A point exactly on a navmesh boundary is fragile. Move a small amount
        // into the triangle; tiny triangles use their center instead.
        const Point center{(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3};
        const float length = formation_rules::Distance(closest, center);
        const float fraction = length > 0 ? std::min(1.0f, EdgeInset / length) : 1;
        closest = {closest.x + (center.x - closest.x) * fraction, closest.y + (center.y - closest.y) * fraction,
                   closest.z + (center.z - closest.z) * fraction};
        return closest;
    }

    inline bool Suitable(Point desired, Point candidate, std::span<const Point> occupied) {
        return Finite(candidate) && formation_rules::Distance(desired, candidate) <= SearchRadius &&
               std::abs(desired.z - candidate.z) <= MaximumHeightChange &&
               std::ranges::none_of(occupied, [&](Point other) {
                   return SquaredDistance(candidate, other) < MinimumSeparation * MinimumSeparation;
               });
    }

    inline bool SuitableAnchor(Point player, Point ground) {
        return Finite(player) && Finite(ground) && formation_rules::Distance(player, ground) <= AnchorRadius &&
               std::abs(player.z - ground.z) <= AnchorHeight;
    }

    inline std::optional<Point> Project(Point desired, std::span<const Triangle> triangles,
                                        std::span<const Point> occupied = {}, std::optional<Point> previous = {}) {
        if (!Finite(desired))
            return {};
        if (previous && SquaredDistance(desired, *previous) <= StableRadius * StableRadius &&
            Suitable(desired, *previous, occupied))
            for (const auto& triangle : triangles)
                if (const auto surface = OnTriangle(*previous, triangle);
                    surface && SquaredDistance(*surface, *previous) <= 4)
                    return previous;  // Revalidate against the current snapshot, not stale engine pointers.

        std::optional<Point> result;
        float best = std::numeric_limits<float>::max();
        for (const auto& triangle : triangles) {
            const auto& vertices = triangle.vertices;
            if (desired.x < std::min({vertices[0].x, vertices[1].x, vertices[2].x}) - SearchRadius ||
                desired.x > std::max({vertices[0].x, vertices[1].x, vertices[2].x}) + SearchRadius ||
                desired.y < std::min({vertices[0].y, vertices[1].y, vertices[2].y}) - SearchRadius ||
                desired.y > std::max({vertices[0].y, vertices[1].y, vertices[2].y}) + SearchRadius)
                continue;
            const auto candidate = OnTriangle(desired, triangle);
            if (!candidate || !Suitable(desired, *candidate, occupied))
                continue;
            const float score = SquaredDistance(desired, *candidate);
            if (score < best) {
                best = score;
                result = candidate;
            }
        }
        return result;
    }
}
