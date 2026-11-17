#include "TrackGenerator.h"
#include "TrackValidator.h"
#include "../algo/Kruskal.h"
#include "../algo/Bfs.h"
#include "../algo/Dfs.h"
#include "../algo/Dijkstra.h"
#include "../algo/RoutePlanner.h"
#include "../algo/Pseudocode.h"
#include "../model/CostModel.h"
#include "../core/util/Rng.h"
#include "../core/util/MathUtil.h"
#include "../core/util/String.h"

#include <cmath>
#include <cstdio>

namespace sim
{
    namespace
    {
        const double kSpeed = 42.0;            // world units per second, as in the hand-made circuits
        const double kLapFuelUnits = 45.0;      // a Balanced lap is tuned to ~45% of a tank
        const int kNeighbours = 4;
        const int kMaxDegree = 4;
        const int kExtraRoadsMin = 3;
        const long long kPlacementNodeLimit = 200000;

        // Backtracking slots, filled in this order.
        const int kSlotStart = 0;
        const int kSlotFirstCheckpoint = 1;     // CP1 .. CP10 are slots 1..10
        const int kSlotFinish = 11;
        const int kSlotFuel1 = 12;
        const int kSlotFuel2 = 13;
        const int kSlotNitro1 = 14;
        const int kSlotCount = 16;

        struct Work
        {
            int n;
            DynArray<double> x;
            DynArray<double> y;
            DynArray<algo::CandidateEdge> candidates;
            DynArray<double> slack;        // per candidate: distance = straight line x (1 + slack)
            DynArray<int> difficulty;       // per candidate
            DynArray<bool> kept;              // per candidate: in the final track
            DynArray<bool> inTree;              // per candidate: a Kruskal tree edge
            DynArray<bool> shortcut;              // per candidate
            DynArray<int> keptIds;                  // kept candidates, in id order (= final edge order)
            DynArray<int> degree;                     // per vertex, over kept roads
            DynArray<int> hops;                         // n x n road counts over kept roads, -1 = unreachable
            DynArray<bool> leaf;
            DynArray<int> order;                          // shuffled vertex order tried by the backtracking
            DynArray<int> role;
            DynArray<int> checkpointOrder;
            int slotVertex[kSlotCount];
            int oilCandidate;
            int blockCandidate;
            long long nodes;
            long long pruned;
            bool limitHit;
            algo::Trace* trace;
        };

        void Reject(GenAttempt* attempt, const char* reason)
        {
            attempt->accepted = false;
            String::StrCopy(attempt->reason, static_cast<int>(sizeof(attempt->reason)), reason);
        }

        int Hop(const Work& w, int a, int b)
        {
            return w.hops[a * w.n + b];
        }

        void PlaceVertices(Work& w, Rng& rng, const GeneratorParams& p)
        {
            int cols = static_cast<int>(ceil(sqrt(p.vertexCount * p.width / p.height)));
            if (cols < 2) cols = 2;
            int rows = (p.vertexCount + cols - 1) / cols;
            double cellW = p.width / cols;
            double cellH = p.height / rows;

            // Jitter stays within +/-30% of a cell, so any two vertices are
            // at least 40% of a cell apart: no near-duplicate positions.
            for (int i = 0; i < p.vertexCount; ++i)
            {
                int r = i / cols;
                int c = i % cols;
                w.x.Push((c + 0.5) * cellW + (rng.NextDouble() - 0.5) * 0.6 * cellW);
                w.y.Push((r + 0.5) * cellH + (rng.NextDouble() - 0.5) * 0.6 * cellH);
            }
        }

        void BuildCandidates(Work& w, Rng& rng)
        {
            int n = w.n;
            DynArray<bool> seen;
            seen.Resize(n * n);
            for (int i = 0; i < n * n; ++i) seen[i] = false;

            for (int i = 0; i < n; ++i)
            {
                // The k nearest neighbours, kept sorted by distance (ties by id).
                int best[kNeighbours];
                double bestDist[kNeighbours];
                int count = 0;
                for (int j = 0; j < n; ++j)
                {
                    if (j == i) continue;
                    double d = MathUtil::Dist2D(w.x[i], w.y[i], w.x[j], w.y[j]);
                    if (count == kNeighbours && d >= bestDist[count - 1]) continue;

                    int pos = (count < kNeighbours) ? count : kNeighbours - 1;
                    while (pos > 0 && bestDist[pos - 1] > d)
                    {
                        best[pos] = best[pos - 1];
                        bestDist[pos] = bestDist[pos - 1];
                        --pos;
                    }
                    best[pos] = j;
                    bestDist[pos] = d;
                    if (count < kNeighbours) ++count;
                }

                for (int k = 0; k < count; ++k)
                {
                    int a = MathUtil::Min(i, best[k]);
                    int b = MathUtil::Max(i, best[k]);
                    if (seen[a * n + b]) continue;
                    seen[a * n + b] = true;

                    algo::CandidateEdge c;
                    c.id = w.candidates.Size();
                    c.from = a;
                    c.to = b;
                    c.weight = bestDist[k];
                    w.candidates.Push(c);
                    w.slack.Push(rng.NextDouble() * 0.25);
                    w.difficulty.Push(1);
                    w.kept.Push(false);
                    w.inTree.Push(false);
                    w.shortcut.Push(false);
                }
            }
        }

        model::Graph* BuildCandidateGraph(const Work& w)
        {
            model::Graph* g = new model::Graph();
            for (int i = 0; i < w.n; ++i) g->AddVertex(w.x[i], w.y[i], model::VertexType::Intersection);
            for (int i = 0; i < w.candidates.Size(); ++i)
            {
                const algo::CandidateEdge& c = w.candidates[i];
                g->AddEdge(c.from, c.to, c.weight, c.weight / kSpeed, c.weight * 0.02, 1, false, false);
            }
            return g;
        }

        // The track so far (kept roads only), for hop counts and bridges.
        model::Graph* BuildKeptGraph(const Work& w)
        {
            model::Graph* g = new model::Graph();
            for (int i = 0; i < w.n; ++i) g->AddVertex(w.x[i], w.y[i], model::VertexType::Intersection);
            for (int k = 0; k < w.keptIds.Size(); ++k)
            {
                const algo::CandidateEdge& c = w.candidates[w.keptIds[k]];
                g->AddEdge(c.from, c.to, c.weight, c.weight / kSpeed, c.weight * 0.02, 1, false, false);
            }
            return g;
        }

        int RoleForSlot(int slot)
        {
            if (slot == kSlotStart) return kGenRoleStart;
            if (slot == kSlotFinish) return kGenRoleFinish;
            if (slot == kSlotFuel1 || slot == kSlotFuel2) return kGenRoleFuel;
            if (slot >= kSlotNitro1) return kGenRoleNitro;
            return kGenRoleCheckpoint;
        }

        bool IsCheckpointSlot(int slot)
        {
            return slot >= kSlotFirstCheckpoint && slot < kSlotFinish;
        }

        // The pruning rules (see TrackGenerator.h for why each is safe).
        bool Admissible(const Work& w, int slot, int v)
        {
            if (w.role[v] != kGenRoleNone || w.leaf[v]) return false;

            if (IsCheckpointSlot(slot))
            {
                int h = Hop(w, w.slotVertex[slot - 1], v);
                return h >= 2 && h <= 3;
            }
            if (slot == kSlotFinish)
            {
                int fromCp10 = Hop(w, w.slotVertex[kSlotFinish - 1], v);
                return Hop(w, w.slotVertex[kSlotStart], v) == 1 && fromCp10 >= 1 && fromCp10 <= 3;
            }
            if (slot == kSlotFuel2) return Hop(w, w.slotVertex[kSlotFuel1], v) >= 3;
            return true;
        }

        bool PlaceSlot(Work& w, int slot)
        {
            if (slot == kSlotCount) return true;

            for (int i = 0; i < w.n; ++i)
            {
                if (w.nodes >= kPlacementNodeLimit)
                {
                    w.limitHit = true;
                    return false;
                }

                int v = w.order[i];
                if (!Admissible(w, slot, v))
                {
                    ++w.pruned;
                    continue;
                }

                ++w.nodes;
                int role = RoleForSlot(slot);
                int checkpoint = IsCheckpointSlot(slot) ? slot : -1;
                w.role[v] = role;
                w.checkpointOrder[v] = checkpoint;
                w.slotVertex[slot] = v;
                if (w.trace != nullptr) w.trace->Record(algo::TraceOp::Place, v, role, checkpoint, algo::lines::generator::kPlace);

                if (PlaceSlot(w, slot + 1)) return true;

                w.role[v] = kGenRoleNone;
                w.checkpointOrder[v] = -1;
                w.slotVertex[slot] = -1;
                if (w.trace != nullptr) w.trace->Record(algo::TraceOp::Backtrack, v, -1, 0.0, algo::lines::generator::kBacktrack);
                if (w.limitHit) return false;
            }
            return false;
        }

        int EdgeStateCode(const Work& w, int candidate)
        {
            if (w.shortcut[candidate]) return kGenEdgeShortcut;
            return w.inTree[candidate] ? 2 : kGenEdgeExtra;   // 2 = tree road, as AcceptEdge records it
        }

        bool ObstacleAllowed(const Work& w, const DynArray<bool>& isBridge, int keptIndex)
        {
            if (isBridge[keptIndex]) return false;
            const algo::CandidateEdge& c = w.candidates[w.keptIds[keptIndex]];
            int start = w.slotVertex[kSlotStart];
            int finish = w.slotVertex[kSlotFinish];
            return c.from != start && c.to != start && c.from != finish && c.to != finish;
        }

        bool PlaceObstacles(Work& w, model::Graph& keptGraph, Rng& rng)
        {
            int m = w.keptIds.Size();
            DynArray<bool> isBridge;
            isBridge.Resize(m);
            for (int i = 0; i < m; ++i) isBridge[i] = false;
            algo::DfsBridgesResult bridges = algo::DfsBridges(keptGraph);
            for (int i = 0; i < bridges.bridgeEdgeIds.Size(); ++i) isBridge[bridges.bridgeEdgeIds[i]] = true;

            DynArray<int> shuffled;
            for (int i = 0; i < m; ++i) shuffled.Push(i);
            for (int i = m - 1; i > 0; --i)
            {
                int j = rng.NextInt(0, i);
                int t = shuffled[i];
                shuffled[i] = shuffled[j];
                shuffled[j] = t;
            }

            for (int a = 0; a < m; ++a)
            {
                int first = shuffled[a];
                if (!ObstacleAllowed(w, isBridge, first))
                {
                    ++w.pruned;
                    continue;
                }
                int oil = w.keptIds[first];
                ++w.nodes;
                if (w.trace != nullptr) w.trace->Record(algo::TraceOp::MarkEdge, oil, kGenEdgeOilSpill, 0.0, algo::lines::generator::kObstacle);

                for (int b = a + 1; b < m; ++b)
                {
                    int second = shuffled[b];
                    const algo::CandidateEdge& e1 = w.candidates[oil];
                    const algo::CandidateEdge& e2 = w.candidates[w.keptIds[second]];
                    bool sharesVertex = (e1.from == e2.from || e1.from == e2.to || e1.to == e2.from || e1.to == e2.to);
                    if (!ObstacleAllowed(w, isBridge, second) || sharesVertex)
                    {
                        ++w.pruned;
                        continue;
                    }

                    ++w.nodes;
                    w.oilCandidate = oil;
                    w.blockCandidate = w.keptIds[second];
                    if (w.trace != nullptr)
                    {
                        w.trace->Record(algo::TraceOp::MarkEdge, w.blockCandidate, kGenEdgeRoadBlock, 0.0,
                            algo::lines::generator::kObstacle);
                    }
                    return true;
                }

                // No partner for this oil spill: undo it and try the next road.
                if (w.trace != nullptr)
                {
                    w.trace->Record(algo::TraceOp::MarkEdge, oil, EdgeStateCode(w, oil), 0.0, algo::lines::generator::kBacktrack);
                }
            }
            return false;
        }

        void ClearTrack(io::TrackData* data)
        {
            delete data->graph;
            data->graph = nullptr;
            data->name.Set("");
            data->mainRoute.Clear();
            for (int i = 0; i < data->detours.Size(); ++i) delete data->detours[i];
            data->detours.Clear();
            data->shortcutPreference.Clear();
            data->events.Clear();
        }

        void AddEvent(io::TrackData* data, int timeMs, const char* type, int target, int durationMs)
        {
            io::ScriptedEvent ev;
            ev.triggerTimeMs = timeMs;
            ev.type.Set(type);
            ev.target = target;
            ev.durationMs = durationMs;
            data->events.Push(ev);
        }

        // Builds the final track from a successful placement. Returns false
        // (with a reason) if no lap can be planned at all.
        bool BuildTrack(const Work& w, const GeneratorParams& p, int attempt, io::TrackData* out, GenAttempt* rec)
        {
            ClearTrack(out);
            model::Graph* g = new model::Graph();
            out->graph = g;

            for (int v = 0; v < w.n; ++v)
            {
                model::VertexType type = model::VertexType::Intersection;
                switch (w.role[v])
                {
                    case kGenRoleStart: type = model::VertexType::Start; break;
                    case kGenRoleFinish: type = model::VertexType::Finish; break;
                    case kGenRoleCheckpoint: type = model::VertexType::Checkpoint; break;
                    case kGenRoleFuel: type = model::VertexType::FuelStation; break;
                    case kGenRoleNitro: type = model::VertexType::NitroPickup; break;
                    default: type = w.leaf[v] ? model::VertexType::DeadEnd : model::VertexType::Intersection; break;
                }
                g->AddVertex(w.x[v], w.y[v], type, w.checkpointOrder[v]);
            }

            DynArray<int> finalOf;   // candidate id -> final edge id
            finalOf.Resize(w.candidates.Size());
            for (int i = 0; i < finalOf.Size(); ++i) finalOf[i] = -1;
            for (int k = 0; k < w.keptIds.Size(); ++k)
            {
                int id = w.keptIds[k];
                const algo::CandidateEdge& c = w.candidates[id];
                double d = c.weight * (1.0 + w.slack[id]);   // never shorter than the straight line
                finalOf[id] = g->AddEdge(c.from, c.to, d, d / kSpeed, d * 0.02, w.difficulty[id], w.shortcut[id], false);
                if (w.shortcut[id]) out->shortcutPreference.Push(finalOf[id]);
            }

            int start = w.slotVertex[kSlotStart];
            int finish = w.slotVertex[kSlotFinish];

            // Fuel balance: scale every road so one Balanced lap uses ~45
            // units. Scaling all fuel by one factor leaves the normalized
            // cost model -- hence every route -- unchanged.
            model::CostReferences refs = model::ComputeReferences(*g);
            algo::PlanResult lap = algo::PlanLap(*g, start, 0, finish, model::WeightsForPreset(model::Preset::Balanced),
                refs, algo::RouteAlgo::Dijkstra);
            if (!lap.found || lap.totalDistance <= 0.0)
            {
                Reject(rec, "no lap can be planned through the placed checkpoints");
                return false;
            }
            double fuelRate = kLapFuelUnits / lap.totalDistance;
            for (int e = 0; e < g->EdgeCount(); ++e)
            {
                model::Edge& edge = g->GetEdgeMutable(e);
                edge.fuelCost = edge.distance * fuelRate;
            }

            // Opponent 1's main route: the shortest-distance lap, then the
            // way back from Finish to Start (the loop closes on the Start
            // vertex itself, which is not repeated).
            model::Weights distance = model::WeightsForPreset(model::Preset::Distance);
            refs = model::ComputeReferences(*g);
            algo::PlanResult mainLap = algo::PlanLap(*g, start, 0, finish, distance, refs, algo::RouteAlgo::Dijkstra);
            algo::PathResult back = algo::Dijkstra(*g, finish, start, distance, refs);
            for (int i = 0; i < mainLap.vertexPath.Size(); ++i) out->mainRoute.Push(mainLap.vertexPath[i]);
            for (int i = 1; i + 1 < back.vertexPath.Size(); ++i) out->mainRoute.Push(back.vertexPath[i]);

            AddEvent(out, 15000, "OilSpill", finalOf[w.oilCandidate], 20000);
            AddEvent(out, 40000, "RoadBlock", finalOf[w.blockCandidate], 0);
            AddEvent(out, 55000, "RoadClear", finalOf[w.blockCandidate], 0);
            AddEvent(out, 70000, "NitroBooster", w.slotVertex[kSlotNitro1], 25000);

            char name[64];
            snprintf(name, sizeof(name), "Generated #%llu-%d", p.seed, attempt);
            out->name.Set(name);
            out->laps = p.laps;
            return true;
        }

        bool RunAttempt(const GeneratorParams& p, int attempt, GenAttempt* rec, algo::Trace* trace,
            model::Graph** outCandidates, io::TrackData* out)
        {
            Rng rng(p.seed * 7919ULL + static_cast<unsigned long long>(attempt) * 104729ULL + 1ULL);

            Work w;
            w.n = p.vertexCount;
            w.nodes = 0;
            w.pruned = 0;
            w.limitHit = false;
            w.trace = trace;
            w.oilCandidate = -1;
            w.blockCandidate = -1;
            for (int s = 0; s < kSlotCount; ++s) w.slotVertex[s] = -1;

            if (w.n < kSlotCount + 2)
            {
                char msg[160];
                snprintf(msg, sizeof(msg), "%d vertices cannot hold the %d roles plus a dead end", w.n, kSlotCount);
                Reject(rec, msg);
                return false;
            }

            // 1-2. Vertices and candidate roads.
            PlaceVertices(w, rng, p);
            BuildCandidates(w, rng);
            *outCandidates = BuildCandidateGraph(w);

            // 3. The candidate roads must connect everything first.
            algo::DfsComponentsResult components = algo::DfsComponents(**outCandidates);
            if (components.componentCount != 1)
            {
                char msg[160];
                snprintf(msg, sizeof(msg), "candidate roads split into %d disconnected groups; an MST would be a forest",
                    components.componentCount);
                Reject(rec, msg);
                return false;
            }

            // 4. Kruskal backbone.
            int traceStart = (trace != nullptr) ? trace->StepCount() : 0;
            algo::MstResult mst = algo::Kruskal(w.n, w.candidates.Data(), w.candidates.Size(), nullptr, trace);
            if (trace != nullptr) trace->SetLineFrom(traceStart, algo::lines::generator::kMst);

            w.degree.Resize(w.n);
            for (int i = 0; i < w.n; ++i) w.degree[i] = 0;
            for (int i = 0; i < mst.acceptedIds.Size(); ++i)
            {
                int id = mst.acceptedIds[i];
                w.kept[id] = true;
                w.inTree[id] = true;
                w.difficulty[id] = rng.NextInt(1, 2);
                ++w.degree[w.candidates[id].from];
                ++w.degree[w.candidates[id].to];
            }

            // 5. Extra roads for loops (shortest first), then 2 shortcuts.
            int target = 4 + rng.NextInt(0, 2);
            DynArray<int> extras;
            for (int i = 0; i < mst.rejectedIds.Size() && extras.Size() < target; ++i)
            {
                int id = mst.rejectedIds[i];
                const algo::CandidateEdge& c = w.candidates[id];
                if (w.degree[c.from] >= kMaxDegree || w.degree[c.to] >= kMaxDegree) continue;

                w.kept[id] = true;
                w.difficulty[id] = rng.NextInt(2, 3);
                ++w.degree[c.from];
                ++w.degree[c.to];
                extras.Push(id);
                if (trace != nullptr) trace->Record(algo::TraceOp::MarkEdge, id, kGenEdgeExtra, c.weight, algo::lines::generator::kExtraRoad);
            }
            if (extras.Size() < kExtraRoadsMin)
            {
                char msg[160];
                snprintf(msg, sizeof(msg), "only %d extra road(s) fit under the degree cap; loops and shortcuts need %d",
                    extras.Size(), kExtraRoadsMin);
                Reject(rec, msg);
                return false;
            }
            for (int k = extras.Size() - 2; k < extras.Size(); ++k)
            {
                int id = extras[k];
                w.shortcut[id] = true;
                w.difficulty[id] = 5;
                if (trace != nullptr) trace->Record(algo::TraceOp::MarkEdge, id, kGenEdgeShortcut, 0.0, algo::lines::generator::kExtraRoad);
            }

            for (int id = 0; id < w.candidates.Size(); ++id)
            {
                if (w.kept[id]) w.keptIds.Push(id);
            }

            // Road counts between every pair, leaves.
            model::Graph* keptGraph = BuildKeptGraph(w);
            w.hops.Resize(w.n * w.n);
            for (int s = 0; s < w.n; ++s)
            {
                algo::BfsResult bfs = algo::Bfs(*keptGraph, s, -1);
                for (int v = 0; v < w.n; ++v) w.hops[s * w.n + v] = bfs.distEdges[v];
            }
            w.leaf.Resize(w.n);
            int leaves = 0;
            for (int v = 0; v < w.n; ++v)
            {
                w.leaf[v] = (w.degree[v] == 1);
                if (w.leaf[v]) ++leaves;
            }
            if (leaves == 0)
            {
                delete keptGraph;
                Reject(rec, "every vertex has 2+ roads: no leaf is left to be the dead end");
                return false;
            }

            // 6. Backtracking placement.
            w.role.Resize(w.n);
            w.checkpointOrder.Resize(w.n);
            for (int v = 0; v < w.n; ++v)
            {
                w.role[v] = kGenRoleNone;
                w.checkpointOrder[v] = -1;
                w.order.Push(v);
            }
            for (int i = w.n - 1; i > 0; --i)
            {
                int j = rng.NextInt(0, i);
                int t = w.order[i];
                w.order[i] = w.order[j];
                w.order[j] = t;
            }

            bool placed = PlaceSlot(w, kSlotStart);
            rec->nodesExplored = w.nodes;
            rec->prunedBranches = w.pruned;
            if (!placed)
            {
                delete keptGraph;
                char msg[160];
                snprintf(msg, sizeof(msg), "backtracking found no placement (%lld tried, %lld pruned%s)",
                    w.nodes, w.pruned, w.limitHit ? ", search limit reached" : "");
                Reject(rec, msg);
                return false;
            }

            // 7. Obstacles.
            bool obstacles = PlaceObstacles(w, *keptGraph, rng);
            delete keptGraph;
            rec->nodesExplored = w.nodes;
            rec->prunedBranches = w.pruned;
            if (!obstacles)
            {
                Reject(rec, "no two non-bridge roads away from Start/Finish for the obstacles");
                return false;
            }

            // 8. Build, then 9. validate.
            if (!BuildTrack(w, p, attempt, out, rec)) return false;

            ValidationReport validation = ValidateTrack(*out->graph, p.laps);
            if (!validation.passed)
            {
                Reject(rec, validation.FirstFailure());
                return false;
            }

            rec->accepted = true;
            rec->reason[0] = '\0';
            return true;
        }
    }

    bool GenerateTrack(const GeneratorParams& params, io::TrackData* out, model::Graph** outCandidates,
        GenReport* report, algo::Trace* trace)
    {
        report->attempts.Clear();
        report->acceptedAttempt = -1;
        if (trace != nullptr) trace->Clear();

        for (int attempt = 0; attempt < params.maxAttempts; ++attempt)
        {
            GenAttempt rec;
            rec.attempt = attempt;
            rec.accepted = false;
            rec.reason[0] = '\0';
            rec.nodesExplored = 0;
            rec.prunedBranches = 0;

            algo::Trace attemptTrace;
            model::Graph* candidates = nullptr;
            io::TrackData built;
            bool ok = RunAttempt(params, attempt, &rec, (trace != nullptr) ? &attemptTrace : nullptr, &candidates, &built);
            report->attempts.Push(rec);

            bool describe = ok || attempt == params.maxAttempts - 1;
            if (describe)
            {
                if (trace != nullptr) trace->CopyFrom(attemptTrace);
                if (outCandidates != nullptr)
                {
                    delete *outCandidates;
                    *outCandidates = candidates;
                    candidates = nullptr;
                }
            }
            delete candidates;

            if (ok)
            {
                ClearTrack(out);
                out->graph = built.graph;
                built.graph = nullptr;
                out->name.Set(built.name.CStr());
                out->laps = built.laps;
                for (int i = 0; i < built.mainRoute.Size(); ++i) out->mainRoute.Push(built.mainRoute[i]);
                for (int i = 0; i < built.shortcutPreference.Size(); ++i) out->shortcutPreference.Push(built.shortcutPreference[i]);
                for (int i = 0; i < built.events.Size(); ++i) out->events.Push(built.events[i]);
                report->acceptedAttempt = attempt;
                return true;
            }
        }
        return false;
    }
}
