#include "TestFramework.h"

#include "../src/sim/Strategies.h"
#include "../src/model/Graph.h"
#include "../src/model/CostModel.h"

using namespace model;
using namespace sim;

namespace
{
    // A 4-vertex diamond, same shape as the brief's own S-T example but
    // used here as a one-lap "ring": Start(0) -> CP1(1) -> CP2(2) ->
    // Finish(3) -> back to Start, with a single documented detour around
    // the Start->CP1 edge via an intersection vertex.
    model::Racer MakeRacer(int currentVertex, int nextCheckpoint)
    {
        model::Racer r;
        r.id = 0;
        r.currentVertex = currentVertex;
        r.nextCheckpoint = nextCheckpoint;
        r.isHuman = false;
        return r;
    }
}

TEST(FixedRouteStrategy_FollowsMainRouteWhenOpen)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(10, 0, VertexType::Checkpoint, 1);
    int cp2 = g.AddVertex(10, 10, VertexType::Checkpoint, 2);
    int finish = g.AddVertex(0, 10, VertexType::Finish);

    int e0 = g.AddEdge(start, cp1, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, cp2, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp2, finish, 10.0, 1.0, 1.0, 1, false, false);
    (void)e0;

    DynArray<int> mainRoute;
    mainRoute.Push(start);
    mainRoute.Push(cp1);
    mainRoute.Push(cp2);
    mainRoute.Push(finish);

    DynArray<io::DetourRule*> detours;
    DynArray<int> shortcuts;

    StrategyContext context;
    context.mainRoute = &mainRoute;
    context.detours = &detours;
    context.shortcutPreference = &shortcuts;
    context.finishVertex = finish;
    context.costReferences = ComputeReferences(g);

    FixedRouteStrategy strategy;
    model::Racer racer = MakeRacer(start, 1);
    StrategyPlan plan = strategy.PlanRoute(g, racer, context);

    CHECK(plan.found);
    CHECK_EQ(plan.vertexPath.Size(), 4);
    CHECK_EQ(plan.vertexPath[0], start);
    CHECK_EQ(plan.vertexPath[1], cp1);
    CHECK_EQ(plan.vertexPath[2], cp2);
    CHECK_EQ(plan.vertexPath[3], finish);
    CHECK_EQ(plan.edgePath.Size(), 3);
}

TEST(FixedRouteStrategy_SplicesDocumentedDetour_WhenBlocked)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(10, 0, VertexType::Checkpoint, 1);
    int cp2 = g.AddVertex(10, 10, VertexType::Checkpoint, 2);
    int finish = g.AddVertex(0, 10, VertexType::Finish);
    int detourVertex = g.AddVertex(5, -5, VertexType::Intersection);

    int blockedEdge = g.AddEdge(start, cp1, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, cp2, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp2, finish, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(start, detourVertex, 7.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(detourVertex, cp1, 7.0, 1.0, 1.0, 1, false, false);

    g.SetEdgeStatus(blockedEdge, EdgeStatus::Blocked);

    DynArray<int> mainRoute;
    mainRoute.Push(start);
    mainRoute.Push(cp1);
    mainRoute.Push(cp2);
    mainRoute.Push(finish);

    io::DetourRule* rule = new io::DetourRule();
    rule->blockedEdgeId = blockedEdge;
    rule->viaVertices.Push(start);          // convention: starts at the blocked edge's `from`...
    rule->viaVertices.Push(detourVertex);     // ...the actual detour hop...
    rule->viaVertices.Push(cp1);                // ...and ends at the blocked edge's `to`.
    DynArray<io::DetourRule*> detours;
    detours.Push(rule);

    DynArray<int> shortcuts;

    StrategyContext context;
    context.mainRoute = &mainRoute;
    context.detours = &detours;
    context.shortcutPreference = &shortcuts;
    context.finishVertex = finish;
    context.costReferences = ComputeReferences(g);

    FixedRouteStrategy strategy;
    model::Racer racer = MakeRacer(start, 1);
    StrategyPlan plan = strategy.PlanRoute(g, racer, context);

    CHECK(plan.found);
    // Expect start -> detourVertex -> cp1 -> cp2 -> finish: the blocked
    // edge is bypassed, and NEITHER endpoint is duplicated (the bug this
    // test guards against spliced in a redundant extra `start` or `cp1`,
    // producing a vertex pair with no connecting edge and silently
    // failing the whole plan).
    CHECK_EQ(plan.vertexPath.Size(), 5);
    CHECK_EQ(plan.vertexPath[0], start);
    CHECK_EQ(plan.vertexPath[1], detourVertex);
    CHECK_EQ(plan.vertexPath[2], cp1);
    CHECK_EQ(plan.vertexPath[3], cp2);
    CHECK_EQ(plan.vertexPath[4], finish);
    CHECK_EQ(plan.edgePath.Size(), 4);

    delete rule;
}

TEST(FixedRouteStrategy_NoMatchingDetour_FailsCleanly)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(10, 0, VertexType::Checkpoint, 1);
    int finish = g.AddVertex(10, 10, VertexType::Finish);

    int blockedEdge = g.AddEdge(start, cp1, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, finish, 10.0, 1.0, 1.0, 1, false, false);
    g.SetEdgeStatus(blockedEdge, EdgeStatus::Blocked);

    DynArray<int> mainRoute;
    mainRoute.Push(start);
    mainRoute.Push(cp1);
    mainRoute.Push(finish);

    DynArray<io::DetourRule*> detours;   // no detour documented for this edge
    DynArray<int> shortcuts;

    StrategyContext context;
    context.mainRoute = &mainRoute;
    context.detours = &detours;
    context.shortcutPreference = &shortcuts;
    context.finishVertex = finish;
    context.costReferences = ComputeReferences(g);

    FixedRouteStrategy strategy;
    model::Racer racer = MakeRacer(start, 1);
    StrategyPlan plan = strategy.PlanRoute(g, racer, context);

    CHECK(!plan.found);   // no crash, just a clean failure -- recovery policy applies
}

TEST(ShortcutLoverStrategy_SubstitutesOpenShortcut)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(10, 0, VertexType::Checkpoint, 1);
    int cp2 = g.AddVertex(20, 0, VertexType::Checkpoint, 2);
    int finish = g.AddVertex(30, 0, VertexType::Finish);

    g.AddEdge(start, cp1, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, cp2, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp2, finish, 10.0, 1.0, 1.0, 1, false, false);
    int shortcutEdge = g.AddEdge(cp1, finish, 5.0, 1.0, 1.0, 5, true, false);   // skips cp2's position in the route

    DynArray<int> mainRoute;
    mainRoute.Push(start);
    mainRoute.Push(cp1);
    mainRoute.Push(cp2);
    mainRoute.Push(finish);

    DynArray<io::DetourRule*> detours;
    DynArray<int> shortcuts;
    shortcuts.Push(shortcutEdge);

    StrategyContext context;
    context.mainRoute = &mainRoute;
    context.detours = &detours;
    context.shortcutPreference = &shortcuts;
    context.finishVertex = finish;
    context.costReferences = ComputeReferences(g);

    ShortcutLoverStrategy strategy;
    model::Racer racer = MakeRacer(cp1, 2);
    StrategyPlan plan = strategy.PlanRoute(g, racer, context);

    CHECK(plan.found);
    CHECK_EQ(plan.vertexPath.Size(), 2);   // cp1 -> finish directly via the shortcut
    CHECK_EQ(plan.vertexPath[0], cp1);
    CHECK_EQ(plan.vertexPath[1], finish);
    CHECK_EQ(plan.edgePath.Size(), 1);
    CHECK_EQ(plan.edgePath[0], shortcutEdge);
}

TEST(ShortcutLoverStrategy_FallsBackWhenShortcutClosed)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(10, 0, VertexType::Checkpoint, 1);
    int cp2 = g.AddVertex(20, 0, VertexType::Checkpoint, 2);
    int finish = g.AddVertex(30, 0, VertexType::Finish);

    g.AddEdge(start, cp1, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, cp2, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp2, finish, 10.0, 1.0, 1.0, 1, false, false);
    int shortcutEdge = g.AddEdge(cp1, finish, 5.0, 1.0, 1.0, 5, true, false, EdgeStatus::Blocked);

    DynArray<int> mainRoute;
    mainRoute.Push(start);
    mainRoute.Push(cp1);
    mainRoute.Push(cp2);
    mainRoute.Push(finish);

    DynArray<io::DetourRule*> detours;
    DynArray<int> shortcuts;
    shortcuts.Push(shortcutEdge);

    StrategyContext context;
    context.mainRoute = &mainRoute;
    context.detours = &detours;
    context.shortcutPreference = &shortcuts;
    context.finishVertex = finish;
    context.costReferences = ComputeReferences(g);

    ShortcutLoverStrategy strategy;
    model::Racer racer = MakeRacer(cp1, 2);
    StrategyPlan plan = strategy.PlanRoute(g, racer, context);

    CHECK(plan.found);
    CHECK_EQ(plan.vertexPath.Size(), 3);   // cp1 -> cp2 -> finish, the main route
    CHECK_EQ(plan.vertexPath[1], cp2);
}

TEST(ShortestPathStrategy_MatchesDijkstraDistance)
{
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(10, 0, VertexType::Checkpoint, 1);
    int finish = g.AddVertex(20, 0, VertexType::Finish);
    g.AddEdge(start, cp1, 10.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, finish, 10.0, 1.0, 1.0, 1, false, false);

    DynArray<int> mainRoute;
    DynArray<io::DetourRule*> detours;
    DynArray<int> shortcuts;

    StrategyContext context;
    context.mainRoute = &mainRoute;
    context.detours = &detours;
    context.shortcutPreference = &shortcuts;
    context.finishVertex = finish;
    context.costReferences = ComputeReferences(g);

    ShortestPathStrategy strategy;
    model::Racer racer = MakeRacer(start, 1);
    StrategyPlan plan = strategy.PlanRoute(g, racer, context);

    CHECK(plan.found);
    CHECK_EQ(plan.vertexPath.Size(), 3);
    CHECK_EQ(plan.edgePath.Size(), 2);
}
