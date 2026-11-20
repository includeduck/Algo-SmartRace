#include "TestFramework.h"

#include "../src/api/SmartRaceApi.h"

#include <cstdio>

// API-level regression tests for completeness findings F04 and F05
// (docs/gui-backend-handoff.md). The engine-level ones are in
// test_completeness.cpp; these live apart because the API's global types
// (Preset, ...) clash with namespace model's.

namespace
{
    bool HasText(const char* text, const char* needle)
    {
        for (int i = 0; text[i] != '\0'; ++i)
        {
            int k = 0;
            while (needle[k] != '\0' && text[i + k] == needle[k]) ++k;
            if (needle[k] == '\0') return true;
        }
        return false;
    }
}

TEST(F04_Api_RepairsAnInfeasibleCombination)
{
    // Through the API: DP alone would take both nitro detours (cheap in
    // time, 2 s benefit each); together they overrun the tank, so the one
    // adding the most fuel (tie: higher id, N2) is withdrawn.
    const char* path = "build/test_f04_pickups.trk";
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    CHECK(f != nullptr);
    if (f == nullptr) return;
    fprintf(f, "CIRCUIT 0 \"F04\"\nLAPS 2\n");
    fprintf(f, "VERTEX 0 0 0 Start\nVERTEX 1 100 0 Checkpoint 1\nVERTEX 2 200 0 Finish\n");
    fprintf(f, "VERTEX 3 50 40 NitroPickup\nVERTEX 4 150 40 NitroPickup\n");
    fprintf(f, "EDGE 0 0 1 100 2 10 1 0 0\nEDGE 1 1 2 100 2 10 1 0 0\n");
    fprintf(f, "EDGE 2 0 3 64 1.3 30 1 0 0\nEDGE 3 3 1 64 1.3 25 1 0 0\n");
    fprintf(f, "EDGE 4 1 4 64 1.3 30 1 0 0\nEDGE 5 4 2 64 1.3 25 1 0 0\n");
    fprintf(f, "EDGE 6 2 0 200 4 10 1 0 0\n");
    fclose(f);

    CHECK(ApiInit("data") == ApiStatus::Ok);
    Settings settings = ApiGetSettings();
    settings.startNitroCharges = 0;
    CHECK(ApiSetSettings(settings) == ApiStatus::Ok);
    CHECK(ApiLoadCircuitFile(path) == ApiStatus::Ok);

    PickupView view = ApiPlanPickups(Preset::Balanced, 1000);
    int chosen = 0;
    bool keptN1 = false;
    for (int i = 0; i < view.items.count; ++i)
    {
        if (view.items[i].chosenByDp) ++chosen;
        if (view.items[i].vertex == 3 && view.items[i].chosenByDp) keptN1 = true;
    }
    CHECK_EQ(chosen, 1);
    CHECK(keptN1);
    ApiShutdown();
}

TEST(F05_Api_ACircuitThatFailsValidation_CannotBeRaced)
{
    // Loads (well-formed), but Start and Finish are not connected: it can
    // be explored, and the validator says why, but a race is refused.
    const char* path = "build/test_f05_unplayable.trk";
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    CHECK(f != nullptr);
    if (f == nullptr) return;
    fprintf(f, "CIRCUIT 0 \"F05\"\nLAPS 2\nVERTEX 0 0 0 Start\nVERTEX 1 100 0 Finish\n"
        "VERTEX 2 50 50 Checkpoint 1\nEDGE 0 0 2 50 1 1 1 0 0\n");
    fclose(f);

    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuitFile(path) == ApiStatus::Ok);
    CHECK(ApiRaceStart() == ApiStatus::Infeasible);
    CHECK(HasText(ApiLastError(), "validation"));

    // The shipped circuits still validate and race.
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    CHECK(ApiRaceStart() == ApiStatus::Ok);
    ApiShutdown();
}
