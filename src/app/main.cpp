#include "App.h"
#include "ui/Theme.h"

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1440, 900, "Smart Racing Championship");
    SetWindowMinSize(800, 600);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    Ui::Init();
    App app;
    app.Init();
    while (!WindowShouldClose() && !app.state.quit)
    {
        app.Update(GetFrameTime());
        BeginDrawing();
        ClearBackground(Ui::Background);
        app.Draw();
        EndDrawing();
    }
    app.Shutdown();
    CloseWindow();
    return 0;
}
