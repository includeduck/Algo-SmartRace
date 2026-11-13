#include "TestFramework.h"

#include "../src/model/Graph.h"
#include "../src/model/CostModel.h"
#include "../src/algo/Dijkstra.h"
#include "../src/algo/AStar.h"
#include "../src/algo/RoutePlanner.h"
#include "../src/core/util/MathUtil.h"

using namespace model;
using namespace algo;

namespace
{
    // A small diamond: S -> A -> T (cost via distance 1+1=2) and
    // S -> B -> T (cost via distance 1+1=2, same total) plus a longer
    // direct S -> T edge (distance 10) so Dijkstra has a real choice.
    Graph* BuildDiamond(int* s, int* a, int* b, int* t)
    {
        Graph* g = new Graph();
        *s = g->AddVertex(0, 0, VertexType::Start);
        *a = g->AddVertex(1, 1, VertexType::Intersection);
        *b = g->AddVertex(1, -1, VertexType::Intersection);
        *t = g->AddVertex(2, 0, VertexType::Finish);

        g->AddEdge(*s, *a, 1.4142135, 1.0, 1.0, 1, false, false);
        g->AddEdge(*a, *t, 1.4142135, 1.0, 1.0, 1, false, false);
        g->AddEdge(*s, *b, 1.4142135, 1.0, 1.0, 1, false, false);
        g->AddEdge(*b, *t, 1.4142135, 1.0, 1.0, 1, false, false);
        g->AddEdge(*s, *t, 10.0, 1.0, 1.0, 1, false, false);

        return g;
    }
}

TEST(Dijkstra_SourceEqualsTarget)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    CostReferences refs = ComputeReferences(g);
    Weights w = WeightsForPreset(Preset::Balanced);

    PathResult result = Dijkstra(g, a, a, w, refs);
    CHECK(result.found);
    CHECK_EQ(result.vertexPath.Size(), 1);
    CHECK(MathUtil::Abs(result.totalCost) < 1e-9);
}

TEST(Dijkstra_UnreachableTarget)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    g.AddVertex(2, 0, VertexType::Intersection);   // isolated

    CostReferences refs = ComputeReferences(g);
    Weights w = WeightsForPreset(Preset::Balanced);
    PathResult result = Dijkstra(g, a, b, w, refs);
    CHECK(!result.found);
}

TEST(Dijkstra_BlockedEdge_Excluded_RouteRecalculates)
{
    int s, a, b, t;
    Graph* g = BuildDiamond(&s, &a, &b, &t);
    CostReferences refs = ComputeReferences(*g);
    Weights w = WeightsForPreset(Preset::Distance);

    PathResult before = Dijkstra(*g, s, t, w, refs);
    CHECK(before.found);
    CHECK(before.totalCost < 9.0);   // took a diamond side, not the 10.0 direct edge

    // Block one entire diamond side (s->a) -- the other side must still work.
    for (const AdjNode* node = g->Neighbors(s); node != nullptr; node = node->next)
    {
        if (node->neighbor == a) g->SetEdgeStatus(node->edgeId, EdgeStatus::Blocked);
    }

    PathResult after = Dijkstra(*g, s, t, w, refs);
    CHECK(after.found);
    CHECK(after.totalCost < 9.0);   // still finds the b-side, recalculated correctly

    delete g;
}

TEST(Dijkstra_EqualCostPaths_DeterministicAcrossRuns)
{
    int s, a, b, t;
    Graph* g = BuildDiamond(&s, &a, &b, &t);
    CostReferences refs = ComputeReferences(*g);
    Weights w = WeightsForPreset(Preset::Distance);

    PathResult first = Dijkstra(*g, s, t, w, refs);
    PathResult second = Dijkstra(*g, s, t, w, refs);

    CHECK(first.found && second.found);
    CHECK(MathUtil::Abs(first.totalCost - second.totalCost) < 1e-9);
    CHECK_EQ(first.vertexPath.Size(), second.vertexPath.Size());
    for (int i = 0; i < first.vertexPath.Size(); ++i)
    {
        CHECK_EQ(first.vertexPath[i], second.vertexPath[i]);
    }

    delete g;
}

TEST(Dijkstra_ZeroWeightEdge_NoCrash)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(0, 0, VertexType::Finish);   // coincident, zero distance
    g.AddEdge(a, b, 0.0, 0.0, 0.0, 1, false, false);

    CostReferences refs = ComputeReferences(g);
    Weights w = WeightsForPreset(Preset::Balanced);
    PathResult result = Dijkstra(g, a, b, w, refs);
    CHECK(result.found);
    CHECK(result.totalCost >= 0.0);
}

TEST(AStar_MatchesDijkstraCost_OnDiamond)
{
    int s, a, b, t;
    Graph* g = BuildDiamond(&s, &a, &b, &t);
    CostReferences refs = ComputeReferences(*g);
    Weights w = WeightsForPreset(Preset::Distance);

    PathResult dijkstra = Dijkstra(*g, s, t, w, refs);
    double scale = ComputeHeuristicScale(*g, w, refs);
    PathResult astar = AStar(*g, s, t, w, refs, scale);

    CHECK(dijkstra.found && astar.found);
    CHECK(MathUtil::Abs(dijkstra.totalCost - astar.totalCost) < 1e-6);

    delete g;
}

TEST(AStar_MatchesDijkstraCost_OnAllCircuitPresetCombinations)
{
    // A slightly bigger synthetic grid, since the real .trk circuits are
    // loaded in test_integration.cpp (Phase 1's grid is enough to exercise
    // the heuristic across many source/target pairs).
    Graph g;
    const int cols = 5, rows = 5, spacing = 50;
    int ids[rows][cols];
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            ids[r][c] = g.AddVertex(c * spacing, r * spacing, VertexType::Intersection);
        }
    }
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            if (c + 1 < cols)
            {
                double d = spacing;
                g.AddEdge(ids[r][c], ids[r][c + 1], d, d / 40.0, d * 0.05, 2, false, false);
            }
            if (r + 1 < rows)
            {
                double d = spacing;
                g.AddEdge(ids[r][c], ids[r + 1][c], d, d / 40.0, d * 0.05, 3, false, false);
            }
        }
    }

    CostReferences refs = ComputeReferences(g);
    Preset presets[4] = { Preset::Balanced, Preset::Speed, Preset::Economy, Preset::Safety };

    for (int p = 0; p < 4; ++p)
    {
        Weights w = WeightsForPreset(presets[p]);
        double scale = ComputeHeuristicScale(g, w, refs);

        PathResult dijkstra = Dijkstra(g, ids[0][0], ids[rows - 1][cols - 1], w, refs);
        PathResult astar = AStar(g, ids[0][0], ids[rows - 1][cols - 1], w, refs, scale);

        CHECK(dijkstra.found && astar.found);
        CHECK(MathUtil::Abs(dijkstra.totalCost - astar.totalCost) < 1e-6);
    }
}

TEST(AStar_ZeroHeuristicFallback_StillCorrect)
{
    int s, a, b, t;
    Graph* g = BuildDiamond(&s, &a, &b, &t);
    CostReferences refs = ComputeReferences(*g);
    Weights w = WeightsForPreset(Preset::Distance);

    PathResult dijkstra = Dijkstra(*g, s, t, w, refs);
    PathResult astarNoHeuristic = AStar(*g, s, t, w, refs, 0.0);   // h = 0 everywhere

    CHECK(dijkstra.found && astarNoHeuristic.found);
    CHECK(MathUtil::Abs(dijkstra.totalCost - astarNoHeuristic.totalCost) < 1e-6);

    delete g;
}

TEST(RoutePlanner_PreservesCheckpointOrder)
{
    // Start -> CP1 -> CP2 -> Finish, plus a tempting direct Start->Finish
    // edge that must NOT be used (it would skip both checkpoints).
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(1, 0, VertexType::Checkpoint, 1);
    int cp2 = g.AddVertex(2, 0, VertexType::Checkpoint, 2);
    int finish = g.AddVertex(3, 0, VertexType::Finish);

    g.AddEdge(start, cp1, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, cp2, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp2, finish, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(start, finish, 1.0, 1.0, 1.0, 1, false, false);   // shortcut that skips both CPs

    CostReferences refs = ComputeReferences(g);
    Weights w = WeightsForPreset(Preset::Balanced);

    PlanResult plan = PlanLap(g, start, 0, finish, w, refs, RouteAlgo::Dijkstra);
    CHECK(plan.found);
    CHECK_EQ(plan.legs.Size(), 3);   // start->cp1, cp1->cp2, cp2->finish
    CHECK_EQ(plan.vertexPath.Size(), 4);
    CHECK_EQ(plan.vertexPath[0], start);
    CHECK_EQ(plan.vertexPath[1], cp1);
    CHECK_EQ(plan.vertexPath[2], cp2);
    CHECK_EQ(plan.vertexPath[3], finish);
}

TEST(RoutePlanner_BlockedLegReportsWhichOne)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(1, 0, VertexType::Checkpoint, 1);
    int finish = g.AddVertex(2, 0, VertexType::Finish);

    g.AddEdge(start, cp1, 1.0, 1.0, 1.0, 1, false, false);
    int legEdge = g.AddEdge(cp1, finish, 1.0, 1.0, 1.0, 1, false, false);
    g.SetEdgeStatus(legEdge, EdgeStatus::Blocked);

    CostReferences refs = ComputeReferences(g);
    Weights w = WeightsForPreset(Preset::Balanced);

    PlanResult plan = PlanLap(g, start, 0, finish, w, refs, RouteAlgo::Dijkstra);
    CHECK(!plan.found);
    CHECK_EQ(plan.blockedLegIndex, 1);   // the cp1 -> finish leg
}

TEST(RoutePlanner_SkipsAlreadyCompletedCheckpoints)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(1, 0, VertexType::Checkpoint, 1);
    int cp2 = g.AddVertex(2, 0, VertexType::Checkpoint, 2);
    int finish = g.AddVertex(3, 0, VertexType::Finish);

    g.AddEdge(start, cp1, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, cp2, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp2, finish, 1.0, 1.0, 1.0, 1, false, false);

    CostReferences refs = ComputeReferences(g);
    Weights w = WeightsForPreset(Preset::Balanced);

    // Racer already completed CP1; planning from cp1's position onward.
    PlanResult plan = PlanLap(g, cp1, 1, finish, w, refs, RouteAlgo::Dijkstra);
    CHECK(plan.found);
    CHECK_EQ(plan.legs.Size(), 2);   // cp1->cp2, cp2->finish (cp1 itself not re-targeted)
}

TEST(CompareRoutes_ReturnsAtLeastTwoDistinctCandidates)
{
    int s, a, b, t;
    Graph* g = BuildDiamond(&s, &a, &b, &t);
    CostReferences refs = ComputeReferences(*g);
    Weights balanced = WeightsForPreset(Preset::Balanced);

    RouteCompareResult compare = CompareRoutes(*g, s, t, balanced, refs);
    CHECK(compare.candidates.Size() >= 1);

    for (int i = 0; i < compare.candidates.Size(); ++i)
    {
        CHECK(compare.candidates[i]->found);
        CHECK(compare.candidates[i]->totalCost >= 0.0);
    }

    delete g;
}
