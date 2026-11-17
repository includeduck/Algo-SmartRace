#include "Theme.h"
#include "raygui.h"
#include "rlgl.h"
#include "WindowCanvas.h"
#include <cstdio>
#include <cstring>

namespace
{
    int g_action = 0;
    int g_focus = 0;
    int g_count = 0;
    int g_previousCount = 0;
    bool g_clipped = false;
    Rectangle g_clip = {};
    int g_canvasWidth = 1;
    int g_canvasHeight = 1;
}

void Ui::Init()
{
    GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, ColorToInt(Text));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, ColorToInt(Panel));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, ColorToInt(Border));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, ColorToInt(Accent));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, ColorToInt(Border));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, ColorToInt(Accent));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, ColorToInt(Background));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, ColorToInt(Accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, ColorToInt(Accent));
}

void Ui::Begin()
{
    // raylib 5.5 GLFW resets its viewport to logical dimensions on resize.
    // Keep our windowed canvas and clips in the same DPI coordinate system.
    g_canvasWidth = GetRenderWidth();
    g_canvasHeight = GetRenderHeight();
    WindowCanvas::ClientSize(GetWindowHandle(), &g_canvasWidth, &g_canvasHeight);
    SetMouseScale(static_cast<float>(GetScreenWidth()) / g_canvasWidth, static_cast<float>(GetScreenHeight()) / g_canvasHeight);
    rlViewport(0, 0, g_canvasWidth, g_canvasHeight);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, GetScreenWidth(), GetScreenHeight(), 0, 0, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    g_count = 0;
    if (IsKeyPressed(KEY_TAB) && g_previousCount > 0)
    {
        g_focus = (g_focus + (IsKeyDown(KEY_LEFT_SHIFT) ? g_previousCount - 1 : 1)) % g_previousCount;
    }
}

void Ui::End()
{
    g_previousCount = g_count;
    if (g_focus >= g_count) g_focus = 0;
}

bool Ui::Button(int id, Rectangle bounds, const char* label, bool enabled, bool selected)
{
    bool visible = !g_clipped || (bounds.y >= g_clip.y && bounds.y + bounds.height <= g_clip.y + g_clip.height);
    if (!visible) return false;
    enabled = enabled && visible;
    bool lockMouse = g_clipped && !CheckCollisionPointRec(GetMousePosition(), g_clip);
    if (lockMouse) GuiLock();
    int index = enabled ? g_count++ : -1;
    if (!enabled) GuiDisable();
    if (selected && enabled) GuiSetState(STATE_PRESSED);
    bool pressed = GuiButton(bounds, label) != 0;
    GuiSetState(STATE_NORMAL);
    GuiEnable();
    if (lockMouse) GuiUnlock();
    if (enabled && index == g_focus)
    {
        DrawRectangleLinesEx({ bounds.x - 3, bounds.y - 3, bounds.width + 6, bounds.height + 6 }, 1, Accent);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) pressed = true;
    }
    if (pressed && enabled && g_action == 0) g_action = id;
    if (pressed && enabled) g_focus = index;
    return pressed && enabled;
}

int Ui::CanvasWidth()
{
    return g_canvasWidth;
}

int Ui::CanvasHeight()
{
    return g_canvasHeight;
}

void Ui::Scissor(int x, int y, int width, int height)
{
    Vector2 dpi = { static_cast<float>(g_canvasWidth) / GetScreenWidth(), static_cast<float>(g_canvasHeight) / GetScreenHeight() };
    rlDrawRenderBatchActive();
    rlEnableScissorTest();
    rlScissor(static_cast<int>(x * dpi.x), static_cast<int>((GetScreenHeight() - y - height) * dpi.y),
        static_cast<int>(width * dpi.x), static_cast<int>(height * dpi.y));
}

void Ui::BeginClip(Rectangle bounds)
{
    g_clipped = true;
    g_clip = bounds;
    Ui::Scissor(static_cast<int>(bounds.x), static_cast<int>(bounds.y), static_cast<int>(bounds.width), static_cast<int>(bounds.height));
}

void Ui::EndClip()
{
    EndScissorMode();
    g_clipped = false;
}

int Ui::TakeAction()
{
    int action = g_action;
    g_action = 0;
    return action;
}

void Ui::PostAction(int id)
{
    if (g_action == 0) g_action = id;
}

void Ui::ClearAction()
{
    g_action = 0;
    g_focus = 0;
}

void Ui::Label(float x, float y, const char* text, int size, Color color)
{
    DrawText(text, static_cast<int>(x), static_cast<int>(y), size, color);
}

void Ui::Card(Rectangle bounds)
{
    DrawRectangleRounded(bounds, 0.035f, 6, Panel);
    DrawRectangleRoundedLinesEx(bounds, 0.035f, 6, 1, Border);
}

float Ui::Wrap(Rectangle bounds, const char* text, int size, Color color)
{
    char line[512] = {};
    int len = 0;
    float y = bounds.y;
    for (const char* p = text; ; ++p)
    {
        char c = *p;
        bool flush = c == '\n' || c == '\0';
        if (!flush && len < 510)
        {
            line[len++] = c;
            line[len] = '\0';
            if (MeasureText(line, size) > bounds.width)
            {
                int split = len - 1;
                while (split > 0 && line[split] != ' ') --split;
                if (split == 0) split = len - 1;
                char rest[512];
                snprintf(rest, sizeof(rest), "%s", line + split + (line[split] == ' ' ? 1 : 0));
                line[split] = '\0';
                if (y + size <= bounds.y + bounds.height) Label(bounds.x, y, line, size, color);
                y += size + 8;
                snprintf(line, sizeof(line), "%s", rest);
                len = static_cast<int>(strlen(line));
            }
        }
        if (flush)
        {
            if (y + size <= bounds.y + bounds.height) Label(bounds.x, y, line, size, color);
            y += size + 8;
            len = 0;
            line[0] = '\0';
            if (c == '\0') break;
        }
    }
    return y;
}

void Ui::Bar(Rectangle bounds, float fraction, Color color)
{
    DrawRectangleRec(bounds, Border);
    if (fraction < 0) fraction = 0;
    if (fraction > 1) fraction = 1;
    bounds.width *= fraction;
    DrawRectangleRec(bounds, color);
}

bool Ui::Number(Rectangle bounds, const char* label, int* value, int min, int max)
{
    bool visible = !g_clipped || (bounds.y >= g_clip.y && bounds.y + bounds.height <= g_clip.y + g_clip.height);
    bool lockMouse = g_clipped && !CheckCollisionPointRec(GetMousePosition(), g_clip);
    if (!visible) return false;
    if (lockMouse) GuiLock();
    int index = visible ? g_count++ : -1;
    if (visible && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds)) g_focus = index;
    GuiSpinner(bounds, label, value, min, max, false);
    if (visible && index == g_focus)
    {
        DrawRectangleLinesEx(bounds, 1, Accent);
        if (IsKeyPressed(KEY_LEFT)) --*value;
        if (IsKeyPressed(KEY_RIGHT)) ++*value;
    }
    if (lockMouse) GuiUnlock();
    GuiEnable();
    if (*value < min) *value = min;
    if (*value > max) *value = max;
    return true;
}

void Ui::TextBox(Rectangle bounds, char* text, int size, bool* editing)
{
    bool visible = !g_clipped || (bounds.y >= g_clip.y && bounds.y + bounds.height <= g_clip.y + g_clip.height);
    if (!visible)
    {
        *editing = false;
        return;
    }
    int index = g_count++;
    if (IsKeyPressed(KEY_TAB)) *editing = false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds)) g_focus = index;
    bool toggle = GuiTextBox(bounds, text, size, *editing) != 0;
    if (index == g_focus)
    {
        DrawRectangleLinesEx(bounds, 1, Accent);
        if (!*editing && IsKeyPressed(KEY_ENTER)) toggle = true;
    }
    if (toggle) *editing = !*editing;
}
