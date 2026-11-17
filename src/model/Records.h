#pragma once

namespace model
{
    enum class RacerState
    {
        AtVertex,
        OnEdge,
        Refuelling,
        Waiting,
        Finished,
        Dnf,
    };

    enum class StrategyType
    {
        Human,
        FixedRoute,       // Opponent 1
        ShortcutLover,     // Opponent 2
        ShortestPath,       // Opponent 3
    };

    // Generous cap on a planned route's vertex/edge count. A full lap
    // through every mandatory checkpoint, even with detours, stays well
    // under this for any circuit size this project targets (drafts are
    // 18-22 vertices; even a much larger generated track would need an
    // implausibly convoluted route to exceed this).
    //
    // Racer intentionally uses fixed arrays here rather than a DynArray:
    // DynArray<T> requires T to be copy-assignable (it shuffles elements
    // with `=` when growing), and a Racer holding an owned DynArray member
    // would not be copy-assignable itself, which would break storing
    // racers in a DynArray<Racer>. Plain C arrays avoid that entirely and
    // keep Racer a simple, fully copyable value type.
    const int kMaxPlannedPathLen = 256;

    struct Racer
    {
        int id;
        char name[32];
        bool isHuman;
        StrategyType strategyType;

        RacerState state;
        int currentVertex;      // valid when AtVertex/Waiting/Refuelling/Finished/Dnf, else -1
        int currentEdge;          // valid when OnEdge, -1 otherwise
        int currentTargetVertex;   // the vertex this edge leads to (resolves edge direction)
        double edgeProgress;         // 0..1 along currentEdge

        int fuelTenths;           // 0..Rules::kMaxFuelTenths
        double fuelRemainder;      // fraction of a tenth consumed but not yet deducted (see AdvanceOnEdge)
        int refuelledAtVertex;      // vertex of the last purchase, -1 once the racer drives on
        int nitroCharges;          // 0..Rules::kMaxNitroCharges
        bool nitroActive;
        int nitroTimeLeftMs;

        int lap;                    // 1-based
        int nextCheckpoint;           // checkpoint order this racer must reach next (1-based)
        int checkpointsCompletedThisLap;

        int totalTimeMs;
        int finishTimeMs;              // -1 until Finished
        int waitingStartMs;              // -1 unless currently Waiting
        int refuelEndMs;                   // -1 unless currently Refuelling
        int bestLapTimeMs;                   // INT_MAX sentinel until a lap completes
        int currentLapStartMs;

        int plannedVertexPath[kMaxPlannedPathLen];
        int plannedVertexPathLen;
        int plannedEdgePath[kMaxPlannedPathLen];
        int plannedEdgePathLen;
        int plannedEdgeCursor;   // index into plannedEdgePath of the edge currently/next being driven
    };

    enum class RaceStatus
    {
        Finished,
        Dnf,
        TimeLimit,   // classified when the race ended by time limit, not finished
    };

    struct RaceResultRow
    {
        int racerId;
        int finishPosition;      // 1-based; 0 if never classified (shouldn't happen once race is over)
        int finishTimeMs;          // -1 if did not finish
        int bestLapTimeMs;           // INT_MAX if no lap completed
        int bonusPoints;
        int pointsAwarded;
        RaceStatus status;
        int lapsCompleted;
        int checkpointsCompletedInFinalLap;
    };

    struct ChampionshipEntry
    {
        int racerId;
        int totalPoints;
        int wins;
        int sumOfFinishPositions;
        int bestFastestLapMs;   // INT_MAX sentinel
    };
}
