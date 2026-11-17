#include "TestFramework.h"

#include "../src/algo/Knapsack.h"
#include "../src/algo/RefuelDp.h"
#include "../src/core/util/Rng.h"

using namespace algo;

namespace
{
    PickupItem Item(int id, int cost, int benefit)
    {
        PickupItem item;
        item.id = id;
        item.vertex = -1;
        item.cost = cost;
        item.benefit = benefit;
        return item;
    }

    bool SelectionIsConsistent(const PickupItem* items, int m, int budget, const KnapsackResult& r)
    {
        int cost = 0;
        int benefit = 0;
        for (int s = 0; s < r.selectedIds.Size(); ++s)
        {
            for (int i = 0; i < m; ++i)
            {
                if (items[i].id == r.selectedIds[s])
                {
                    cost += items[i].cost;
                    benefit += items[i].benefit;
                }
            }
        }
        return cost <= budget && cost == r.totalCost && benefit == r.totalBenefit;
    }

    void SetupProblem(RefuelProblem* p, int startFuel, const int* legs, const bool* stations, int m)
    {
        p->legFuel.Clear();
        p->canBuyAt.Clear();
        p->stopVertex.Clear();
        for (int i = 0; i < m; ++i)
        {
            p->legFuel.Push(legs[i]);
            p->canBuyAt.Push(stations[i]);
            p->stopVertex.Push(i);
        }
        p->stopVertex.Push(m);
        p->startFuel = startFuel;
        p->capacity = 1000;
        p->maxPurchase = 400;
        p->stopTimeMs = 3000;
    }
}

TEST(Knapsack_BriefExample_GreedyWorseThanDp)
{
    // Project.pdf section 5.3: budget 10, A(6,30) B(5,24) C(5,24).
    PickupItem items[3] = { Item(0, 6, 30), Item(1, 5, 24), Item(2, 5, 24) };

    KnapsackResult greedy = GreedyPickup(items, 3, 10);
    KnapsackResult dp = KnapsackDp(items, 3, 10);

    CHECK_EQ(greedy.totalBenefit, 30);   // takes A, then nothing else fits
    CHECK_EQ(greedy.selectedIds.Size(), 1);
    CHECK_EQ(greedy.selectedIds[0], 0);

    CHECK_EQ(dp.totalBenefit, 48);        // B + C
    CHECK_EQ(dp.selectedIds.Size(), 2);
    CHECK_EQ(dp.selectedIds[0], 1);
    CHECK_EQ(dp.selectedIds[1], 2);
}

TEST(Knapsack_TeamCaseG1_GreedyStrictlyWorse)
{
    // costs {4,3,3}, benefits {9,6,6}, budget 6: greedy's best ratio (2.25)
    // eats the budget; DP pairs the two 3-cost items.
    PickupItem items[3] = { Item(0, 4, 9), Item(1, 3, 6), Item(2, 3, 6) };

    CHECK_EQ(GreedyPickup(items, 3, 6).totalBenefit, 9);
    CHECK_EQ(KnapsackDp(items, 3, 6).totalBenefit, 12);
    CHECK_EQ(ExhaustiveKnapsack(items, 3, 6).totalBenefit, 12);
}

TEST(Knapsack_TeamCaseG2_GreedyStrictlyWorse)
{
    // costs {5,4,4,2}, benefits {11,8,8,1}, budget 8: greedy takes 11 then
    // can only fit the 2-cost item (12 total); DP takes 4+4 (16).
    PickupItem items[4] = { Item(0, 5, 11), Item(1, 4, 8), Item(2, 4, 8), Item(3, 2, 1) };

    CHECK_EQ(GreedyPickup(items, 4, 8).totalBenefit, 12);
    CHECK_EQ(KnapsackDp(items, 4, 8).totalBenefit, 16);
    CHECK_EQ(ExhaustiveKnapsack(items, 4, 8).totalBenefit, 16);
}

TEST(Knapsack_DpMatchesExhaustive_On500RandomInstances)
{
    Rng rng(2026);
    for (int trial = 0; trial < 500; ++trial)
    {
        int m = rng.NextInt(1, 12);
        int budget = rng.NextInt(0, 40);
        PickupItem items[12];
        for (int i = 0; i < m; ++i) items[i] = Item(i, rng.NextInt(0, 15), rng.NextInt(0, 30));

        KnapsackResult dp = KnapsackDp(items, m, budget);
        KnapsackResult brute = ExhaustiveKnapsack(items, m, budget);
        KnapsackResult greedy = GreedyPickup(items, m, budget);

        CHECK_EQ(dp.totalBenefit, brute.totalBenefit);
        CHECK_EQ(KnapsackValueOnly(items, m, budget), dp.totalBenefit);
        CHECK(SelectionIsConsistent(items, m, budget, dp));
        CHECK(SelectionIsConsistent(items, m, budget, greedy));
        CHECK(greedy.totalBenefit <= dp.totalBenefit);   // feasible heuristic, never better than optimal
    }
}

TEST(Knapsack_ZeroCostItems_AlwaysTakenWhenUseful)
{
    PickupItem items[3] = { Item(0, 0, 5), Item(1, 0, 0), Item(2, 7, 3) };

    KnapsackResult greedy = GreedyPickup(items, 3, 0);   // nothing with cost fits a 0 budget
    CHECK_EQ(greedy.totalBenefit, 5);
    CHECK_EQ(greedy.selectedIds.Size(), 1);              // the zero-benefit item is not taken
    CHECK_EQ(greedy.selectedIds[0], 0);

    KnapsackResult dp = KnapsackDp(items, 3, 0);
    CHECK_EQ(dp.totalBenefit, 5);
}

TEST(Knapsack_EmptyAndInvalidInputs)
{
    PickupItem items[2] = { Item(0, -1, 50), Item(1, 3, 4) };   // item 0 is invalid (negative cost)

    CHECK_EQ(KnapsackDp(items, 0, 10).totalBenefit, 0);
    CHECK_EQ(KnapsackDp(items, 2, -5).totalBenefit, 0);
    CHECK_EQ(KnapsackDp(items, 2, 10).totalBenefit, 4);          // invalid item ignored
    CHECK_EQ(GreedyPickup(items, 2, 10).totalBenefit, 4);
    CHECK_EQ(ExhaustiveKnapsack(items, 2, 10).totalBenefit, 4);
}

TEST(Knapsack_CountsDpStates)
{
    PickupItem items[3] = { Item(0, 6, 30), Item(1, 5, 24), Item(2, 5, 24) };
    AlgoStats stats;
    KnapsackDp(items, 3, 10, &stats);
    CHECK_EQ(stats.dpStates, 3 * 11);   // m * (B + 1) cells filled
}

TEST(Refuel_FullTank_RouteFitsWithoutStopping)
{
    int legs[3] = { 200, 300, 100 };
    bool stations[3] = { false, true, true };
    RefuelProblem p;
    SetupProblem(&p, 1000, legs, stations, 3);

    RefuelPlan plan = PlanRefuelling(p);
    CHECK(plan.feasible);
    CHECK_EQ(plan.stopCount, 0);
    CHECK_EQ(plan.totalStopTimeMs, 0);
    CHECK_EQ(plan.finalFuel, 400);
}

TEST(Refuel_InsufficientFuel_NoStation_Infeasible)
{
    int legs[2] = { 600, 600 };
    bool stations[2] = { false, false };
    RefuelProblem p;
    SetupProblem(&p, 1000, legs, stations, 2);

    CHECK(!PlanRefuelling(p).feasible);
}

TEST(Refuel_UnavailableStation_CannotBuy)
{
    int legs[2] = { 600, 600 };
    bool stations[2] = { false, false };   // stop 1 IS a station on the route, but deactivated
    RefuelProblem p;
    SetupProblem(&p, 1000, legs, stations, 2);
    CHECK(!PlanRefuelling(p).feasible);

    p.canBuyAt[1] = true;                  // reactivated: now one stop fixes it
    RefuelPlan plan = PlanRefuelling(p);
    CHECK(plan.feasible);
    CHECK_EQ(plan.stopCount, 1);
    CHECK(plan.purchase[1] >= 200);        // must cover the 200-tenth shortfall
    CHECK_EQ(plan.totalStopTimeMs, 3000);
}

TEST(Refuel_CapacityAndPurchaseLimit_ForceTwoStops)
{
    // Start nearly empty; the remaining legs need 700 more than we hold,
    // but one stop may buy at most 400 -- so two stops are unavoidable.
    int legs[3] = { 100, 400, 400 };
    bool stations[3] = { false, true, true };
    RefuelProblem p;
    SetupProblem(&p, 200, legs, stations, 3);

    RefuelPlan plan = PlanRefuelling(p);
    CHECK(plan.feasible);
    CHECK_EQ(plan.stopCount, 2);
    CHECK_EQ(plan.totalStopTimeMs, 6000);
    for (int j = 0; j < 3; ++j) CHECK(plan.purchase[j] <= 400);
}

TEST(Refuel_PrefersFewestStops)
{
    // Either station alone can cover the shortfall: the DP must pick one
    // stop (3s), not two (6s).
    int legs[3] = { 300, 300, 300 };
    bool stations[3] = { false, true, true };
    RefuelProblem p;
    SetupProblem(&p, 700, legs, stations, 3);

    RefuelPlan plan = PlanRefuelling(p);
    CHECK(plan.feasible);
    CHECK_EQ(plan.stopCount, 1);
    CHECK_EQ(plan.totalStopTimeMs, 3000);
}

TEST(Refuel_FirstLegTooLong_InfeasibleEvenWithStationsLater)
{
    int legs[2] = { 1200, 10 };   // first leg alone exceeds a full tank
    bool stations[2] = { true, true };
    RefuelProblem p;
    SetupProblem(&p, 1000, legs, stations, 2);

    // The racer may buy at stop 0 only if already standing at a station
    // (canBuyAt[0]) -- even topping up to capacity cannot cover 1200.
    CHECK(!PlanRefuelling(p).feasible);
}
