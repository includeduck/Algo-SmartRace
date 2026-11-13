#include "TestFramework.h"

#include "../src/model/Graph.h"
#include "../src/model/CostModel.h"
#include "../src/core/util/MathUtil.h"

using namespace model;

TEST(Graph_AddVertexAndEdge_Undirected)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(10, 0, VertexType::Checkpoint, 1);
    int e = g.AddEdge(a, b, 10.0, 1.0, 1.0, 1, false, false);

    CHECK_EQ(g.VertexCount(), 2);
    CHECK_EQ(g.EdgeCount(), 1);

    const AdjNode* fromA = g.Neighbors(a);
    CHECK(fromA != nullptr);
    CHECK_EQ(fromA->neighbor, b);
    CHECK_EQ(fromA->edgeId, e);

    const AdjNode* fromB = g.Neighbors(b);
    CHECK(fromB != nullptr);
    CHECK_EQ(fromB->neighbor, a);
}

TEST(Graph_DirectedEdge_OneWayOnly)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Intersection);
    int b = g.AddVertex(5, 0, VertexType::Intersection);
    g.AddEdge(a, b, 5.0, 1.0, 1.0, 1, false, true);   // directed a -> b

    CHECK(g.Neighbors(a) != nullptr);
    CHECK(g.Neighbors(b) == nullptr);        // no forward edge back to a
    CHECK(g.Incoming(b) != nullptr);          // but b knows a can reach it
}

TEST(Graph_SetEdgeStatus_NoRebuild)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, false);

    CHECK(g.IsTraversable(e));
    g.SetEdgeStatus(e, EdgeStatus::Blocked);
    CHECK(!g.IsTraversable(e));
    CHECK_EQ(g.EdgeCount(), 1);   // still the same edge, not rebuilt
}

TEST(Graph_Clone_IsIndependentDeepCopy)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, false);

    Graph* clone = g.Clone();
    clone->SetEdgeStatus(e, EdgeStatus::Blocked);

    CHECK(g.IsTraversable(e));          // original untouched
    CHECK(!clone->IsTraversable(e));     // clone changed independently

    delete clone;
}

TEST(CostModel_AllWeightsNonNegative_CostNonNegative)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(3, 4, VertexType::Finish);   // 3-4-5 triangle, distance 5
    int e = g.AddEdge(a, b, 5.0, 1.0, 2.0, 3, false, false);

    CostReferences refs = ComputeReferences(g);

    Preset presets[5] = { Preset::Balanced, Preset::Speed, Preset::Economy, Preset::Safety, Preset::Distance };
    for (int i = 0; i < 5; ++i)
    {
        Weights w = WeightsForPreset(presets[i]);
        CHECK(w.wd >= 0.0);
        CHECK(w.wt >= 0.0);
        CHECK(w.wf >= 0.0);
        CHECK(w.wr >= 0.0);
        CHECK(w.wo >= 0.0);

        double cost = EdgeCost(g, e, w, refs);
        CHECK(cost >= 0.0);
    }
}

TEST(CostModel_DistancePresetEqualsRawDistance)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(10, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 10.0, 2.0, 1.0, 1, false, false);

    CostReferences refs = ComputeReferences(g);
    Weights w = WeightsForPreset(Preset::Distance);
    double cost = EdgeCost(g, e, w, refs);

    // One edge in the whole graph -> distanceRef == its own distance, so
    // the normalized D term is exactly 1.0 and Distance-preset cost == wd.
    CHECK(MathUtil::Abs(cost - w.wd) < 1e-9);
}
