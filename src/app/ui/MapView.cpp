#include "MapView.h"
#include "Theme.h"
#include <cmath>
#include <cstdio>

namespace
{
    bool Contains(ArrayView<int> values, int id)
    {
        for (int i = 0; i < values.count; ++i)
        {
            if (values[i] == id) return true;
        }
        return false;
    }

    const VertexView* Vertex(GraphView g, int id)
    {
        for (int i = 0; i < g.vertices.count; ++i)
        {
            if (g.vertices[i].id == id) return &g.vertices[i];
        }
        return nullptr;
    }
}

float MapView::Scale() const
{
    float sx = (bounds.width - 80) / m_spanX;
    float sy = (bounds.height - 80) / m_spanY;
    return (sx < sy ? sx : sy) * m_zoom;
}

void MapView::Fit(GraphView graph)
{
    if (graph.vertices.count == 0) return;
    double x0 = graph.vertices[0].x, x1 = x0, y0 = graph.vertices[0].y, y1 = y0;
    for (int i = 0; i < graph.vertices.count; ++i)
    {
        const VertexView& v = graph.vertices[i];
        if (v.x < x0) x0 = v.x;
        if (v.x > x1) x1 = v.x;
        if (v.y < y0) y0 = v.y;
        if (v.y > y1) y1 = v.y;
    }
    m_center = { static_cast<float>((x0 + x1) / 2), static_cast<float>((y0 + y1) / 2) };
    m_spanX = static_cast<float>(x1 - x0 + 1);
    m_spanY = static_cast<float>(y1 - y0 + 1);
    m_zoom = 1;
    m_pan = {};
}

Vector2 MapView::Point(double x, double y) const
{
    float scale = Scale();
    return { bounds.x + bounds.width / 2 + m_pan.x + (static_cast<float>(x) - m_center.x) * scale,
        bounds.y + bounds.height / 2 + m_pan.y + (static_cast<float>(y) - m_center.y) * scale };
}

void MapView::Update()
{
    if (!CheckCollisionPointRec(GetMousePosition(), bounds)) return;
    m_zoom *= 1 + GetMouseWheelMove() * 0.12f;
    if (m_zoom < 0.35f) m_zoom = 0.35f;
    if (m_zoom > 5) m_zoom = 5;
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
        Vector2 delta = GetMouseDelta();
        m_pan.x += delta.x;
        m_pan.y += delta.y;
    }
    if (IsKeyPressed(KEY_F))
    {
        m_zoom = 1;
        m_pan = {};
    }
}

int MapView::HitVertex(GraphView graph) const
{
    if (!CheckCollisionPointRec(GetMousePosition(), bounds)) return -1;
    for (int i = 0; i < graph.vertices.count; ++i)
    {
        const VertexView& v = graph.vertices[i];
        if (CheckCollisionPointCircle(GetMousePosition(), Point(v.x, v.y), 13)) return v.id;
    }
    return -1;
}

int MapView::HitEdge(GraphView graph) const
{
    if (!CheckCollisionPointRec(GetMousePosition(), bounds)) return -1;
    for (int i = 0; i < graph.edges.count; ++i)
    {
        const EdgeView& e = graph.edges[i];
        const VertexView* a = Vertex(graph, e.from);
        const VertexView* b = Vertex(graph, e.to);
        if (a && b && CheckCollisionPointLine(GetMousePosition(), Point(a->x, a->y), Point(b->x, b->y), 8)) return e.id;
    }
    return -1;
}

void MapView::Draw(GraphView graph, ArrayView<int> edges, ArrayView<int> vertices, const LabFrame* frame, bool generator) const
{
    Ui::Card(bounds);
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y), static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    for (int x = static_cast<int>(bounds.x); x < bounds.x + bounds.width; x += 40)
    {
        DrawLine(x, static_cast<int>(bounds.y), x, static_cast<int>(bounds.y + bounds.height), { 29, 38, 52, 255 });
    }
    for (int y = static_cast<int>(bounds.y); y < bounds.y + bounds.height; y += 40)
    {
        DrawLine(static_cast<int>(bounds.x), y, static_cast<int>(bounds.x + bounds.width), y, { 29, 38, 52, 255 });
    }
    for (int i = 0; i < graph.edges.count; ++i)
    {
        const EdgeView& e = graph.edges[i];
        const VertexView* a = Vertex(graph, e.from);
        const VertexView* b = Vertex(graph, e.to);
        if (!a || !b) continue;
        Vector2 p = Point(a->x, a->y), q = Point(b->x, b->y);
        Color color = e.isShortcut ? Color{ 120, 137, 210, 255 } : Color{ 75, 94, 117, 255 };
        if (e.status == EdgeStatus::Restricted) color = Ui::Gold;
        if (e.status == EdgeStatus::Blocked) color = Ui::Danger;
        bool highlight = Contains(edges, e.id);
        if (frame && i < frame->edgeState.count && frame->edgeState[i] > 0)
        {
            int state = frame->edgeState[i];
            color = state == 3 || state == 7 ? Ui::Danger : (state == 1 ? Ui::Gold : Ui::Accent);
            highlight = true;
        }
        if (highlight) color = e.status == EdgeStatus::Blocked ? Ui::Danger : Ui::Accent;
        DrawLineEx(p, q, highlight ? 6.0f : 3.0f, color);
        Vector2 mid = { (p.x + q.x) / 2, (p.y + q.y) / 2 };
        if (e.directed)
        {
            float angle = atan2f(q.y - p.y, q.x - p.x);
            Vector2 l = { mid.x - 10 * cosf(angle - 0.5f), mid.y - 10 * sinf(angle - 0.5f) };
            Vector2 r = { mid.x - 10 * cosf(angle + 0.5f), mid.y - 10 * sinf(angle + 0.5f) };
            DrawLineEx(mid, l, 2, color);
            DrawLineEx(mid, r, 2, color);
        }
        if (e.status != EdgeStatus::Open) Ui::Label(mid.x + 5, mid.y + 3, e.status == EdgeStatus::Blocked ? "X" : "!", 17, color);
    }
    for (int i = 0; i < graph.vertices.count; ++i)
    {
        const VertexView& v = graph.vertices[i];
        Vector2 p = Point(v.x, v.y);
        Color color = Ui::Muted;
        if (v.checkpointOrder > 0 || v.type == VertexType::Start || v.type == VertexType::Finish) color = Ui::Accent;
        if (v.type == VertexType::FuelStation) color = Ui::Gold;
        if (v.type == VertexType::NitroPickup) color = { 150, 164, 255, 255 };
        if (!v.active) color = Ui::Border;
        if (Contains(vertices, v.id)) color = Ui::Gold;
        if (frame && i < frame->vertexState.count && frame->vertexState[i] > 0) color = Ui::Gold;
        DrawCircleV(p, 10, Ui::Background);
        DrawCircleLinesV(p, 9, color);
        char label[48];
        if (generator && frame && i < frame->vertexState.count)
        {
            snprintf(label, sizeof(label), "V%d [%d]", v.id, frame->vertexState[i]);
        }
        else if (v.checkpointOrder > 0) snprintf(label, sizeof(label), "CP%d", v.checkpointOrder);
        else if (v.type == VertexType::Start) snprintf(label, sizeof(label), "START");
        else if (v.type == VertexType::Finish) snprintf(label, sizeof(label), "FINISH");
        else if (v.type == VertexType::FuelStation) snprintf(label, sizeof(label), "F%d%s", v.id, v.active ? "" : " OFF");
        else if (v.type == VertexType::NitroPickup) snprintf(label, sizeof(label), "N%d%s", v.id, v.active ? "" : " OFF");
        else snprintf(label, sizeof(label), "V%d", v.id);
        Ui::Label(p.x + 12, p.y - 9, label, 16, color);
    }
    Ui::Label(bounds.x + 14, bounds.y + 12, "SCROLL zoom  /  MIDDLE drag  /  F fit", 16, Ui::Muted);
    EndScissorMode();
}

void MapView::Cars(RaceSnapshot snapshot) const
{
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y), static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    for (int i = 0; i < snapshot.racers.count; ++i)
    {
        const RacerView& r = snapshot.racers[i];
        Vector2 p = Point(r.x, r.y);
        Color color = r.isHuman ? Ui::Accent : Ui::Gold;
        DrawRectanglePro({ p.x, p.y, 24, 13 }, { 12, 6.5f }, static_cast<float>(r.heading * 180 / PI), color);
        char label[32];
        snprintf(label, sizeof(label), "%d %s", r.livePosition, r.name);
        Ui::Label(p.x + 14, p.y - 22 - (i % 3) * 15.0f, label, 15, color);
    }
    EndScissorMode();
}
