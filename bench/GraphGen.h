#pragma once

#include "../src/model/Graph.h"

// Deterministic input graphs for Experiments 1 and 2. Every graph is
// undirected and built from a seed, so each run and each competing
// algorithm sees exactly the same instance.

// Random graph with `components` connected components: a random tree over
// each component's vertices (vertex i joins a random earlier vertex of its
// component), then random extra roads inside components until the average
// degree reaches `avgDegree` (avgDegree <= 2 gives a forest: no cycles).
// Coordinates are uniform in a square; distance = straight-line length + 1.
model::Graph* MakeRandomGraph(int vertexCount, double avgDegree, int components, unsigned long long seed);

// Road-network-like graph for routing: about vertexCount vertices on a
// jittered square grid, 4-neighbour roads plus ~20% diagonals. distance =
// straight-line length x (1 + 0..25%), so distance >= euclid on every edge
// (the A* admissibility condition); difficulty 1..5; travel time and fuel
// grow with distance and difficulty. Vertex 0 is the top-left corner and
// the last vertex the bottom-right corner.
model::Graph* MakeGeometricGrid(int vertexCount, unsigned long long seed, int* outSide);
