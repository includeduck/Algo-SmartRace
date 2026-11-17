#include "TestFramework.h"

#include "../src/algo/Kruskal.h"
#include "../src/sim/TrackValidator.h"
#include "../src/sim/TrackGenerator.h"
#include "../src/io/TrackLoader.h"
#include "../src/io/TrackWriter.h"
#include "../src/core/util/MathUtil.h"

#include <cstdio>

using namespace model;

namespace
{
    algo::CandidateEdge Candidate(int id, int from, int to, double weight)
    {
        algo::CandidateEdge e;
        e.id = id;
        e.from = from;
        e.to = to;
        e.weight = weight;
        return e;
    }

    bool LoadCircuit(const char* path, io::TrackData* data)
    {
        int line = 0;
        char message[200];
        return io::LoadTrackFile(path, data, &line, message, sizeof(message)) == io::LoadResult::Ok;
    }
}

TEST(Kruskal_FindsMinimumSpanningTree)
{
    // A square 0-1-2-3 with both diagonals; the MST is the three cheapest
    // non-cycle-forming roads: 0-1 (1), 1-2 (2), 2-3 (3), total 6.
    algo::CandidateEdge edges[6] = {
        Candidate(0, 0, 1, 1.0), Candidate(1, 1, 2, 2.0), Candidate(2, 2, 3, 3.0),
        Candidate(3, 3, 0, 4.0), Candidate(4, 0, 2, 5.0), Candidate(5, 1, 3, 6.0),
    };

    algo::MstResult mst = algo::Kruskal(4, edges, 6);
    CHECK_EQ(mst.acceptedIds.Size(), 3);
    CHECK_EQ(mst.componentCount, 1);
    CHECK(MathUtil::Abs(mst.totalWeight - 6.0) < 1e-9);
    CHECK_EQ(mst.rejectedIds.Size(), 3);
}

TEST(Kruskal_EqualWeights_DeterministicByLowerId)
{
    // A triangle of equal weights: ids 0 and 1 are accepted, 2 closes the cycle.
    algo::CandidateEdge edges[3] = { Candidate(2, 0, 2, 1.0), Candidate(1, 1, 2, 1.0), Candidate(0, 0, 1, 1.0) };
    algo::MstResult mst = algo::Kruskal(3, edges, 3);
    CHECK_EQ(mst.acceptedIds[0], 0);
    CHECK_EQ(mst.acceptedIds[1], 1);
    CHECK_EQ(mst.rejectedIds[0], 2);
}

TEST(Kruskal_DisconnectedGraph_ReturnsForest)
{
    algo::CandidateEdge edges[2] = { Candidate(0, 0, 1, 1.0), Candidate(1, 2, 3, 1.0) };
    algo::MstResult mst = algo::Kruskal(4, edges, 2);
    CHECK_EQ(mst.componentCount, 2);   // a spanning forest, not one tree
    CHECK_EQ(mst.acceptedIds.Size(), 2);
}

TEST(Validator_AcceptsAllThreeHandMadeCircuits)
{
    const char* paths[3] = { "data/circuits/c1_harbor.trk", "data/circuits/c2_twinpeaks.trk", "data/circuits/c3_downtown.trk" };
    for (int i = 0; i < 3; ++i)
    {
        io::TrackData data;
        CHECK(LoadCircuit(paths[i], &data));
        if (data.graph == nullptr) continue;

        sim::ValidationReport report = sim::ValidateTrack(*data.graph, data.laps);
        if (!report.passed) printf("    %s: %s\n", paths[i], report.FirstFailure());
        CHECK(report.passed);
    }
}

TEST(Validator_RejectsBrokenTracks_WithTheRightReason)
{
    io::TrackData data;
    CHECK(LoadCircuit("data/circuits/c1_harbor.trk", &data));
    if (data.graph == nullptr) return;

    // Remove CP7: checkpoint orders are no longer 1..10.
    Vertex& cp7 = data.graph->GetVertexMutable(11);
    CHECK_EQ(cp7.checkpointOrder, 7);
    cp7.checkpointOrder = -1;
    cp7.type = VertexType::Intersection;
    sim::ValidationReport noCp7 = sim::ValidateTrack(*data.graph, data.laps);
    CHECK(!noCp7.passed);
    CHECK(!noCp7.check[static_cast<int>(sim::ValidationCheck::CheckpointsComplete)]);
    cp7.checkpointOrder = 7;
    cp7.type = VertexType::Checkpoint;

    // Make a road shorter than the straight line between its ends: A*'s
    // heuristic would no longer be admissible.
    double original = data.graph->GetEdge(5).distance;
    data.graph->GetEdgeMutable(5).distance = 1.0;
    sim::ValidationReport tooShort = sim::ValidateTrack(*data.graph, data.laps);
    CHECK(!tooShort.check[static_cast<int>(sim::ValidationCheck::DistancesAdmissible)]);
    data.graph->GetEdgeMutable(5).distance = original;

    // Make every road cost 60 fuel units: no plan can finish 3 laps.
    for (int e = 0; e < data.graph->EdgeCount(); ++e) data.graph->GetEdgeMutable(e).fuelCost = 60.0;
    sim::ValidationReport noFuel = sim::ValidateTrack(*data.graph, data.laps);
    CHECK(!noFuel.check[static_cast<int>(sim::ValidationCheck::FuelPlanFeasible)]);
}

TEST(Generator_ProducesValidTracksAcrossSeeds)
{
    int accepted = 0;
    for (unsigned long long seed = 1; seed <= 12; ++seed)
    {
        sim::GeneratorParams params;
        params.seed = seed;

        io::TrackData track;
        model::Graph* candidates = nullptr;
        sim::GenReport report;
        bool ok = sim::GenerateTrack(params, &track, &candidates, &report, nullptr);
        delete candidates;

        if (!ok)
        {
            printf("    seed %llu: every attempt failed; last: %s\n", seed,
                report.attempts[report.attempts.Size() - 1].reason);
            continue;
        }
        ++accepted;

        // The generator's own acceptance means the validator passed; check
        // it independently anyway, plus the brief's minimums directly.
        sim::ValidationReport validation = sim::ValidateTrack(*track.graph, track.laps);
        CHECK(validation.passed);
        CHECK(track.mainRoute.Size() > 10);
        CHECK(track.shortcutPreference.Size() >= 2);
        CHECK_EQ(report.attempts[report.acceptedAttempt].accepted, true);
    }
    CHECK(accepted >= 10);   // nearly every seed should yield a track within the retry budget
}

TEST(Generator_IsDeterministicForASeed)
{
    sim::GeneratorParams params;
    params.seed = 4;

    io::TrackData a;
    io::TrackData b;
    sim::GenReport ra;
    sim::GenReport rb;
    CHECK(sim::GenerateTrack(params, &a, nullptr, &ra, nullptr));
    CHECK(sim::GenerateTrack(params, &b, nullptr, &rb, nullptr));
    if (a.graph == nullptr || b.graph == nullptr) return;

    CHECK_EQ(a.graph->EdgeCount(), b.graph->EdgeCount());
    CHECK_EQ(ra.acceptedAttempt, rb.acceptedAttempt);
    for (int v = 0; v < a.graph->VertexCount(); ++v)
    {
        CHECK(a.graph->GetVertex(v).type == b.graph->GetVertex(v).type);
        CHECK_EQ(a.graph->GetVertex(v).checkpointOrder, b.graph->GetVertex(v).checkpointOrder);
    }
}

TEST(Generator_ImpossibleParams_RejectEveryAttemptWithAReason)
{
    sim::GeneratorParams params;
    params.seed = 9;
    params.vertexCount = 12;   // fewer vertices than roles: no attempt can succeed
    params.maxAttempts = 3;

    io::TrackData track;
    sim::GenReport report;
    CHECK(!sim::GenerateTrack(params, &track, nullptr, &report, nullptr));
    CHECK_EQ(report.attempts.Size(), 3);
    CHECK_EQ(report.acceptedAttempt, -1);
    for (int i = 0; i < report.attempts.Size(); ++i)
    {
        CHECK(!report.attempts[i].accepted);
        CHECK(report.attempts[i].reason[0] != '\0');
    }
}

TEST(TrackWriter_RoundTrip_GeneratedTrackStillValid)
{
    sim::GeneratorParams params;
    params.seed = 2;
    io::TrackData track;
    sim::GenReport report;
    CHECK(sim::GenerateTrack(params, &track, nullptr, &report, nullptr));
    if (track.graph == nullptr) return;

    char message[200];
    const char* path = "build/test_generated_roundtrip.trk";
    CHECK(io::WriteTrackFile(path, track, message, sizeof(message)));

    io::TrackData reloaded;
    CHECK(LoadCircuit(path, &reloaded));
    if (reloaded.graph == nullptr) return;

    CHECK_EQ(reloaded.graph->VertexCount(), track.graph->VertexCount());
    CHECK_EQ(reloaded.graph->EdgeCount(), track.graph->EdgeCount());
    CHECK_EQ(reloaded.mainRoute.Size(), track.mainRoute.Size());
    CHECK_EQ(reloaded.events.Size(), track.events.Size());
    CHECK(sim::ValidateTrack(*reloaded.graph, reloaded.laps).passed);
}
