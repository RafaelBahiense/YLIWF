#pragma once

#include <cmath>
#include <deque>
#include <iterator>
#include <optional>

namespace mod::formation_rules {
    struct Point {
        float x, y, z;
    };

    inline float Distance(Point a, Point b) {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    // Sample the route actually walked, rather than rotating slots with facing.
    class Path {
    public:
        void Clear() {
            points.clear();
            heading.reset();
        }

        void Observe(Point position) {
            if (!points.empty()) {
                const float distance = Distance(position, points.front());
                if (distance > 1024 || std::abs(position.z - points.front().z) > 256)
                    Clear();  // Teleports and abrupt vertical changes are not a walkable route.
                else if (distance < 16)
                    return;  // Accumulate small movement; ignore idle position jitter.
            }
            if (!points.empty()) {
                const float distance = Distance(position, points.front());
                const Point movement{(position.x - points.front().x) / distance,
                                     (position.y - points.front().y) / distance, 0};
                if (heading) {
                    const float weight = 1 - std::exp(-distance / 160);
                    // Interpolate angles so even an exact reversal converges.
                    const float turn = std::atan2(heading->x * movement.y - heading->y * movement.x,
                                                  heading->x * movement.x + heading->y * movement.y) *
                                       weight;
                    const float cosine = std::cos(turn), sine = std::sin(turn);
                    heading =
                        Point{heading->x * cosine - heading->y * sine, heading->x * sine + heading->y * cosine, 0};
                } else {
                    heading = movement;
                }
            }
            points.push_front(position);
            if (points.size() > 512)
                points.pop_back();
        }

        std::optional<Point> Behind(float distance) const {
            if (points.empty() || distance < 0)
                return {};
            if (distance == 0)
                return points.front();
            auto previous = points.begin();
            for (auto current = std::next(previous); current != points.end(); ++current) {
                const float length = Distance(*previous, *current);
                if (length >= distance && length > 0) {
                    const float fraction = distance / length;
                    return Point{previous->x + (current->x - previous->x) * fraction,
                                 previous->y + (current->y - previous->y) * fraction,
                                 previous->z + (current->z - previous->z) * fraction};
                }
                distance -= length;
                previous = current;
            }
            return {};  // Stay with ordinary following until enough route is known.
        }

        std::optional<Point> Heading() const {
            return heading;
        }

    private:
        std::deque<Point> points;
        std::optional<Point> heading;
    };
}
