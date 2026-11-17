#pragma once

#include "raylib.h"

namespace Ui
{
    const Color Background = { 15, 20, 30, 255 };
    const Color Panel = { 23, 31, 44, 255 };
    const Color Border = { 44, 57, 73, 255 };
    const Color Text = { 230, 237, 245, 255 };
    const Color Muted = { 151, 169, 189, 255 };
    const Color Accent = { 103, 229, 188, 255 };
    const Color Gold = { 249, 194, 97, 255 };
    const Color Danger = { 255, 132, 137, 255 };

    void Init();
    void Begin();
    void End();
    bool Button(int id, Rectangle bounds, const char* label, bool enabled = true, bool selected = false);
    int TakeAction();
    void ClearAction();
    void BeginClip(Rectangle bounds);
    void EndClip();
    void Label(float x, float y, const char* text, int size = 18, Color color = Text);
    void Card(Rectangle bounds);
    float Wrap(Rectangle bounds, const char* text, int size = 18, Color color = Muted);
    void Bar(Rectangle bounds, float fraction, Color color);
    bool Number(Rectangle bounds, const char* label, int* value, int min, int max);
    void TextBox(Rectangle bounds, char* text, int size, bool* editing);
}
