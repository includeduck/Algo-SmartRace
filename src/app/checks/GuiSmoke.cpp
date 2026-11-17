#include "App.h"
#include "ui/Theme.h"
#include "rlgl.h"
#include <cstdio>

namespace
{
    int g_failures = 0;
    int g_checks = 0;

    void Check(bool pass, const char* name)
    {
        ++g_checks;
        printf("%s %s\n", pass ? "PASS" : "FAIL", name);
        if (!pass)
        {
            ++g_failures;
        }
    }

    void Frame(App& app)
    {
        app.Update(0);
        BeginDrawing();
        ClearBackground(Ui::Background);
        app.Draw();
        EndDrawing();
    }

    void Action(App& app, int id)
    {
        Ui::ClearAction();
        Ui::PostAction(id);
        app.Update(0);
    }

    void Capture(App& app, const char* name)
    {
        for (int i = 0; i < 3; ++i)
        {
            Frame(app);
        }
        char path[256];
        snprintf(path, sizeof(path), "report/screenshots/%s.png", name);
        // TakeScreenshot strips directory components in raylib 5.5.
        // Export the real framebuffer explicitly to the report directory.
        Image screenshot = { rlReadScreenPixels(Ui::CanvasWidth(), Ui::CanvasHeight()), Ui::CanvasWidth(), Ui::CanvasHeight(), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        Color corner = GetImageColor(screenshot, screenshot.width - 5, 5);
        Check(corner.r == Ui::Panel.r && corner.g == Ui::Panel.g && corner.b == Ui::Panel.b, "Canvas fills the actual framebuffer after DPI resize");
        ExportImage(screenshot, path);
        UnloadImage(screenshot);
        Check(FileExists(path), name);
    }

    void SpeedUp(App& app)
    {
        for (int i = 0; i < 3; ++i)
        {
            Action(app, 6);
        }
    }

    void FinishRace(App& app)
    {
        for (int i = 0; i < 1200 && app.Current() == ScreenId::Race; ++i)
        {
            app.Update(0.25f);
            if (i % 100 == 0)
            {
                Frame(app);
            }
        }
        Check(app.Current() == ScreenId::Results && ApiRaceOver(), "Race reaches results through the screen update loop");
    }
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(1600, 900, "SmartRace automated GUI smoke test");
    SetTargetFPS(60);
    Ui::Init();
    MakeDirectory("report/screenshots");
    App app;
    app.Init();
    Check(app.state.dataReady, "Real circuit data loads");
    Capture(app, "01_main_menu");
    Action(app, 1);
    Check(app.Current() == ScreenId::Explorer, "Championship menu opens Explorer");
    Action(app, 13);
    Capture(app, "02_track_explorer");
    Action(app, 40);
    Action(app, 50);
    Check(app.Current() == ScreenId::Explorer, "Applying an ordered route preserves the screen/session");
    Action(app, 21);
    Check(app.Current() == ScreenId::Race, "Explorer starts the race");
    SpeedUp(app);
    for (int i = 0; i < 22 && app.Current() == ScreenId::Race; ++i)
    {
        app.Update(0.25f);
    }
    Action(app, 2);
    int frozen = ApiRaceSnapshot().timeMs;
    Check(frozen >= 40000, "Race advanced through the authored Harbor closure");
    Capture(app, "03_racing");
    Action(app, 14);
    Check(app.Current() == ScreenId::Explorer, "Live route planner opens without restarting");
    Check(ApiRaceSnapshot().timeMs == frozen, "Explorer freezes race time");
    Action(app, 40);
    Action(app, 50);
    Action(app, 24);
    Check(app.Current() == ScreenId::Race && ApiRaceSnapshot().timeMs == frozen, "Live plan selection resumes the same race");
    Action(app, 5);
    Check(app.Current() == ScreenId::Lab, "Race opens Lab");
    for (int module = 0; module < 13; ++module)
    {
        if (module == 6)
        {
            Action(app, 12);
        }
        Action(app, 2);
        Check(ApiLabStepCount() > 0, "Lab module exposes real trace frames");
        Action(app, 7);
        if (module == 2)
        {
            Capture(app, "04_lab_graph");
        }
        if (module == 5)
        {
            GeneratorReportView report = ApiLabGeneratorReport();
            Check(report.attempts.count >= 2 && !report.attempts[0].accepted && report.attempts[1].accepted && report.acceptedAttempt == 1, "Seed 2 rejects the first generated circuit and accepts the next attempt");
            Capture(app, "05_lab_generator");
        }
        if (module == 6)
        {
            Capture(app, "06_lab_table");
        }
        if (module == 8)
        {
            Capture(app, "13_live_resource_infeasible");
        }
        if (module == 9)
        {
            Capture(app, "07_lab_array");
        }
        if (module == 12)
        {
            Capture(app, "08_lab_events");
        }
        Action(app, 3);
        Check(ApiLabFrame(-1).step == -1, "Lab initial seek remains valid");
        if (module == 6)
        {
            Action(app, 12);
        }
        if (module < 12)
        {
            Action(app, 11);
        }
    }
    Action(app, 1);
    Check(app.Current() == ScreenId::Race && ApiRaceSnapshot().timeMs == frozen, "Lab Back preserves the race clock");
    app.Update(0.25f);
    Check(ApiRaceSnapshot().timeMs == frozen, "Race pause survives the Lab round trip");
    Action(app, 2);
    FinishRace(app);
    Capture(app, "09_race_results");
    Action(app, 3);
    Check(ApiChampionshipGetStatus().racesCompleted == 1, "Results records one championship race");
    for (int race = 1; race < 3; ++race)
    {
        Action(app, 2);
        Action(app, 21);
        SpeedUp(app);
        FinishRace(app);
        Action(app, 3);
        Check(ApiChampionshipGetStatus().racesCompleted == race + 1, "Continue advances exactly once");
    }
    Check(ApiChampionshipGetStatus().complete, "Three races declare a champion");
    Capture(app, "10_championship");
    Action(app, 3);
    Check(ApiChampionshipGetStatus().racesCompleted == 3, "Repeated old Continue action cannot award another race");
    SetWindowSize(800, 600);
    for (int i = 0; i < 10; ++i)
    {
        Frame(app);
    }
    printf("Resize: screen %dx%d render %dx%d DPI %.2f %.2f\n", GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight(), GetWindowScaleDPI().x, GetWindowScaleDPI().y);
    app.Go(ScreenId::Explorer);
    Capture(app, "11_explorer_800x600");
    app.Go(ScreenId::Settings);
    Capture(app, "12_settings_800x600");
    app.Shutdown();
    CloseWindow();
    printf("GUI rendering smoke: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
