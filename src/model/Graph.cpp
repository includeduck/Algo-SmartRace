#include "Graph.h"

namespace model
{
    Graph::Graph()
        : m_nodePool(512)
    {
    }

    Graph::~Graph()
    {
    }

    int Graph::AddVertex(double x, double y, VertexType type, int checkpointOrder, bool active)
    {
        Vertex v;
        v.id = m_vertices.Size();
        v.x = x;
        v.y = y;
        v.type = type;
        v.checkpointOrder = checkpointOrder;
        v.active = active;

        m_vertices.Push(v);
        m_adj.Push(nullptr);
        m_radj.Push(nullptr);

        return v.id;
    }

    int Graph::AddEdge(int from, int to, double distance, double baseTime, double fuelCost,
        int difficulty, bool isShortcut, bool directed, EdgeStatus baseStatus)
    {
        Edge e;
        e.id = m_edges.Size();
        e.from = from;
        e.to = to;
        e.distance = distance;
        e.baseTime = baseTime;
        e.fuelCost = fuelCost;
        e.difficulty = difficulty;
        e.isShortcut = isShortcut;
        e.directed = directed;
        e.baseStatus = baseStatus;

        e.status = baseStatus;
        e.effDifficulty = difficulty;
        e.speedCap = -1.0;
        e.obstaclePenalty = 0.0;

        m_edges.Push(e);

        AdjNode* forward = m_nodePool.Allocate();
        forward->edgeId = e.id;
        forward->neighbor = to;
        forward->next = m_adj[from];
        m_adj[from] = forward;

        AdjNode* reverse = m_nodePool.Allocate();
        reverse->edgeId = e.id;
        reverse->neighbor = from;
        reverse->next = m_radj[to];
        m_radj[to] = reverse;

        if (!directed)
        {
            AdjNode* forwardBack = m_nodePool.Allocate();
            forwardBack->edgeId = e.id;
            forwardBack->neighbor = from;
            forwardBack->next = m_adj[to];
            m_adj[to] = forwardBack;

            AdjNode* reverseBack = m_nodePool.Allocate();
            reverseBack->edgeId = e.id;
            reverseBack->neighbor = to;
            reverseBack->next = m_radj[from];
            m_radj[from] = reverseBack;
        }

        return e.id;
    }

    void Graph::SetEdgeStatus(int edgeId, EdgeStatus status)
    {
        m_edges[edgeId].status = status;
    }

    void Graph::SetVertexActive(int vertexId, bool active)
    {
        m_vertices[vertexId].active = active;
    }

    Graph* Graph::Clone() const
    {
        Graph* copy = new Graph();

        for (int i = 0; i < m_vertices.Size(); ++i)
        {
            const Vertex& v = m_vertices[i];
            copy->AddVertex(v.x, v.y, v.type, v.checkpointOrder, v.active);
        }

        for (int i = 0; i < m_edges.Size(); ++i)
        {
            const Edge& e = m_edges[i];
            copy->AddEdge(e.from, e.to, e.distance, e.baseTime, e.fuelCost,
                e.difficulty, e.isShortcut, e.directed, e.baseStatus);

            Edge& copied = copy->GetEdgeMutable(e.id);
            copied.status = e.status;
            copied.effDifficulty = e.effDifficulty;
            copied.speedCap = e.speedCap;
            copied.obstaclePenalty = e.obstaclePenalty;
        }

        return copy;
    }
}
