#include "GraphGen.h"
#include "../src/core/util/Rng.h"

#include <cmath>

namespace
{
    double Euclid(const model::Graph& g, int a, int b)
    {
        double dx = g.GetVertex(a).x - g.GetVertex(b).x;
        double dy = g.GetVertex(a).y - g.GetVertex(b).y;
        return sqrt(dx * dx + dy * dy);
    }

    void AddPlainRoad(model::Graph* g, int a, int b)
    {
        double d = Euclid(*g, a, b) + 1.0;
        g->AddEdge(a, b, d, d / 20.0, d / 50.0, 1, false, false);
    }
}

model::Graph* MakeRandomGraph(int vertexCount, double avgDegree, int components, unsigned long long seed)
{
    Rng rng(seed);
    model::Graph* g = new model::Graph();
    double side = sqrt(static_cast<double>(vertexCount)) * 100.0;
    for (int i = 0; i < vertexCount; ++i)
    {
        g->AddVertex(rng.NextDouble() * side, rng.NextDouble() * side, model::VertexType::Intersection);
    }

    if (components < 1) components = 1;
    if (components > vertexCount) components = vertexCount;

    // Vertex v belongs to component v % components. Within a component,
    // members are v, v + c, v + 2c, ...; member k > 0 joins a random
    // earlier member.
    for (int v = components; v < vertexCount; ++v)
    {
        int comp = v % components;
        int k = (v - comp) / components;            // index of v among its component's members
        int earlier = rng.NextInt(0, k - 1);
        AddPlainRoad(g, v, comp + earlier * components);
    }

    int targetEdges = static_cast<int>(avgDegree * vertexCount / 2.0);
    int guard = 0;
    while (g->EdgeCount() < targetEdges && guard < targetEdges * 4)
    {
        ++guard;
        int a = rng.NextInt(0, vertexCount - 1);
        int comp = a % components;
        int members = (vertexCount - 1 - comp) / components + 1;
        int b = comp + rng.NextInt(0, members - 1) * components;
        if (a == b) continue;
        AddPlainRoad(g, a, b);
    }
    return g;
}

model::Graph* MakeGeometricGrid(int vertexCount, unsigned long long seed, int* outSide)
{
    Rng rng(seed);
    int side = static_cast<int>(sqrt(static_cast<double>(vertexCount)) + 0.5);
    if (side < 2) side = 2;
    if (outSide != nullptr) *outSide = side;

    model::Graph* g = new model::Graph();
    for (int row = 0; row < side; ++row)
    {
        for (int col = 0; col < side; ++col)
        {
            double x = col * 100.0 + (rng.NextDouble() - 0.5) * 60.0;
            double y = row * 100.0 + (rng.NextDouble() - 0.5) * 60.0;
            g->AddVertex(x, y, model::VertexType::Intersection);
        }
    }

    for (int row = 0; row < side; ++row)
    {
        for (int col = 0; col < side; ++col)
        {
            int v = row * side + col;
            int neighbours[3] = { -1, -1, -1 };
            if (col + 1 < side) neighbours[0] = v + 1;
            if (row + 1 < side) neighbours[1] = v + side;
            if (col + 1 < side && row + 1 < side && rng.NextDouble() < 0.2) neighbours[2] = v + side + 1;

            for (int n = 0; n < 3; ++n)
            {
                int u = neighbours[n];
                if (u < 0) continue;
                double straight = Euclid(*g, v, u);
                double distance = straight * (1.0 + rng.NextDouble() * 0.25);
                int difficulty = rng.NextInt(1, 5);
                double speed = 30.0 - 4.0 * difficulty;                      // units per second
                double time = distance / speed;
                double fuel = distance * 0.02 * (1.0 + 0.1 * (difficulty - 1));
                g->AddEdge(v, u, distance, time, fuel, difficulty, false, false);
            }
        }
    }
    return g;
}
