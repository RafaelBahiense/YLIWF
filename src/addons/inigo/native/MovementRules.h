#pragma once

namespace mod::inigo_movement_rules {
    enum class Selection { Owner, Distance, Formation, StandDown };

    struct Snapshot {
        bool following = false;
        bool waiting = false;
        bool controllerAllows = false;
        bool pending = false;
        bool loaded = false;
        bool ordinaryOnFoot = false;
        bool ordinaryPassive = false;
        bool combat = false;
        bool scene = false;
        bool normalMovement = false;
        bool ridingRequested = false;
        bool foreignInterrupt = false;
    };

    inline bool Permitted(const Snapshot& value) {
        return value.following && value.controllerAllows && !value.pending && value.loaded && value.ordinaryOnFoot &&
               !value.combat && !value.scene && value.normalMovement && !value.ridingRequested &&
               !value.foreignInterrupt;
    }

    inline bool StandDownPermitted(const Snapshot& value) {
        return (value.following || value.waiting) && value.controllerAllows && !value.pending && value.loaded &&
               value.ordinaryPassive && !value.scene && value.normalMovement && !value.ridingRequested &&
               !value.foreignInterrupt;
    }

    inline Selection Select(const Snapshot& value, bool distance, bool formation, bool standDown = false) {
        if (standDown)
            return StandDownPermitted(value) ? Selection::StandDown : Selection::Owner;
        if (!Permitted(value))
            return Selection::Owner;
        if (formation)
            return Selection::Formation;
        return distance ? Selection::Distance : Selection::Owner;
    }
}
