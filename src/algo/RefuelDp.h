#pragma once

#include "../core/ds/DynArray.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    // Input to the refuelling DP: a FIXED route already chosen by the
    // planner, cut into stops. Stop 0 is where the racer is now; stops
    // 1..m-1 are the fuel stations the route passes; stop m is the
    // destination. All fuel quantities are integer TENTHS of a fuel unit
    // (Project.pdf section 4.3: the DP needs discrete fuel states; tenths
    // keep 0.1-unit precision at the price of a fuel dimension 10x larger,
    // C = 1000 states instead of 100).
    struct RefuelProblem
    {
        DynArray<int> legFuel;       // legFuel[j]: fuel to drive from stop j to stop j+1 (size m)
        DynArray<bool> canBuyAt;      // canBuyAt[j]: stop j is an ACTIVE station (size m)
        DynArray<int> stopVertex;       // graph vertex of stop j, for display (size m + 1)
        int startFuel;
        int capacity;
        int maxPurchase;                  // per stop (40.0 units = 400 by the rules)
        int stopTimeMs;                     // fixed cost of any positive purchase
    };

    struct RefuelPlan
    {
        RefuelPlan()
            : feasible(false), totalStopTimeMs(0), finalFuel(0), stopCount(0)
        {
        }

        bool feasible;
        int totalStopTimeMs;
        int finalFuel;
        int stopCount;
        DynArray<int> purchase;   // purchase[j]: tenths to buy at stop j (size m; 0 = drive through)
    };

    // Problem B of section 5.3:
    //   dp[j][f] = least total stop time on ARRIVAL at stop j holding f fuel
    //   dp[0][startFuel] = 0, every other state unreachable (+infinity)
    //   from a reachable (j, f), buy a in [0, min(C - f, maxPurchase)] if
    //   stop j is an active station (a = 0 otherwise); if f + a >= legFuel[j]
    //   move to dp[j+1][f + a - legFuel[j]] at cost dp[j][f] + stopTime(a),
    //   where stopTime(0) = 0 and stopTime(a > 0) = stopTimeMs.
    // The route is feasible iff some dp[m][f] is reachable. Among the plans
    // with the least stop time the one arriving with the MOST fuel is
    // returned (deterministic, and leaves the most margin for later laps).
    // Unreachable states are simply never expanded. O(m * C * maxPurchase)
    // = O(m C^2) in the straightforward form used here.
    RefuelPlan PlanRefuelling(const RefuelProblem& problem, AlgoStats* stats = nullptr, Trace* trace = nullptr);
}
