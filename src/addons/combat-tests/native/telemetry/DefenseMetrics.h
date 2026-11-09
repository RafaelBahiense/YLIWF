#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mod::combat_tests::metrics {
    struct Snapshot {
        std::uint32_t dodgeEpisodes = 0, cleanDodgeEpisodes = 0, recoveryEntries = 0, recoveryExits = 0;
        std::uint32_t partialDodgeEpisodes = 0, dodgeUnavailableSamples = 0;
        float dodgeNetLoss = 0, cleanDodgeNetLoss = 0, lowStaminaSeconds = 0, zeroStaminaSeconds = 0;
        float recoverySeconds = 0, completedRecoverySeconds = 0, longestRecovery = 0;
        float damage = 0, seconds = 0;
    };

    struct Sample {
        float elapsed = 0, stamina = 0, maximum = 0, health = 0;
        bool dodging = false, attacking = false, recovering = false;
        std::uint32_t incoming = 0, outgoing = 0, fixtureSteps = 0;
        bool dodgeReadable = true;
        bool recoveryReadable = true;
        float recoveryEntryFraction = .4f, recoveryExitFraction = .65f;
    };

    class Tracker {
        bool initialized = false, dodgeOpen = false, contaminated = false, recoveryOpen = false;
        float dodgeInitial = 0, dodgeMinimum = 0, recoveryDuration = 0;
        Sample previous;
        Snapshot snapshot;

        static bool RecoveryAvailable(const Sample& sample) {
            return sample.recoveryReadable && std::isfinite(sample.maximum) && sample.maximum > 0 &&
                   std::isfinite(sample.recoveryEntryFraction) && std::isfinite(sample.recoveryExitFraction) &&
                   sample.recoveryEntryFraction >= 0 && sample.recoveryEntryFraction < sample.recoveryExitFraction &&
                   sample.recoveryExitFraction <= 1;
        }

        static bool EnteredRecovery(const Sample& sample) {
            return RecoveryAvailable(sample) && sample.recovering &&
                   sample.stamina <= sample.maximum * sample.recoveryEntryFraction;
        }

        void CloseDodge() {
            if (!dodgeOpen)
                return;
            const auto loss = std::max(0.0f, dodgeInitial - dodgeMinimum);
            snapshot.dodgeNetLoss += loss;
            if (!contaminated) {
                ++snapshot.cleanDodgeEpisodes;
                snapshot.cleanDodgeNetLoss += loss;
            }
            dodgeOpen = false;
        }

    public:
        void Observe(const Sample& sample) {
            if (!std::isfinite(sample.elapsed) || !std::isfinite(sample.stamina) || !std::isfinite(sample.health))
                return;
            snapshot.dodgeUnavailableSamples += !sample.dodgeReadable;
            if (!initialized) {
                initialized = true;
                previous = sample;
                if (EnteredRecovery(sample)) {
                    recoveryOpen = true;
                    ++snapshot.recoveryEntries;
                }
                return;
            }
            if (sample.elapsed < previous.elapsed)
                return;
            const float delta = sample.elapsed - previous.elapsed;
            snapshot.seconds += delta;
            snapshot.damage += std::max(0.0f, std::max(previous.health, 0.0f) - std::max(sample.health, 0.0f));
            snapshot.lowStaminaSeconds += sample.stamina < sample.maximum * .2f ? delta : 0;
            snapshot.zeroStaminaSeconds += sample.stamina <= 1 ? delta : 0;
            if (EnteredRecovery(sample) && !recoveryOpen) {
                recoveryOpen = true;
                recoveryDuration = 0;
                ++snapshot.recoveryEntries;
            }
            if (recoveryOpen) {
                recoveryDuration += delta;
                snapshot.recoverySeconds += delta;
                snapshot.longestRecovery = std::max(snapshot.longestRecovery, recoveryDuration);
                if (RecoveryAvailable(sample) && !sample.recovering &&
                    sample.stamina >= sample.maximum * sample.recoveryExitFraction) {
                    ++snapshot.recoveryExits;
                    snapshot.completedRecoverySeconds += recoveryDuration;
                    recoveryOpen = false;
                }
            }
            if (sample.dodgeReadable && sample.dodging && !dodgeOpen) {
                CloseDodge();
                dodgeOpen = true;
                contaminated = !previous.dodgeReadable || previous.dodging;
                dodgeInitial = previous.stamina;
                dodgeMinimum = std::min(sample.stamina, previous.stamina);
                ++snapshot.dodgeEpisodes;
            }
            if (dodgeOpen) {
                dodgeMinimum = std::min(dodgeMinimum, sample.stamina);
                contaminated |= !sample.dodgeReadable || sample.attacking || previous.attacking ||
                                sample.incoming != previous.incoming || sample.outgoing != previous.outgoing ||
                                sample.fixtureSteps != previous.fixtureSteps;
                if (sample.dodgeReadable && !sample.dodging)
                    CloseDodge();
            }
            previous = sample;
        }

        void Finish() {
            if (dodgeOpen) {
                ++snapshot.partialDodgeEpisodes;
                contaminated = true;
            }
            CloseDodge();
        }

        Snapshot Read() const {
            return snapshot;
        }
    };

    constexpr const char* Unavailable(bool exists, bool dead, bool downed, bool disabled, bool scene, bool equipped,
                                      bool role, bool allowed) {
        return !exists     ? "Test follower is unavailable."
               : dead      ? "Test follower died."
               : downed    ? "Test follower downed: bleeding out or health depleted."
               : disabled  ? "Test follower was disabled."
               : scene     ? "Test follower entered a scene."
               : !equipped ? "Test follower lost the required equipment or spells."
               : !role     ? "Test follower combat role changed."
               : !allowed  ? "Test follower combat permission changed."
                           : nullptr;
    }
}
