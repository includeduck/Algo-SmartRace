#include "TestFramework.h"

#include "../src/algo/Bfs.h"
#include "../src/algo/Knapsack.h"
#include "../src/algo/Sorting.h"
#include "../src/algo/Trace.h"
#include "../src/algo/TraceCursor.h"
#include "../src/algo/Pseudocode.h"
#include "../src/io/TrackLoader.h"
#include "../src/api/SmartRaceApi.h"

namespace
{
    struct IntOrderLab
    {
        static int Compare(const int& a, const int& b)
        {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        }

        static int Key(const int& v)
        {
            return v;
        }
    };

    const int kSortInput[10] = { 42, 7, 93, 7, 0, 58, 21, 99, 3, 64 };
    const int kSortSorted[10] = { 0, 3, 7, 7, 21, 42, 58, 64, 93, 99 };

    // Runs one sort with a trace and checks the cursor's replayed array
    // ends up sorted -- i.e. the trace really describes what the sort did.
    template <int Which>
    bool SortReplayEndsSorted()
    {
        int data[10];
        for (int i = 0; i < 10; ++i)
        {
            data[i] = kSortInput[i];
        }

        algo::Trace trace;
        if (Which == 0) algo::MergeSort<int, IntOrderLab>(data, 10, nullptr, &trace);
        if (Which == 1) algo::QuickSort<int, IntOrderLab>(data, 10, nullptr, &trace);
        if (Which == 2) algo::HeapSort<int, IntOrderLab>(data, 10, nullptr, &trace);

        algo::TraceCursor cursor;
        cursor.Reset(&trace, algo::FrameKind::Array, 0, 0, kSortInput, 10, 0, 0);
        cursor.SeekTo(trace.StepCount() - 1);
        for (int i = 0; i < 10; ++i)
        {
            if (cursor.ArrayValues()[i] != kSortSorted[i]) return false;
        }
        return trace.StepCount() > 0;
    }

    const LabEventView* FindLabEvent(const ArrayView<LabEventView>& events, int id)
    {
        for (int i = 0; i < events.count; ++i)
        {
            if (events.data[i].id == id) return &events.data[i];
        }
        return nullptr;
    }
}

TEST(TraceCursor_BfsReplay_MatchesResult)
{
    io::TrackData data;
    int line = 0;
    char message[200];
    CHECK(io::LoadTrackFile("data/circuits/c1_harbor.trk", &data, &line, message, sizeof(message)) == io::LoadResult::Ok);

    const model::Graph& g = *data.graph;
    algo::Trace trace;
    algo::BfsResult r = algo::Bfs(g, 0, -1, nullptr, &trace);

    algo::TraceCursor cursor;
    cursor.Reset(&trace, algo::FrameKind::Graph, g.VertexCount(), g.EdgeCount(), nullptr, 0, 0, 0);
    cursor.SeekTo(trace.StepCount() - 1);
    for (int v = 0; v < g.VertexCount(); ++v)
    {
        if (r.distEdges[v] < 0)
        {
            CHECK_EQ(cursor.VertexState()[v], 0);
            continue;
        }
        CHECK_EQ(cursor.VertexState()[v], 2);
        CHECK_EQ(static_cast<int>(cursor.VertexValue()[v]), r.distEdges[v]);
        CHECK_EQ(cursor.VertexParent()[v], r.parent[v]);
    }

    // Before any step nothing is marked.
    cursor.SeekTo(-1);
    CHECK_EQ(cursor.CurrentStep(), -1);
    CHECK_EQ(cursor.VertexState()[0], 0);
    CHECK_EQ(cursor.Counters().visited, 0);
}

TEST(TraceCursor_SeekBackwardEqualsForward)
{
    io::TrackData data;
    int line = 0;
    char message[200];
    CHECK(io::LoadTrackFile("data/circuits/c2_twinpeaks.trk", &data, &line, message, sizeof(message)) == io::LoadResult::Ok);

    algo::Trace trace;
    algo::Bfs(*data.graph, 0, -1, nullptr, &trace);
    int mid = trace.StepCount() / 2;

    algo::TraceCursor a;
    a.Reset(&trace, algo::FrameKind::Graph, data.graph->VertexCount(), data.graph->EdgeCount(), nullptr, 0, 0, 0);
    a.SeekTo(mid);

    algo::TraceCursor b;
    b.Reset(&trace, algo::FrameKind::Graph, data.graph->VertexCount(), data.graph->EdgeCount(), nullptr, 0, 0, 0);
    b.SeekTo(trace.StepCount() - 1);
    b.SeekTo(mid);

    CHECK_EQ(a.Counters().visited, b.Counters().visited);
    CHECK_EQ(a.Counters().examined, b.Counters().examined);
    for (int v = 0; v < data.graph->VertexCount(); ++v)
    {
        CHECK_EQ(a.VertexState()[v], b.VertexState()[v]);
    }
    for (int e = 0; e < data.graph->EdgeCount(); ++e)
    {
        CHECK_EQ(a.EdgeState()[e], b.EdgeState()[e]);
    }
}

TEST(TraceCursor_SortReplays_EndSorted)
{
    CHECK(SortReplayEndsSorted<0>());
    CHECK(SortReplayEndsSorted<1>());
    CHECK(SortReplayEndsSorted<2>());
}

TEST(TraceCursor_KnapsackTable_MatchesDp)
{
    algo::PickupItem items[3] = { { 0, -1, 6, 30 }, { 1, -1, 5, 24 }, { 2, -1, 5, 24 } };
    algo::Trace trace;
    algo::KnapsackResult r = algo::KnapsackDp(items, 3, 10, nullptr, &trace);
    CHECK_EQ(r.totalBenefit, 48);

    algo::TraceCursor cursor;
    cursor.Reset(&trace, algo::FrameKind::Table, 0, 0, nullptr, 0, 4, 11);
    cursor.SeekTo(trace.StepCount() - 1);
    CHECK_EQ(cursor.Table()[3 * 11 + 10], 48);
    for (int i = 0; i < cursor.Table().Size(); ++i)
    {
        CHECK(cursor.Table()[i] >= 0);   // every cell was filled
    }
    CHECK_EQ(cursor.Selected().Size(), r.selectedIds.Size());
}

TEST(Pseudocode_EveryModuleHasLines)
{
    for (int i = 0; i <= static_cast<int>(algo::PseudocodeId::EventQueue); ++i)
    {
        const char* const* lines = nullptr;
        int count = algo::GetPseudocode(static_cast<algo::PseudocodeId>(i), &lines);
        CHECK(count > 0);
        CHECK(lines != nullptr);
    }
}

TEST(LabApi_EveryModuleRunsOnHarbor)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);

    LabInput input = ApiLabDefaultInput();
    for (int i = 0; i <= static_cast<int>(LabAlgo::EventQueue); ++i)
    {
        LabAlgo algo = static_cast<LabAlgo>(i);
        CHECK(ApiLabRun(algo, input) == ApiStatus::Ok);

        int steps = ApiLabStepCount();
        CHECK(steps > 0);
        CHECK(ApiLabResultText()[0] != '\0');
        CHECK(ApiLabPseudocode(algo).lines.count > 0);

        LabFrame last = ApiLabFrame(steps - 1);
        CHECK_EQ(last.step, steps - 1);
        CHECK_EQ(last.stepCount, steps);
        CHECK(last.line >= 0);

        LabFrame first = ApiLabFrame(-1);
        CHECK_EQ(first.step, -1);
        CHECK_EQ(first.comparisons + first.examined + first.heapPops + first.tableCellsFilled, 0);

        if (last.kind == LabFrameKind::Graph)
        {
            GraphView graph = ApiLabGraph();
            CHECK_EQ(last.vertexState.count, graph.vertices.count);
            CHECK_EQ(last.edgeState.count, graph.edges.count);
        }
    }
    ApiShutdown();
}

TEST(LabApi_BriefExample_DpBeatsGreedy)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    LabInput input = ApiLabDefaultInput();
    input.useBriefExample = true;

    // Runs without a circuit loaded.
    CHECK(ApiLabRun(LabAlgo::KnapsackDp, input) == ApiStatus::Ok);
    LabFrame dp = ApiLabFrame(ApiLabStepCount() - 1);
    CHECK_EQ(dp.tableRows, 4);
    CHECK_EQ(dp.tableCols, 11);
    CHECK_EQ(dp.tableCells.data[3 * 11 + 10], 48);
    CHECK_EQ(dp.selectedIds.count, 2);

    CHECK(ApiLabRun(LabAlgo::GreedyPickup, input) == ApiStatus::Ok);
    LabFrame greedy = ApiLabFrame(ApiLabStepCount() - 1);
    CHECK_EQ(greedy.selectedIds.count, 1);
    CHECK_EQ(greedy.selectedIds.data[0], 0);   // A alone: benefit 30 < 48

    // Graph modules need a circuit.
    CHECK(ApiLabRun(LabAlgo::Bfs, input) == ApiStatus::NotLoaded);
    ApiShutdown();
}

TEST(LabApi_EventQueue_PopsInHeapOrder)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    CHECK(ApiLabRun(LabAlgo::EventQueue, ApiLabDefaultInput()) == ApiStatus::Ok);

    ArrayView<LabEventView> events = ApiLabEvents();
    CHECK(events.count > 0);
    LabFrame last = ApiLabFrame(ApiLabStepCount() - 1);
    CHECK(last.selectedIds.count > 0);

    for (int i = 1; i < last.selectedIds.count; ++i)
    {
        const LabEventView* prev = FindLabEvent(events, last.selectedIds.data[i - 1]);
        const LabEventView* cur = FindLabEvent(events, last.selectedIds.data[i]);
        CHECK(prev != nullptr && cur != nullptr);
        if (prev == nullptr || cur == nullptr) continue;

        bool ordered = prev->triggerTimeMs < cur->triggerTimeMs
            || (prev->triggerTimeMs == cur->triggerTimeMs && (prev->priority < cur->priority
            || (prev->priority == cur->priority && prev->sequence < cur->sequence)));
        CHECK(ordered);
    }

    // Cancelled events never reach the sink.
    for (int r = 0; r < last.rejectedIds.count; ++r)
    {
        for (int s = 0; s < last.selectedIds.count; ++s)
        {
            CHECK(last.selectedIds.data[s] != last.rejectedIds.data[r]);
        }
    }
    ApiShutdown();
}

TEST(LabApi_GeneratedTrack_SavesLoadsAndValidates)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLabSaveGenerated("build/test_lab_generated.trk") == ApiStatus::NotLoaded);

    LabInput input = ApiLabDefaultInput();
    input.seed = 3;
    CHECK(ApiLabRun(LabAlgo::TrackGenerator, input) == ApiStatus::Ok);

    GeneratorReportView report = ApiLabGeneratorReport();
    CHECK(report.attempts.count > 0);
    CHECK(report.acceptedAttempt >= 0);
    CHECK(report.attempts.data[report.acceptedAttempt].accepted);

    CHECK(ApiLabSaveGenerated("build/test_lab_generated.trk") == ApiStatus::Ok);
    CHECK(ApiLoadCircuitFile("build/test_lab_generated.trk") == ApiStatus::Ok);
    ValidationView validation = ApiValidateTrack();
    CHECK(validation.passed);
    CHECK(validation.checks.count > 0);
    CHECK(ApiGetTrackInfo().validatorPassed);
    ApiShutdown();
}
