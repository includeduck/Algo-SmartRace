#pragma once

#include "../io/TrackLoader.h"
#include "../algo/Trace.h"
#include "../model/Rules.h"
#include "../core/ds/DynArray.h"

namespace sim
{
    struct GeneratorParams
    {
        GeneratorParams()
            : seed(1), vertexCount(28), width(840.0), height(620.0),
              laps(Rules::kDefaultLaps), maxAttempts(Rules::kGeneratorMaxRetries)
        {
        }

        unsigned long long seed;
        int vertexCount;
        double width;
        double height;
        int laps;
        int maxAttempts;
    };

    struct GenAttempt
    {
        int attempt;
        bool accepted;
        char reason[160];             // why it was rejected ("" if accepted)
        long long nodesExplored;        // backtracking assignments tried
        long long prunedBranches;         // candidates cut by a constraint before recursing
    };

    struct GenReport
    {
        GenReport()
            : acceptedAttempt(-1)
        {
        }

        DynArray<GenAttempt> attempts;
        int acceptedAttempt;   // index into attempts, -1 if every attempt failed
    };

    // State codes the generator records in its trace, for the Algorithm Lab.
    // Vertices (Place a=vertex, b=role, value=checkpoint order):
    const int kGenRoleNone = 0;
    const int kGenRoleStart = 1;
    const int kGenRoleFinish = 2;
    const int kGenRoleCheckpoint = 3;
    const int kGenRoleFuel = 4;
    const int kGenRoleNitro = 5;
    // Edges (AcceptEdge = tree, Reject = would close a cycle, MarkEdge b=):
    const int kGenEdgeExtra = 4;
    const int kGenEdgeShortcut = 5;
    const int kGenEdgeOilSpill = 6;
    const int kGenEdgeRoadBlock = 7;

    // Automatic track generation (Project.pdf section 5.4, Challenge 1):
    //  1. place `vertexCount` vertices on a jittered grid;
    //  2. candidate roads: each vertex to its 4 nearest neighbours;
    //  3. reject the attempt if those roads are disconnected (Kruskal would
    //     then return a spanning FOREST, not a tree);
    //  4. Kruskal: the minimum spanning tree is the backbone;
    //  5. add the shortest remaining candidates (vertex degree <= 4) for
    //     loops; the two longest of those become difficulty-5 shortcuts;
    //  6. backtracking places Start, CP1..CP10, Finish, 2 fuel stations and
    //     2 nitro pickups, with these pruning rules:
    //       - leaves (degree 1) are never assigned a role: they are kept as
    //         dead ends, which the brief requires at least one of;
    //       - consecutive checkpoints are 2 or 3 roads apart (never
    //         adjacent, never far), so laps stay compact but not trivial;
    //       - the Finish is next to the Start and within 3 roads of CP10, so
    //         a lap closes naturally;
    //       - the two fuel stations are at least 3 roads apart, so they
    //         serve different parts of the lap;
    //  7. backtracking places an oil spill and a road block on two roads that
    //     are NOT bridges (closing a bridge could cut the track in two), do
    //     not touch Start or Finish, and share no vertex;
    //  8. fuel is scaled so one Balanced lap uses about 45% of a tank (the
    //     same balance as the hand-made circuits: one stop per 3-lap race);
    //  9. the whole track must pass ValidateTrack; otherwise the attempt is
    //     rejected with the validator's reason and the next one starts.
    // Up to `maxAttempts` attempts, each with its own seed derived from
    // `seed`, so the outcome is deterministic.
    //
    // On success `out` holds the track (it owns the graph). `outCandidates`
    // receives the candidate-road graph of the attempt `trace` describes
    // (the accepted one, or the last one if all failed); the caller owns it.
    // `trace` (nullable) records only that one attempt. Returns false if
    // every attempt failed; `report` lists every attempt either way.
    bool GenerateTrack(const GeneratorParams& params, io::TrackData* out, model::Graph** outCandidates,
        GenReport* report, algo::Trace* trace);
}
