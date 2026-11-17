#include "RefuelDp.h"
#include "Pseudocode.h"
#include "../core/util/MathUtil.h"

namespace algo
{
    namespace
    {
        const int kUnreachable = 0x7fffffff;
    }

    RefuelPlan PlanRefuelling(const RefuelProblem& problem, AlgoStats* stats, Trace* trace)
    {
        RefuelPlan plan;

        int m = problem.legFuel.Size();
        int capacity = problem.capacity;
        if (capacity < 0 || problem.startFuel < 0 || problem.canBuyAt.Size() != m) return plan;

        int startFuel = MathUtil::Min(problem.startFuel, capacity);
        int width = capacity + 1;

        if (m == 0)
        {
            plan.feasible = true;
            plan.finalFuel = startFuel;
            return plan;
        }

        // dp, plus the predecessor needed to reconstruct the purchases.
        int cells = (m + 1) * width;
        int* dp = new int[cells];
        int* fromFuel = new int[cells];
        int* bought = new int[cells];
        for (int i = 0; i < cells; ++i)
        {
            dp[i] = kUnreachable;
            fromFuel[i] = -1;
            bought[i] = 0;
        }
        dp[startFuel] = 0;
        if (trace != nullptr) trace->Record(TraceOp::DpCell, 0, startFuel, 0.0, lines::refuel::kStart);

        for (int j = 0; j < m; ++j)
        {
            int need = problem.legFuel[j];
            int maxBuyHere = problem.canBuyAt[j] ? problem.maxPurchase : 0;

            for (int f = 0; f <= capacity; ++f)
            {
                int here = dp[j * width + f];
                if (here == kUnreachable) continue;

                int maxA = MathUtil::Min(capacity - f, maxBuyHere);
                for (int a = 0; a <= maxA; ++a)
                {
                    if (stats != nullptr) ++stats->dpStates;
                    if (f + a < need) continue;   // cannot make the next leg

                    int nf = f + a - need;
                    int cost = here + ((a > 0) ? problem.stopTimeMs : 0);
                    int idx = (j + 1) * width + nf;

                    // Ties keep the smaller purchase (first found), which
                    // makes the reconstructed plan deterministic.
                    if (cost < dp[idx])
                    {
                        dp[idx] = cost;
                        fromFuel[idx] = f;
                        bought[idx] = a;
                        if (trace != nullptr) trace->Record(TraceOp::DpCell, j + 1, nf, static_cast<double>(cost), lines::refuel::kCell);
                    }
                }
            }
        }

        int bestFuel = -1;
        for (int f = capacity; f >= 0; --f)
        {
            int value = dp[m * width + f];
            if (value == kUnreachable) continue;
            if (bestFuel == -1 || value < dp[m * width + bestFuel]) bestFuel = f;
        }

        if (bestFuel != -1)
        {
            plan.feasible = true;
            plan.totalStopTimeMs = dp[m * width + bestFuel];
            plan.finalFuel = bestFuel;
            plan.purchase.Resize(m);

            int f = bestFuel;
            for (int j = m; j >= 1; --j)
            {
                int idx = j * width + f;
                plan.purchase[j - 1] = bought[idx];
                if (bought[idx] > 0) ++plan.stopCount;
                f = fromFuel[idx];
            }
        }

        delete[] dp;
        delete[] fromFuel;
        delete[] bought;
        return plan;
    }
}
