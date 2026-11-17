#include "Pseudocode.h"

namespace algo
{
    namespace
    {
        // Each listing's line indices must match the constants in
        // Pseudocode.h (noted at the end of each line below).
        const char* const kBfs[] =
        {
            "BFS(G, s, t):",
            "  for each vertex v: dist[v] = -1, parent[v] = none",
            "  dist[s] = 0; Q.enqueue(s)",                                         // kStart
            "  while Q is not empty:",
            "    u = Q.dequeue()",                                                 // kDequeue
            "    for each road (u, v) that is not blocked:",                       // kExamine
            "      if dist[v] == -1:",
            "        dist[v] = dist[u] + 1; parent[v] = u; Q.enqueue(v)",          // kDiscover
            "  return the path to t by following parent[] back from t",
        };

        const char* const kDfs[] =
        {
            "DFS(G, s):",
            "  time = 0; discover(s); stack.push(s)",                              // kStart
            "  while the stack is not empty:",
            "    u = stack.top()",
            "    if u has an unexamined road (u, v) that is not blocked:",         // kExamine
            "      if v is undiscovered: discover(v), parent[v] = u, push(v)",     // kDiscover
            "    else:",
            "      finish(u); stack.pop()",                                        // kFinish
        };

        const char* const kDijkstra[] =
        {
            "Dijkstra(G, s, t):",
            "  dist[v] = infinity for every v; dist[s] = 0; H.push(0, s)",         // kStart
            "  while H is not empty:",
            "    (d, u) = H.pop()",                                                // kPop
            "    if u is closed: skip it (stale entry: lazy decrease-key)",
            "    close u; if u == t: stop",                                        // kClose
            "    for each road (u, v) that is not blocked:",                       // kExamine
            "      if dist[u] + cost(u, v) < dist[v]:",
            "        dist[v] = dist[u] + cost(u, v); parent[v] = u",               // kRelax
            "        H.push(dist[v], v)",                                          // kPush
        };

        const char* const kAStar[] =
        {
            "AStar(G, s, t):  h(v) = rho x straight-line distance(v, t)",
            "  g[v] = infinity for every v; g[s] = 0; H.push(h(s), s)",            // kStart
            "  while H is not empty:",
            "    (f, u) = H.pop()",                                                // kPop
            "    if u is closed: skip it (h is consistent: never reopened)",
            "    close u; if u == t: stop",                                        // kClose
            "    for each road (u, v) that is not blocked:",                       // kExamine
            "      if g[u] + cost(u, v) < g[v]:",
            "        g[v] = g[u] + cost(u, v); parent[v] = u",                     // kRelax
            "        H.push(g[v] + h(v), v)",                                      // kPush
        };

        const char* const kKruskal[] =
        {
            "Kruskal(V, E):",
            "  sort the roads by weight (Merge Sort; ties by road id)",
            "  every vertex starts in its own set (union-find)",
            "  for each road (u, v) in sorted order:",                             // kExamine
            "    if Find(u) != Find(v):",
            "      Union(u, v); keep (u, v) in the tree",                          // kAccept
            "    else: reject (u, v), it would close a cycle",                     // kReject
            "  return the tree (a forest if the graph was disconnected)",
        };

        const char* const kKnapsack[] =
        {
            "KnapsackDP(items 1..m, budget B):",
            "  dp[0][b] = 0 for every b",
            "  for i = 1..m:",
            "    for b = 0..B:",
            "      dp[i][b] = dp[i-1][b]   (skip item i)",
            "      if cost[i] <= b: dp[i][b] = max(dp[i][b], dp[i-1][b-cost[i]] + benefit[i])",   // kCell
            "  b = B",
            "  for i = m..1: if dp[i][b] != dp[i-1][b]: take item i; b = b - cost[i]",            // kTake
        };

        const char* const kGreedy[] =
        {
            "Greedy(items, budget B):",
            "  take every zero-cost item with benefit > 0",                        // kFree
            "  sort the rest by benefit/cost, highest first (ties: lower id)",
            "  for each item in that order:",
            "    if cost <= remaining: take it; remaining = remaining - cost",    // kTake
            "    else: skip it",                                                   // kSkip
        };

        const char* const kRefuel[] =
        {
            "Refuel(legs, capacity C):",
            "  dp[0][startFuel] = 0; every other state is unreachable",
            "  for each stop j and fuel f with dp[j][f] reachable:",
            "    for a = 0..min(C - f, maxBuy)   (a = 0 unless j is an active station):",
            "      if f + a >= leg[j]:",
            "        dp[j+1][f + a - leg[j]] = min(itself, dp[j][f] + stopTime(a))",   // kCell
            "  answer = least dp[m][f]; walk the choices back for each purchase",
        };

        const char* const kMerge[] =
        {
            "MergeSort(A, lo, hi):",
            "  if hi - lo < 2: return",
            "  mid = (lo + hi) / 2",
            "  MergeSort(A, lo, mid); MergeSort(A, mid, hi)",
            "  while both runs have elements: compare their heads",               // kCompare
            "    move the smaller head to the buffer (left one on ties: stable)",
            "  copy the merged run back into A[lo..hi)",                           // kPlace
        };

        const char* const kQuick[] =
        {
            "QuickSort(A, lo, hi):",
            "  while lo < hi:",
            "    move the median of A[lo], A[mid], A[hi] to A[lo]; pivot = A[lo]",    // kMedian
            "    lt = lo, i = lo + 1, gt = hi   (three-way partition)",
            "    while i <= gt: compare A[i] with the pivot",                      // kCompare
            "      smaller: swap A[lt], A[i] | larger: swap A[i], A[gt] | equal: i++",   // kSwap
            "    recurse into the smaller side, loop on the larger one",
        };

        const char* const kHeap[] =
        {
            "HeapSort(A, n):",
            "  for i = n/2 - 1 down to 0: SiftDown(i, n)   (last-ranked on top)",
            "  for end = n - 1 down to 1:",
            "    swap A[0], A[end]",                                               // kExtract
            "    SiftDown(0, end)",
            "SiftDown(root, size): take the child that ranks later; if it ranks after root, swap and repeat",   // kSift
        };

        const char* const kGenerator[] =
        {
            "GenerateTrack(seed):",
            "  place vertices on a jittered grid",
            "  candidate roads: each vertex to its 4 nearest neighbours",
            "  reject the attempt if the candidate roads are disconnected",
            "  Kruskal: the minimum spanning tree is the backbone",                // kMst
            "  add the shortest extra roads for cycles; mark 2 as shortcuts",      // kExtraRoad
            "  backtracking: place Start, CP1..CP10, Finish, fuel and nitro",      // kPlace
            "    undo and try the next vertex when a constraint fails",            // kBacktrack
            "  backtracking: place obstacles, never on a bridge road",             // kObstacle
            "  validate the whole track; on failure retry with the next attempt",
        };

        const char* const kEvents[] =
        {
            "EventQueue: a min-heap keyed by (triggerTime, priority, sequence)",
            "Schedule(event): sequence = next counter; H.push(event)",             // kSchedule
            "Cancel(id): mark the event cancelled (lazy deletion)",                // kCancel
            "ProcessUntil(now):",
            "  while H.top().triggerTime <= now:",
            "    e = H.pop(); if e is cancelled: skip it",                         // kPop
            "    apply e",
        };

        template <int N>
        int Listing(const char* const (&lines)[N], const char* const** out)
        {
            *out = lines;
            return N;
        }
    }

    int GetPseudocode(PseudocodeId id, const char* const** outLines)
    {
        switch (id)
        {
            case PseudocodeId::Bfs: return Listing(kBfs, outLines);
            case PseudocodeId::Dfs: return Listing(kDfs, outLines);
            case PseudocodeId::Dijkstra: return Listing(kDijkstra, outLines);
            case PseudocodeId::AStar: return Listing(kAStar, outLines);
            case PseudocodeId::Kruskal: return Listing(kKruskal, outLines);
            case PseudocodeId::KnapsackDp: return Listing(kKnapsack, outLines);
            case PseudocodeId::GreedyPickup: return Listing(kGreedy, outLines);
            case PseudocodeId::RefuelDp: return Listing(kRefuel, outLines);
            case PseudocodeId::MergeSort: return Listing(kMerge, outLines);
            case PseudocodeId::QuickSort: return Listing(kQuick, outLines);
            case PseudocodeId::HeapSort: return Listing(kHeap, outLines);
            case PseudocodeId::TrackGenerator: return Listing(kGenerator, outLines);
            case PseudocodeId::EventQueue: return Listing(kEvents, outLines);
            default:
            {
                *outLines = nullptr;
                return 0;
            }
        }
    }
}
