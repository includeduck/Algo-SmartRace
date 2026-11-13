#pragma once

namespace model
{
    enum class VertexType
    {
        Start,
        Finish,
        Checkpoint,
        Intersection,
        FuelStation,
        NitroPickup,
        DeadEnd,
    };

    enum class EdgeStatus
    {
        Open,
        Blocked,
        Restricted,
    };

    struct Vertex
    {
        int id;
        double x, y;
        VertexType type;
        int checkpointOrder;   // -1 if this vertex is not a mandatory checkpoint
        bool active;             // false for a deactivated service vertex
    };

    struct Edge
    {
        int id;
        int from, to;
        double distance, baseTime, fuelCost;   // base values from the track file (immutable)
        int difficulty;                          // base, 1 (easy) .. 5 (hard)
        bool isShortcut;
        bool directed;                            // if false, traversable both ways
        EdgeStatus baseStatus;                    // persists across effect expiry

        // Effective state. Recomputed by EffectManager once it exists
        // (Phase 2); until then effective always equals base.
        EdgeStatus status;
        int effDifficulty;
        double speedCap;           // <= 0 means no active cap
        double obstaclePenalty;    // 0..1
    };

    // One node of a vertex's adjacency list. Owned by the Graph's internal
    // Pool<AdjNode> — never new'd/deleted individually.
    struct AdjNode
    {
        int edgeId;
        int neighbor;
        AdjNode* next;
    };
}
