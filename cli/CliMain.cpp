// smartrace_cli: headless driver and API usage sample.
//
// This is both a quick way to exercise the backend without the GUI and a
// runnable example for Rayyan of how each SmartRaceApi.h call is used.
#include "../src/api/SmartRaceApi.h"

#include <cstdio>

int main()
{
    ApiStatus status = ApiInit("data");
    if (status != ApiStatus::Ok)
    {
        printf("ApiInit failed: %s\n", ApiLastError());
        return 1;
    }

    printf("Circuits available: %d\n", ApiCircuitCount());
    for (int i = 0; i < ApiCircuitCount(); ++i)
    {
        printf("  [%d] %s\n", i, ApiCircuitName(i));
    }

    status = ApiLoadCircuit(0);
    if (status != ApiStatus::Ok)
    {
        printf("ApiLoadCircuit failed: %s\n", ApiLastError());
        return 1;
    }

    TrackInfo info = ApiGetTrackInfo();
    printf("\nLoaded track: %s\n  vertices=%d edges=%d laps=%d\n",
        info.name, info.vertexCount, info.edgeCount, info.laps);

    GraphView graph = ApiGetGraph();
    for (int i = 0; i < graph.vertices.count; ++i)
    {
        const VertexView& v = graph.vertices[i];
        printf("  V%d (%.1f, %.1f) type=%d cpOrder=%d\n",
            v.id, v.x, v.y, static_cast<int>(v.type), v.checkpointOrder);
    }

    status = ApiRaceStart();
    if (status != ApiStatus::Ok)
    {
        printf("ApiRaceStart failed: %s\n", ApiLastError());
        return 1;
    }

    printf("\nStub race running (10 ticks of 1 simulated second each)...\n");
    for (int step = 0; step < 10; ++step)
    {
        ApiRaceStep(50);   // 50 ticks * 20ms = 1 simulated second
        RaceSnapshot snap = ApiRaceSnapshot();

        printf("  t=%5dms ", snap.timeMs);
        for (int i = 0; i < snap.racers.count; ++i)
        {
            const RacerView& r = snap.racers[i];
            printf("| %-8s (%6.1f,%6.1f) ", r.name, r.x, r.y);
        }
        printf("\n");
    }

    ApiShutdown();
    return 0;
}
