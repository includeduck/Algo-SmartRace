#pragma once

namespace sim
{
    // Priorities from Project.pdf section 4.5's table: lower fires first on
    // a trigger-time tie. Each *Expire type inherits its parent effect's
    // priority (not listed separately in the brief, but keeps an expiry
    // sorted sensibly against other events at the same instant).
    enum class EventType
    {
        RoadBlock,              // priority 1
        RoadClear,                // priority 1
        ShortcutToggle,             // priority 2
        OilSpill,                     // priority 3
        OilSpillExpire,                 // priority 3
        SpeedRestriction,                 // priority 4
        SpeedRestrictionExpire,              // priority 4
        FuelStationToggle,                      // priority 5
        FuelStationReactivate,                     // priority 5
        NitroBooster,                                 // priority 6
        NitroBoosterExpire,                              // priority 6
    };

    struct Event
    {
        int eventId;
        EventType type;
        int triggerTimeMs;
        int priority;
        int insertionSequence;   // final deterministic tiebreak
        int target;                 // edge id or vertex id, depending on type
        int durationMs;
        int effectId;                  // links an expiry event to the ActiveEffect it clears (-1 if n/a)
        bool cancelled;                   // lazy deletion: set by Cancel(), skipped when popped
        bool fired;                          // true once popped and (if not cancelled) applied
    };
}
