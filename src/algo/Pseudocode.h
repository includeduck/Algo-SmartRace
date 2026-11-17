#pragma once

// Pseudocode listings for the Algorithm Lab, and the line numbers each
// algorithm records in TraceStep::line. Both live here, side by side, so a
// recorded line can never point at the wrong text: an algorithm records
// `lines::bfs::kDequeue`, and that constant is the index of the
// "u = Q.dequeue()" line in the BFS listing below it in Pseudocode.cpp.
namespace algo
{
    enum class PseudocodeId
    {
        Bfs,
        Dfs,
        Dijkstra,
        AStar,
        Kruskal,
        KnapsackDp,
        GreedyPickup,
        RefuelDp,
        MergeSort,
        QuickSort,
        HeapSort,
        TrackGenerator,
        EventQueue,
    };

    // Writes the listing's line array to *outLines and returns its length
    // (0 for an unknown id). The strings are static; never freed.
    int GetPseudocode(PseudocodeId id, const char* const** outLines);

    namespace lines
    {
        namespace bfs
        {
            const int kStart = 2;
            const int kDequeue = 4;
            const int kExamine = 5;
            const int kDiscover = 7;
        }

        namespace dfs
        {
            const int kStart = 1;
            const int kExamine = 4;
            const int kDiscover = 5;
            const int kFinish = 7;
        }

        // Shared by Dijkstra and A*: their listings have the same shape.
        namespace shortest
        {
            const int kStart = 1;
            const int kPop = 3;
            const int kClose = 5;
            const int kExamine = 6;
            const int kRelax = 8;
            const int kPush = 9;
        }

        namespace kruskal
        {
            const int kExamine = 3;
            const int kAccept = 5;
            const int kReject = 6;
        }

        namespace knapsack
        {
            const int kBase = 1;
            const int kCell = 5;
            const int kTake = 7;
        }

        namespace greedy
        {
            const int kFree = 1;
            const int kTake = 4;
            const int kSkip = 5;
        }

        namespace refuel
        {
            const int kStart = 1;
            const int kCell = 5;
        }

        namespace merge
        {
            const int kCompare = 4;
            const int kPlace = 6;
        }

        namespace quick
        {
            const int kMedian = 2;
            const int kCompare = 4;
            const int kSwap = 5;
        }

        namespace heap
        {
            const int kExtract = 3;
            const int kSift = 5;
        }

        namespace generator
        {
            const int kMst = 4;
            const int kExtraRoad = 5;
            const int kPlace = 6;
            const int kBacktrack = 7;
            const int kObstacle = 8;
        }

        namespace events
        {
            const int kSchedule = 1;
            const int kCancel = 2;
            const int kPop = 5;
        }
    }
}
