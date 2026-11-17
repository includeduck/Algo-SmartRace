#include "TrackWriter.h"

#include <cstdio>

namespace io
{
    const char* VertexTypeName(model::VertexType type)
    {
        switch (type)
        {
            case model::VertexType::Start: return "Start";
            case model::VertexType::Finish: return "Finish";
            case model::VertexType::Checkpoint: return "Checkpoint";
            case model::VertexType::FuelStation: return "FuelStation";
            case model::VertexType::NitroPickup: return "NitroPickup";
            case model::VertexType::DeadEnd: return "DeadEnd";
            case model::VertexType::Intersection:
            default: return "Intersection";
        }
    }

    bool WriteTrackFile(const char* path, const TrackData& data, char* errorMessage, int errorMessageCapacity)
    {
        if (data.graph == nullptr)
        {
            String::StrCopy(errorMessage, errorMessageCapacity, "no track to write");
            return false;
        }

        FILE* file = fopen(path, "w");
        if (file == nullptr)
        {
            String::StrCopy(errorMessage, errorMessageCapacity, "could not open the file for writing");
            return false;
        }

        const model::Graph& g = *data.graph;
        fprintf(file, "CIRCUIT 0 \"%s\"\n", data.name.CStr());
        fprintf(file, "LAPS %d\n\n", data.laps);

        fprintf(file, "# VERTEX id x y type [cpOrder]\n");
        for (int i = 0; i < g.VertexCount(); ++i)
        {
            const model::Vertex& v = g.GetVertex(i);
            fprintf(file, "VERTEX %d %.3f %.3f %s", v.id, v.x, v.y, VertexTypeName(v.type));
            if (v.checkpointOrder >= 0) fprintf(file, " %d", v.checkpointOrder);
            fprintf(file, "\n");
        }

        fprintf(file, "\n# EDGE id from to distance baseTime fuelCost difficulty shortcut directed [Blocked]\n");
        for (int i = 0; i < g.EdgeCount(); ++i)
        {
            const model::Edge& e = g.GetEdge(i);
            fprintf(file, "EDGE %d %d %d %.4f %.4f %.4f %d %d %d%s\n", e.id, e.from, e.to,
                e.distance, e.baseTime, e.fuelCost, e.difficulty, e.isShortcut ? 1 : 0, e.directed ? 1 : 0,
                (e.baseStatus == model::EdgeStatus::Blocked) ? " Blocked" : "");
        }

        if (data.mainRoute.Size() > 0)
        {
            fprintf(file, "\nMAINROUTE");
            for (int i = 0; i < data.mainRoute.Size(); ++i) fprintf(file, " %d", data.mainRoute[i]);
            fprintf(file, "\n");
        }

        for (int i = 0; i < data.detours.Size(); ++i)
        {
            const DetourRule* rule = data.detours[i];
            fprintf(file, "DETOUR %d", rule->blockedEdgeId);
            for (int k = 0; k < rule->viaVertices.Size(); ++k) fprintf(file, " %d", rule->viaVertices[k]);
            fprintf(file, "\n");
        }

        for (int i = 0; i < data.shortcutPreference.Size(); ++i)
        {
            fprintf(file, "SHORTCUT %d\n", data.shortcutPreference[i]);
        }

        for (int i = 0; i < data.events.Size(); ++i)
        {
            const ScriptedEvent& ev = data.events[i];
            fprintf(file, "EVENT %d %s %d %d\n", ev.triggerTimeMs, ev.type.CStr(), ev.target, ev.durationMs);
        }

        bool ok = (ferror(file) == 0);
        fclose(file);
        if (!ok) String::StrCopy(errorMessage, errorMessageCapacity, "write error");
        return ok;
    }
}
