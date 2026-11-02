#pragma once
#include "DefenderMovementRules.h"
#include <array>
#include <cstdint>

namespace mod::defender_movement_rules {
    // Progress follows the actor, not command acceptance or a moving destination.
    class Progress {
    public:
        void Sample(Point position, float elapsed, bool active) {
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(elapsed) || elapsed <= 0)
                return;
            elapsed = std::min(elapsed, .5f);
            yield_ = std::max(0.0f, yield_ - elapsed);
            detour_ = std::max(0.0f, detour_ - elapsed);
            for (auto& item : rejected_)
                item.remaining = std::max(0.0f, item.remaining - elapsed);
            if (!initialized_ || Distance(position, previous_) > 512) {
                anchor_ = previous_ = position;
                initialized_ = true;
                waiting_ = 0;
                return;
            }
            progress_ = Distance(position, anchor_);
            previous_ = position;
            if (progress_ >= 32 || (commanded_ && Distance(position, destination_) <= 80)) {
                anchor_ = position;
                waiting_ = 0;
                consecutive_ = 0;
            } else if (active && commanded_) {
                waiting_ += elapsed;
                if (waiting_ >= 1.2f) {
                    rejected_[stalls_ % rejected_.size()] = {destination_, 6};
                    ++stalls_;
                    ++consecutive_;
                    detour_ = 2;
                    waiting_ = 0;
                    if (consecutive_ >= 3) {
                        yield_ = 2;
                        consecutive_ = 0;
                    }
                }
            }
        }

        void Command(Point destination, bool detour) {
            destination_ = destination;
            commanded_ = true;
            if (detour)
                ++detours_;
        }

        bool Blocked(Point point) const {
            return std::ranges::any_of(rejected_, [point](const auto& item) {
                return item.remaining > 0 && Distance(point, item.position) < 128;
            });
        }

        bool Detouring() const {
            return detour_ > 0;
        }

        bool Yielding() const {
            return yield_ > 0;
        }

        std::uint32_t Stalls() const {
            return stalls_;
        }

        std::uint32_t Detours() const {
            return detours_;
        }

        float Measured() const {
            return progress_;
        }

        float Waiting() const {
            return waiting_;
        }

    private:
        struct Rejected {
            Point position;
            float remaining = 0;
        };

        std::array<Rejected, 4> rejected_{};
        Point anchor_{}, previous_{}, destination_{};
        bool initialized_ = false, commanded_ = false;
        std::uint32_t stalls_ = 0, detours_ = 0, consecutive_ = 0;
        float waiting_ = 0, detour_ = 0, yield_ = 0, progress_ = 0;
    };
}
