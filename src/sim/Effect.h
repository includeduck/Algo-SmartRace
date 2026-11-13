#pragma once

namespace sim
{
    enum class EffectType
    {
        OilSpill,
        SpeedRestriction,
    };

    // One independently-tracked temporary effect on one edge. Project.pdf
    // section 4.5: "store each effect independently with its own
    // expiry/event ID... expiring one effect must not erase another effect
    // that is still active" — effectId is exactly that independent handle.
    struct ActiveEffect
    {
        int effectId;
        EffectType type;
        double magnitude;    // OilSpill: difficulty bonus; SpeedRestriction: cap fraction of base speed
        int expiresAtMs;
    };
}
