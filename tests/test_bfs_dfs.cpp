#include "TestFramework.h"

#include "../src/model/Graph.h"
#include "../src/algo/Bfs.h"
#include "../src/algo/Dfs.h"

using namespace model;
using namespace algo;

namespace
{
    // S --10-- T  and  S --2-- A --2-- B --2-- T  (three edges, length 2 each)
    // From Project.pdf section 5.1's worked example: BFS must pick the
    // direct S-T edge (1 edge) over the 3-edge path, even though the
    // 3-edge path has less total distance (6 < 10).
    Graph* BuildBriefExample(int* outS, int* outT)
    {
        Graph* g = new Graph();
        int s = g->AddVertex(0, 0, VertexType::Start);
        int t = g->AddVertex(10, 0, VertexType::Finish);
        int a = g->AddVertex(0, 2, VertexType::Intersection);
        int b = g->AddVertex(5, 4, VertexType::Intersection);

        g->AddEdge(s, t, 10.0, 1.0, 1.0, 1, false, false);
        g->AddEdge(s, a, 2.0, 1.0, 1.0, 1, false, false);
        g->AddEdge(a, b, 2.0, 1.0, 1.0, 1, false, false);
        g->AddEdge(b, t, 2.0, 1.0, 1.0, 1, false, false);

        *outS = s;
        *outT = t;
        return g;
    }
}

TEST(Bfs_MinimumEdgeCount_NotMinimumDistance)
{
    int s, t;
    Graph* g = BuildBriefExample(&s, &t);

    BfsResult result = Bfs(*g, s, t);
    CHECK(result.found);
    CHECK_EQ(result.path.Size(), 2);        // S -> T directly: 1 edge, 2 vertices
    CHECK_EQ(result.distEdges[t], 1);

    delete g;
}

TEST(Bfs_UnreachableTarget_NoCrash)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    g.AddVertex(2, 0, VertexType::Intersection);   // isolated, no edges at all

    BfsResult result = Bfs(g, a, b);
    CHECK(!result.found);
    CHECK_EQ(result.path.Size(), 0);

    BfsResult fullReach = Bfs(g, a, -1);
    CHECK(fullReach.reachable[a]);
    CHECK(!fullReach.reachable[b]);
}

TEST(Bfs_SingleVertexGraph)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);

    BfsResult result = Bfs(g, a, a);
    CHECK(result.found);
    CHECK_EQ(result.path.Size(), 1);
    CHECK_EQ(result.distEdges[a], 0);
}

TEST(Bfs_BlockedEdgeExcluded)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, false);

    g.SetEdgeStatus(e, EdgeStatus::Blocked);
    BfsResult result = Bfs(g, a, b);
    CHECK(!result.found);
}

TEST(Dfs_DetectsCycle_UndirectedTriangle)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Intersection);
    int b = g.AddVertex(1, 0, VertexType::Intersection);
    int c = g.AddVertex(0, 1, VertexType::Intersection);
    g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(b, c, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(c, a, 1.0, 1.0, 1.0, 1, false, false);

    DfsCycleResult result = DfsCycles(g);
    CHECK(result.hasCycle);
    CHECK(result.witnessCycle.Size() >= 3);
}

TEST(Dfs_NoCycle_OnATree)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Intersection);
    int c = g.AddVertex(2, 0, VertexType::Finish);
    g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(b, c, 1.0, 1.0, 1.0, 1, false, false);

    DfsCycleResult result = DfsCycles(g);
    CHECK(!result.hasCycle);
}

TEST(Dfs_DirectedCycle_Detected)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Intersection);
    int b = g.AddVertex(1, 0, VertexType::Intersection);
    int c = g.AddVertex(2, 0, VertexType::Intersection);
    g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, true);
    g.AddEdge(b, c, 1.0, 1.0, 1.0, 1, false, true);
    g.AddEdge(c, a, 1.0, 1.0, 1.0, 1, false, true);

    DfsCycleResult result = DfsCycles(g);
    CHECK(result.hasCycle);
}

TEST(Dfs_DirectedAcyclic_NoCycle)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Intersection);
    int b = g.AddVertex(1, 0, VertexType::Intersection);
    int c = g.AddVertex(2, 0, VertexType::Intersection);
    g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, true);
    g.AddEdge(b, c, 1.0, 1.0, 1.0, 1, false, true);
    g.AddEdge(a, c, 1.0, 1.0, 1.0, 1, false, true);   // extra forward edge, still a DAG

    DfsCycleResult result = DfsCycles(g);
    CHECK(!result.hasCycle);
}

TEST(Dfs_Components_DisconnectedGraph)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, false);
    g.AddVertex(5, 5, VertexType::Intersection);   // isolated, separate component

    DfsComponentsResult result = DfsComponents(g);
    CHECK_EQ(result.componentCount, 2);
    CHECK_EQ(result.componentId[a], result.componentId[b]);
}

TEST(Dfs_Bridges_SingleConnectingEdgeIsABridge)
{
    // Two triangles joined by exactly one edge -- that edge is the only bridge.
    Graph g;
    int a0 = g.AddVertex(0, 0, VertexType::Intersection);
    int a1 = g.AddVertex(1, 0, VertexType::Intersection);
    int a2 = g.AddVertex(0, 1, VertexType::Intersection);
    int b0 = g.AddVertex(10, 0, VertexType::Intersection);
    int b1 = g.AddVertex(11, 0, VertexType::Intersection);
    int b2 = g.AddVertex(10, 1, VertexType::Intersection);

    g.AddEdge(a0, a1, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(a1, a2, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(a2, a0, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(b0, b1, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(b1, b2, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(b2, b0, 1.0, 1.0, 1.0, 1, false, false);
    int bridgeEdge = g.AddEdge(a1, b0, 5.0, 1.0, 1.0, 1, false, false);

    DfsBridgesResult result = DfsBridges(g);
    CHECK_EQ(result.bridgeEdgeIds.Size(), 1);
    CHECK_EQ(result.bridgeEdgeIds[0], bridgeEdge);
}

TEST(Dfs_DeadEnd_SpurOffTheMainLoop)
{
    // A triangle (Start-CP1-Finish) with a dead-end spur off CP1 that leads
    // nowhere mandatory.
    Graph g;
    int start = g.AddVertex(0, 0, VertexType::Start);
    int cp1 = g.AddVertex(1, 0, VertexType::Checkpoint, 1);
    int finish = g.AddVertex(0, 1, VertexType::Finish);
    int spur = g.AddVertex(2, 0, VertexType::DeadEnd);

    g.AddEdge(start, cp1, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, finish, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(finish, start, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(cp1, spur, 1.0, 1.0, 1.0, 1, false, false);

    DfsDeadEndsResult result = DfsDeadEnds(g, finish);

    bool spurFlagged = false;
    for (int i = 0; i < result.deadEndVertices.Size(); ++i)
    {
        if (result.deadEndVertices[i] == spur) spurFlagged = true;
    }
    CHECK(spurFlagged);

    for (int i = 0; i < result.deadEndVertices.Size(); ++i)
    {
        CHECK(result.deadEndVertices[i] != start);
        CHECK(result.deadEndVertices[i] != cp1);
        CHECK(result.deadEndVertices[i] != finish);
    }
}

TEST(Dfs_DeadEnd_UnreachableVertex_ViaOneWayRoad)
{
    // A -> B directed, B -> Finish directed, but nothing leads back from a
    // side spur C that A points to: C cannot reach Finish at all.
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Checkpoint, 1);
    int finish = g.AddVertex(2, 0, VertexType::Finish);
    int c = g.AddVertex(0, 1, VertexType::Intersection);

    g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, true);
    g.AddEdge(b, finish, 1.0, 1.0, 1.0, 1, false, true);
    g.AddEdge(a, c, 1.0, 1.0, 1.0, 1, false, true);   // one-way into a trap

    DfsDeadEndsResult result = DfsDeadEnds(g, finish);

    bool cFlagged = false;
    for (int i = 0; i < result.deadEndVertices.Size(); ++i)
    {
        if (result.deadEndVertices[i] == c) cFlagged = true;
    }
    CHECK(cFlagged);
}

TEST(Dfs_AlternativeRoutes_BoundedAndDiffersFromMain)
{
    // Start -> A -> Finish (main route) and Start -> B -> Finish (alternative).
    Graph g;
    int s = g.AddVertex(0, 0, VertexType::Start);
    int a = g.AddVertex(1, 0, VertexType::Intersection);
    int b = g.AddVertex(1, 1, VertexType::Intersection);
    int t = g.AddVertex(2, 0, VertexType::Finish);

    g.AddEdge(s, a, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(a, t, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(s, b, 1.0, 1.0, 1.0, 1, false, false);
    g.AddEdge(b, t, 1.0, 1.0, 1.0, 1, false, false);

    DynArray<int> mainRoute;
    mainRoute.Push(s);
    mainRoute.Push(a);
    mainRoute.Push(t);

    AlternativeRoutesResult result = DfsAlternativeRoutes(g, s, t, mainRoute, 8, 10);
    CHECK_EQ(result.routes.Size(), 1);
    CHECK(!result.truncated);
    CHECK_EQ((*result.routes[0])[1], b);   // the alternative goes through B, not A
}
