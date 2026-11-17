#include "Toast.h"
#include "Theme.h"
#include <cstdio>

void Toast::Show(const char* text, bool persistent)
{
    snprintf(m_text, sizeof(m_text), "%s", text);
    m_left = 7;
    m_persistent = persistent;
}

void Toast::Update(float dt)
{
    if (!m_persistent) m_left -= dt;
}

void Toast::Draw() const
{
    if (m_left <= 0 && !m_persistent) return;
    Rectangle r = { 20, static_cast<float>(GetScreenHeight() - 108), static_cast<float>(GetScreenWidth() - 40), 72 };
    DrawRectangleRec(r, { 64, 35, 44, 255 });
    Ui::Wrap({ r.x + 16, r.y + 12, r.width - 32, r.height - 16 }, m_text, 18, Ui::Danger);
}
