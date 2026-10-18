#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace mod::healing_rules {
    inline constexpr float HealingWindow = 6;

    struct HealingEffect {
        float magnitude = 0;
        float duration = 0;
    };

    struct HealingBudget {
        float magicka = 0, maximumMagicka = 0, regeneration = 0, regenerationDelay = 0;
    };

    namespace detail {
        inline constexpr float ForecastStep = 0.1f;
        inline constexpr int ForecastTicks = static_cast<int>(HealingWindow / ForecastStep);

        class HealingForecast {
        public:
            HealingForecast(std::span<const HealingEffect> effects, float cost, float charge, float recovery,
                            const HealingBudget& budget)
                : effects_(effects),
                  cost_(cost),
                  charge_(std::max(ForecastStep, charge)),
                  recovery_(recovery),
                  budget_(budget),
                  magicka_(budget.magicka),
                  expires_(effects.size(), 0) {
                StartCast(0);
            }

            float Run(float missingHealth) {
                for (int tick = 1; tick <= ForecastTicks; ++tick) {
                    const float time = static_cast<float>(tick) * ForecastStep;
                    RestoreOngoingHealing(time);
                    if (charging_ && time >= releaseAt_)
                        ReleaseSpell(time);
                    RegenerateMagicka(time);
                    if (!charging_ && time >= readyAt_ && magicka_ >= cost_)
                        StartCast(time);
                }
                return std::min(healed_, missingHealth) / HealingWindow;
            }

        private:
            void RestoreOngoingHealing(float time) {
                const float previous = time - ForecastStep;
                for (std::size_t i = 0; i < effects_.size(); ++i)
                    if (ValidEffect(effects_[i]) && expires_[i] > previous)
                        healed_ += effects_[i].magnitude * std::min(ForecastStep, expires_[i] - previous);
            }

            void ReleaseSpell(float time) {
                charging_ = false;
                for (std::size_t i = 0; i < effects_.size(); ++i) {
                    const auto& effect = effects_[i];
                    if (!ValidEffect(effect))
                        continue;
                    if (effect.duration == 0)
                        healed_ += effect.magnitude;
                    else
                        expires_[i] = time + effect.duration;
                }
            }

            void RegenerateMagicka(float time) {
                if (charging_ || time <= regenerationAt_)
                    return;
                const float elapsed = std::min(ForecastStep, time - regenerationAt_);
                magicka_ = std::min(budget_.maximumMagicka, magicka_ + std::max(0.0f, budget_.regeneration) * elapsed);
            }

            void StartCast(float time) {
                magicka_ -= cost_;
                charging_ = true;
                releaseAt_ = time + charge_;
                readyAt_ = releaseAt_ + recovery_;
                regenerationAt_ = releaseAt_ + std::max(0.0f, budget_.regenerationDelay);
            }

            static bool ValidEffect(const HealingEffect& effect) {
                return std::isfinite(effect.magnitude) && effect.magnitude > 0 && std::isfinite(effect.duration) &&
                       effect.duration >= 0;
            }

            std::span<const HealingEffect> effects_;
            float cost_, charge_, recovery_;
            HealingBudget budget_;
            float magicka_, healed_ = 0;
            float releaseAt_ = 0, readyAt_ = 0, regenerationAt_ = 0;
            bool charging_ = false;
            std::vector<float> expires_;
        };
    }

    // A short forecast, not a prediction of incoming damage or scripted effects.
    // Duration effects refresh rather than stacking with repeated casts.
    inline float HealingPerSecond(std::span<const HealingEffect> effects, float cost, float charge, float recovery,
                                  const HealingBudget& budget, float missingHealth) {
        if (!std::isfinite(cost) || cost < 0 || !std::isfinite(charge) || charge < 0 || !std::isfinite(recovery) ||
            recovery < 0 || !std::isfinite(missingHealth) || missingHealth <= 0 || !std::isfinite(budget.magicka) ||
            !std::isfinite(budget.maximumMagicka) || !std::isfinite(budget.regeneration) ||
            !std::isfinite(budget.regenerationDelay) || budget.magicka < cost || budget.maximumMagicka < cost)
            return 0;
        return detail::HealingForecast(effects, cost, charge, recovery, budget).Run(missingHealth);
    }
}
