#pragma once

#include "../model/Graph.h"

namespace sim
{
    // Every rule a playable circuit must satisfy (Project.pdf sections 4.1
    // and 5.4: "a connected graph is not automatically playable"). The
    // order here is the order checks run and are reported in.
    enum class ValidationCheck
    {
        HasStartAndFinish,
        CheckpointsComplete,           // orders 1..K, K >= 10, each used once
        DistancesAdmissible,             // distance >= straight line (keeps A* admissible)
        Connected,
        CheckpointsReachableInOrder,       // Start -> CP1 -> ... -> CPK -> Finish, directed
        LapReturns,                          // Finish -> CP1 reachable, so a second lap exists
        HasCycle,
        HasDeadEnd,
        EnoughShortcuts,
        HasAlternativeRoute,                   // a second Start -> Finish route
        FuelPlanFeasible,                        // some refuelling plan finishes the whole race
        Count,
    };

    const int kValidationCheckCount = static_cast<int>(ValidationCheck::Count);

    struct ValidationReport
    {
        bool passed;
        bool check[kValidationCheckCount];
        char detail[kValidationCheckCount][112];   // why a check failed ("" when it passed)

        // The first failing check's name and detail, or "" if all passed.
        const char* FirstFailure() const;

    private:
        friend ValidationReport ValidateTrack(const model::Graph& graph, int laps);
        char m_firstFailure[160];
    };

    const char* ValidationCheckName(ValidationCheck check);

    // Runs every check on `graph` for a race of `laps` laps.
    //
    // FuelPlanFeasible: the race is feasible if the refuelling DP finds a
    // plan either on the plain Balanced route, or on a route that passes
    // one active fuel station on every lap (each station is tried in turn).
    // That is a concrete plan the race engine can execute, not just "some
    // station exists somewhere".
    ValidationReport ValidateTrack(const model::Graph& graph, int laps);
}
