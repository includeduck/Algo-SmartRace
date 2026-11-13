#pragma once

namespace algo
{
    // Shared operation counters every algorithm in this project fills in
    // (when given a non-null pointer). Used by the 5 required experiments
    // and by the Algorithm Lab's counter display. Gameplay calls pass
    // nullptr and pay for nothing beyond the null check.
    struct AlgoStats
    {
        AlgoStats()
            : verticesVisited(0), edgesExamined(0), edgesRelaxed(0),
              heapPushes(0), heapPops(0), comparisons(0), swaps(0),
              dpStates(0), nodesExplored(0), prunedBranches(0),
              elapsedMicros(0.0)
        {
        }

        long long verticesVisited;
        long long edgesExamined;
        long long edgesRelaxed;
        long long heapPushes;
        long long heapPops;
        long long comparisons;
        long long swaps;
        long long dpStates;
        long long nodesExplored;
        long long prunedBranches;
        double elapsedMicros;
    };
}
