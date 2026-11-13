#pragma once

#include "Types.h"
#include "../core/ds/DynArray.h"
#include "../core/ds/Pool.h"

namespace model
{
    // Graph: the track as an adjacency list (Project.pdf section 3.1).
    // O(V+E) memory, O(1) amortized neighbour iteration — the brief rejects
    // an adjacency matrix (O(V^2)) for these sparse circuits.
    //
    // Undirected edges add an AdjNode to both endpoints' out-adjacency.
    // Directed edges add one only at `from`. The reverse adjacency list
    // (Incoming) always gets an entry at `to` (and at `from` too, for an
    // undirected edge) — it backs directed-reachability and dead-end
    // analysis (Act 1) without re-scanning every edge.
    //
    // SetEdgeStatus/SetVertexActive are O(1): dynamic updates never rebuild
    // adjacency (Project.pdf section 3.1's explicit requirement).
    class Graph
    {
    public:
        Graph();
        ~Graph();

        Graph(const Graph&) = delete;
        Graph& operator=(const Graph&) = delete;

        int AddVertex(double x, double y, VertexType type, int checkpointOrder = -1, bool active = true);

        int AddEdge(int from, int to, double distance, double baseTime, double fuelCost,
            int difficulty, bool isShortcut, bool directed, EdgeStatus baseStatus = EdgeStatus::Open);

        int VertexCount() const
        {
            return m_vertices.Size();
        }

        int EdgeCount() const
        {
            return m_edges.Size();
        }

        const Vertex& GetVertex(int id) const
        {
            return m_vertices[id];
        }

        Vertex& GetVertexMutable(int id)
        {
            return m_vertices[id];
        }

        const Edge& GetEdge(int id) const
        {
            return m_edges[id];
        }

        Edge& GetEdgeMutable(int id)
        {
            return m_edges[id];
        }

        // Head of vertex v's out-adjacency list (the direction a car may
        // drive away from v). Walk via node->next; nullptr ends the list.
        const AdjNode* Neighbors(int v) const
        {
            return m_adj[v];
        }

        // Head of vertex v's in-adjacency list: edges that can reach v.
        const AdjNode* Incoming(int v) const
        {
            return m_radj[v];
        }

        bool IsTraversable(int edgeId) const
        {
            return m_edges[edgeId].status != EdgeStatus::Blocked;
        }

        void SetEdgeStatus(int edgeId, EdgeStatus status);
        void SetVertexActive(int vertexId, bool active);

        // Deep copy (generator candidates, Algorithm Lab sandbox). Preserves
        // current effective state, not just the base track.
        Graph* Clone() const;

    private:
        DynArray<Vertex> m_vertices;
        DynArray<Edge> m_edges;
        DynArray<AdjNode*> m_adj;     // per-vertex out-adjacency head
        DynArray<AdjNode*> m_radj;    // per-vertex in-adjacency head
        Pool<AdjNode> m_nodePool;
    };
}
