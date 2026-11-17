#include "Dfs.h"
#include "Pseudocode.h"
#include "../core/ds/Stack.h"
#include "../core/ds/Queue.h"

namespace algo
{
    AlternativeRoutesResult::~AlternativeRoutesResult()
    {
        for (int i = 0; i < routes.Size(); ++i) delete routes[i];
    }

    AlternativeRoutesResult::AlternativeRoutesResult(AlternativeRoutesResult&& other) noexcept
        : routes(static_cast<DynArray<DynArray<int>*>&&>(other.routes)), truncated(other.truncated)
    {
    }

    AlternativeRoutesResult& AlternativeRoutesResult::operator=(AlternativeRoutesResult&& other) noexcept
    {
        if (this != &other)
        {
            for (int i = 0; i < routes.Size(); ++i) delete routes[i];
            routes = static_cast<DynArray<DynArray<int>*>&&>(other.routes);
            truncated = other.truncated;
        }
        return *this;
    }

    // -----------------------------------------------------------------
    // DfsTraversal
    // -----------------------------------------------------------------
    namespace
    {
        struct TraversalFrame
        {
            int vertex;
            const model::AdjNode* it;
        };
    }

    DfsTraversalResult DfsTraversal(const model::Graph& graph, int source, AlgoStats* stats, Trace* trace)
    {
        DfsTraversalResult result;
        int n = graph.VertexCount();

        result.discoveryTime.Resize(n);
        result.finishTime.Resize(n);
        result.parent.Resize(n);
        for (int i = 0; i < n; ++i)
        {
            result.discoveryTime[i] = -1;
            result.finishTime[i] = -1;
            result.parent[i] = -1;
        }

        if (source < 0 || source >= n) return result;

        Stack<TraversalFrame> stack;
        int clock = 0;

        result.discoveryTime[source] = clock;
        ++clock;
        result.order.Push(source);
        if (stats != nullptr) ++stats->verticesVisited;
        if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, source, -1, 0.0, lines::dfs::kStart);

        TraversalFrame first = { source, graph.Neighbors(source) };
        stack.Push(first);

        while (!stack.Empty())
        {
            TraversalFrame& top = stack.Top();
            bool advanced = false;

            while (top.it != nullptr)
            {
                const model::AdjNode* node = top.it;
                top.it = node->next;

                if (stats != nullptr) ++stats->edgesExamined;
                if (trace != nullptr) trace->Record(TraceOp::ExamineEdge, node->edgeId, -1, 0.0, lines::dfs::kExamine);

                if (!graph.IsTraversable(node->edgeId)) continue;

                int v = node->neighbor;
                if (result.discoveryTime[v] != -1) continue;

                result.discoveryTime[v] = clock;
                ++clock;
                result.parent[v] = top.vertex;
                result.order.Push(v);
                if (stats != nullptr) ++stats->verticesVisited;
                if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, v, top.vertex, result.discoveryTime[v], lines::dfs::kDiscover);

                TraversalFrame next = { v, graph.Neighbors(v) };
                stack.Push(next);
                advanced = true;
                break;
            }

            if (!advanced)
            {
                int u = stack.Top().vertex;
                result.finishTime[u] = clock;
                ++clock;
                if (trace != nullptr) trace->Record(TraceOp::FinishVertex, u, -1, result.finishTime[u], lines::dfs::kFinish);
                stack.Pop();
            }
        }

        return result;
    }

    // -----------------------------------------------------------------
    // DfsCycles
    // -----------------------------------------------------------------
    DfsCycleResult DfsCycles(const model::Graph& graph, AlgoStats* stats, Trace* trace)
    {
        DfsCycleResult result;
        int n = graph.VertexCount();

        DynArray<int> color;        // 0 = white, 1 = gray, 2 = black
        DynArray<int> parent;
        DynArray<int> arrivalEdge;
        color.Resize(n);
        parent.Resize(n);
        arrivalEdge.Resize(n);
        for (int i = 0; i < n; ++i)
        {
            color[i] = 0;
            parent[i] = -1;
            arrivalEdge[i] = -1;
        }

        for (int s = 0; s < n && !result.hasCycle; ++s)
        {
            if (color[s] != 0) continue;

            Stack<TraversalFrame> stack;
            color[s] = 1;
            if (stats != nullptr) ++stats->verticesVisited;

            TraversalFrame first = { s, graph.Neighbors(s) };
            stack.Push(first);

            while (!stack.Empty() && !result.hasCycle)
            {
                TraversalFrame& top = stack.Top();
                bool advanced = false;

                while (top.it != nullptr)
                {
                    const model::AdjNode* node = top.it;
                    top.it = node->next;

                    if (stats != nullptr) ++stats->edgesExamined;
                    if (trace != nullptr) trace->Record(TraceOp::ExamineEdge, node->edgeId);

                    if (!graph.IsTraversable(node->edgeId)) continue;
                    if (node->edgeId == arrivalEdge[top.vertex]) continue;   // trivial undo

                    int v = node->neighbor;

                    if (color[v] == 1)
                    {
                        DynArray<int> rev;
                        int cur = top.vertex;
                        while (cur != -1 && cur != v)
                        {
                            rev.Push(cur);
                            cur = parent[cur];
                        }
                        rev.Push(v);
                        for (int i = rev.Size() - 1; i >= 0; --i) result.witnessCycle.Push(rev[i]);
                        result.witnessCycle.Push(v);
                        result.hasCycle = true;
                        break;
                    }

                    if (color[v] == 0)
                    {
                        color[v] = 1;
                        parent[v] = top.vertex;
                        arrivalEdge[v] = node->edgeId;
                        if (stats != nullptr) ++stats->verticesVisited;
                        if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, v, top.vertex);

                        TraversalFrame next = { v, graph.Neighbors(v) };
                        stack.Push(next);
                        advanced = true;
                        break;
                    }
                    // color[v] == 2 (black): already fully explored, safe.
                }

                if (result.hasCycle) break;

                if (!advanced)
                {
                    int u = stack.Top().vertex;
                    color[u] = 2;
                    if (trace != nullptr) trace->Record(TraceOp::FinishVertex, u);
                    stack.Pop();
                }
            }
        }

        return result;
    }

    // -----------------------------------------------------------------
    // DfsComponents
    // -----------------------------------------------------------------
    DfsComponentsResult DfsComponents(const model::Graph& graph, AlgoStats* stats, Trace* trace)
    {
        DfsComponentsResult result;
        int n = graph.VertexCount();
        result.componentId.Resize(n);
        for (int i = 0; i < n; ++i) result.componentId[i] = -1;

        Stack<int> stack;
        int nextId = 0;

        for (int s = 0; s < n; ++s)
        {
            if (result.componentId[s] != -1) continue;

            result.componentId[s] = nextId;
            stack.Push(s);
            if (stats != nullptr) ++stats->verticesVisited;
            if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, s, -1);

            while (!stack.Empty())
            {
                int u = stack.Pop();
                if (trace != nullptr) trace->Record(TraceOp::VisitVertex, u);

                for (const model::AdjNode* node = graph.Neighbors(u); node != nullptr; node = node->next)
                {
                    if (stats != nullptr) ++stats->edgesExamined;
                    int v = node->neighbor;
                    if (result.componentId[v] == -1)
                    {
                        result.componentId[v] = nextId;
                        stack.Push(v);
                        if (stats != nullptr) ++stats->verticesVisited;
                        if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, v, u);
                    }
                }

                for (const model::AdjNode* node = graph.Incoming(u); node != nullptr; node = node->next)
                {
                    if (stats != nullptr) ++stats->edgesExamined;
                    int v = node->neighbor;
                    if (result.componentId[v] == -1)
                    {
                        result.componentId[v] = nextId;
                        stack.Push(v);
                        if (stats != nullptr) ++stats->verticesVisited;
                        if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, v, u);
                    }
                }
            }

            ++nextId;
        }

        result.componentCount = nextId;
        return result;
    }

    // -----------------------------------------------------------------
    // DfsBridges (iterative Tarjan, undirected skeleton)
    // -----------------------------------------------------------------
    namespace
    {
        struct BridgeFrame
        {
            int vertex;
            const model::AdjNode* outIt;
            const model::AdjNode* inIt;
            bool usingIn;
        };
    }

    DfsBridgesResult DfsBridges(const model::Graph& graph, AlgoStats* stats, Trace* trace)
    {
        DfsBridgesResult result;
        int n = graph.VertexCount();

        DynArray<int> disc;
        DynArray<int> low;
        DynArray<int> arrivalEdge;
        DynArray<bool> visited;
        disc.Resize(n);
        low.Resize(n);
        arrivalEdge.Resize(n);
        visited.Resize(n);
        for (int i = 0; i < n; ++i)
        {
            disc[i] = -1;
            low[i] = -1;
            arrivalEdge[i] = -1;
            visited[i] = false;
        }

        int clock = 0;

        for (int s = 0; s < n; ++s)
        {
            if (visited[s]) continue;

            Stack<BridgeFrame> stack;
            visited[s] = true;
            disc[s] = clock;
            low[s] = clock;
            ++clock;
            if (stats != nullptr) ++stats->verticesVisited;

            BridgeFrame first = { s, graph.Neighbors(s), graph.Incoming(s), false };
            stack.Push(first);

            while (!stack.Empty())
            {
                BridgeFrame& top = stack.Top();
                bool advanced = false;

                while (true)
                {
                    const model::AdjNode* node = nullptr;

                    if (!top.usingIn)
                    {
                        if (top.outIt != nullptr)
                        {
                            node = top.outIt;
                            top.outIt = node->next;
                        }
                        else
                        {
                            top.usingIn = true;
                            continue;
                        }
                    }
                    else
                    {
                        if (top.inIt != nullptr)
                        {
                            node = top.inIt;
                            top.inIt = node->next;
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (stats != nullptr) ++stats->edgesExamined;
                    if (node->edgeId == arrivalEdge[top.vertex]) continue;

                    int v = node->neighbor;

                    if (visited[v])
                    {
                        if (low[top.vertex] > disc[v]) low[top.vertex] = disc[v];
                        continue;
                    }

                    visited[v] = true;
                    disc[v] = clock;
                    low[v] = clock;
                    ++clock;
                    arrivalEdge[v] = node->edgeId;
                    if (stats != nullptr) ++stats->verticesVisited;

                    BridgeFrame next = { v, graph.Neighbors(v), graph.Incoming(v), false };
                    stack.Push(next);
                    advanced = true;
                    break;
                }

                if (!advanced)
                {
                    int u = stack.Top().vertex;
                    int arrivedBy = arrivalEdge[u];
                    stack.Pop();

                    if (!stack.Empty())
                    {
                        BridgeFrame& parentFrame = stack.Top();
                        int p = parentFrame.vertex;
                        if (low[u] < low[p]) low[p] = low[u];
                        if (low[u] > disc[p])
                        {
                            result.bridgeEdgeIds.Push(arrivedBy);
                            if (trace != nullptr) trace->Record(TraceOp::AcceptEdge, arrivedBy);
                        }
                    }
                }
            }
        }

        return result;
    }

    // -----------------------------------------------------------------
    // DfsDeadEnds
    // -----------------------------------------------------------------
    namespace
    {
        void FloodWithoutEdge(const model::Graph& graph, int start, int excludeEdgeId, DynArray<bool>* visited)
        {
            Stack<int> stack;
            (*visited)[start] = true;
            stack.Push(start);

            while (!stack.Empty())
            {
                int u = stack.Pop();

                for (const model::AdjNode* node = graph.Neighbors(u); node != nullptr; node = node->next)
                {
                    if (node->edgeId == excludeEdgeId) continue;
                    int v = node->neighbor;
                    if (!(*visited)[v])
                    {
                        (*visited)[v] = true;
                        stack.Push(v);
                    }
                }

                for (const model::AdjNode* node = graph.Incoming(u); node != nullptr; node = node->next)
                {
                    if (node->edgeId == excludeEdgeId) continue;
                    int v = node->neighbor;
                    if (!(*visited)[v])
                    {
                        (*visited)[v] = true;
                        stack.Push(v);
                    }
                }
            }
        }

        bool IsMandatoryType(model::VertexType t)
        {
            return t == model::VertexType::Start || t == model::VertexType::Checkpoint
                || t == model::VertexType::Finish;
        }
    }

    DfsDeadEndsResult DfsDeadEnds(const model::Graph& graph, int finish, AlgoStats* stats, Trace* trace)
    {
        DfsDeadEndsResult result;
        int n = graph.VertexCount();

        DynArray<bool> isDeadEnd;
        isDeadEnd.Resize(n);
        for (int i = 0; i < n; ++i) isDeadEnd[i] = false;

        if (finish >= 0 && finish < n)
        {
            DynArray<bool> canReachFinish;
            canReachFinish.Resize(n);
            for (int i = 0; i < n; ++i) canReachFinish[i] = false;

            Queue<int> frontier;
            canReachFinish[finish] = true;
            frontier.Enqueue(finish);
            if (stats != nullptr) ++stats->verticesVisited;

            while (!frontier.Empty())
            {
                int u = frontier.Dequeue();
                for (const model::AdjNode* node = graph.Incoming(u); node != nullptr; node = node->next)
                {
                    if (stats != nullptr) ++stats->edgesExamined;
                    if (!graph.IsTraversable(node->edgeId)) continue;

                    int v = node->neighbor;
                    if (!canReachFinish[v])
                    {
                        canReachFinish[v] = true;
                        frontier.Enqueue(v);
                        if (stats != nullptr) ++stats->verticesVisited;
                    }
                }
            }

            for (int v = 0; v < n; ++v)
            {
                if (!canReachFinish[v] && !IsMandatoryType(graph.GetVertex(v).type)) isDeadEnd[v] = true;
            }
        }

        DfsBridgesResult bridges = DfsBridges(graph, stats, trace);
        for (int i = 0; i < bridges.bridgeEdgeIds.Size(); ++i)
        {
            int edgeId = bridges.bridgeEdgeIds[i];
            const model::Edge& e = graph.GetEdge(edgeId);

            DynArray<bool> sideX;
            DynArray<bool> sideY;
            sideX.Resize(n);
            sideY.Resize(n);
            for (int k = 0; k < n; ++k)
            {
                sideX[k] = false;
                sideY[k] = false;
            }

            FloodWithoutEdge(graph, e.from, edgeId, &sideX);
            FloodWithoutEdge(graph, e.to, edgeId, &sideY);

            bool finishInX = (finish >= 0 && finish < n) ? sideX[finish] : false;
            bool finishInY = (finish >= 0 && finish < n) ? sideY[finish] : false;

            const DynArray<bool>* deadSide = nullptr;
            if (finishInX && !finishInY)
            {
                deadSide = &sideY;
            }
            else if (finishInY && !finishInX)
            {
                deadSide = &sideX;
            }
            else
            {
                continue;
            }

            bool hasMandatory = false;
            for (int v = 0; v < n && !hasMandatory; ++v)
            {
                if ((*deadSide)[v] && IsMandatoryType(graph.GetVertex(v).type)) hasMandatory = true;
            }

            if (!hasMandatory)
            {
                for (int v = 0; v < n; ++v)
                {
                    if ((*deadSide)[v]) isDeadEnd[v] = true;
                }
            }
        }

        for (int v = 0; v < n; ++v)
        {
            if (isDeadEnd[v]) result.deadEndVertices.Push(v);
        }

        return result;
    }

    // -----------------------------------------------------------------
    // DfsAlternativeRoutes
    // -----------------------------------------------------------------
    namespace
    {
        bool EdgeInMainRoute(const DynArray<int>& mainRoute, int from, int to)
        {
            int n = mainRoute.Size();
            if (n < 2) return false;

            for (int i = 0; i < n; ++i)
            {
                int a = mainRoute[i];
                int b = mainRoute[(i + 1) % n];
                if ((a == from && b == to) || (a == to && b == from)) return true;
            }
            return false;
        }

        void EnumerateRoutes(const model::Graph& graph, int current, int target,
            DynArray<int>* path, DynArray<bool>* onPath, bool& divergedFromMain,
            const DynArray<int>& mainRoute, int maxRoutes, int maxDepth,
            AlternativeRoutesResult* result, AlgoStats* stats, Trace* trace)
        {
            if (result->routes.Size() >= maxRoutes)
            {
                result->truncated = true;
                return;
            }

            if (current == target)
            {
                if (divergedFromMain && path->Size() >= 2)
                {
                    DynArray<int>* route = new DynArray<int>();
                    for (int i = 0; i < path->Size(); ++i) route->Push((*path)[i]);
                    result->routes.Push(route);
                }
                return;
            }

            if (path->Size() - 1 >= maxDepth)
            {
                result->truncated = true;
                return;
            }

            for (const model::AdjNode* node = graph.Neighbors(current); node != nullptr; node = node->next)
            {
                if (result->routes.Size() >= maxRoutes)
                {
                    result->truncated = true;
                    return;
                }

                if (stats != nullptr) ++stats->edgesExamined;
                if (!graph.IsTraversable(node->edgeId)) continue;

                int next = node->neighbor;
                if ((*onPath)[next]) continue;

                bool edgeDiffers = !EdgeInMainRoute(mainRoute, current, next);
                bool prevDiverged = divergedFromMain;
                divergedFromMain = divergedFromMain || edgeDiffers;

                (*onPath)[next] = true;
                path->Push(next);
                if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, next, current);

                EnumerateRoutes(graph, next, target, path, onPath, divergedFromMain,
                    mainRoute, maxRoutes, maxDepth, result, stats, trace);

                path->Pop();
                (*onPath)[next] = false;
                divergedFromMain = prevDiverged;
            }
        }
    }

    AlternativeRoutesResult DfsAlternativeRoutes(const model::Graph& graph, int source, int target,
        const DynArray<int>& mainRoute, int maxRoutes, int maxDepth, AlgoStats* stats, Trace* trace)
    {
        AlternativeRoutesResult result;
        int n = graph.VertexCount();
        if (source < 0 || source >= n || target < 0 || target >= n) return result;

        DynArray<bool> onPath;
        onPath.Resize(n);
        for (int i = 0; i < n; ++i) onPath[i] = false;

        DynArray<int> path;
        path.Push(source);
        onPath[source] = true;
        bool diverged = false;

        EnumerateRoutes(graph, source, target, &path, &onPath, diverged,
            mainRoute, maxRoutes, maxDepth, &result, stats, trace);

        return result;
    }
}
