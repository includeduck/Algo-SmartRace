#include "LabRenderers.h"
#include "LabEventStatus.h"
#include "Theme.h"
#include <cstdio>

void LabRenderers::Array(Rectangle r, const LabFrame& f, bool racerIds)
{
    Ui::Card(r);
    if (f.arrayValues.count == 0)
    {
        Ui::Label(r.x + 16, r.y + 20, "No array values.", 18, Ui::Muted);
        return;
    }
    Ui::Label(r.x + 16, r.y + 16, racerIds ? "Racer IDs are identities; order uses championship records." : "Array values / highlighted operation operands", 16, Ui::Muted);
    int maximum = 1;
    for (int i = 0; i < f.arrayValues.count; ++i)
    {
        if (f.arrayValues[i] > maximum) maximum = f.arrayValues[i];
    }
    float w = (r.width - 32) / f.arrayValues.count;
    for (int i = 0; i < f.arrayValues.count; ++i)
    {
        float height = racerIds ? r.height * 0.45f : (r.height - 90) * f.arrayValues[i] / maximum;
        Color color = i == f.a || i == f.b ? Ui::Gold : Ui::Accent;
        Rectangle bar = { r.x + 16 + i * w, r.y + r.height - 38 - height, w > 5 ? w - 3 : w, height };
        DrawRectangleRec(bar, color);
        if (w >= 22)
        {
            char label[32];
            snprintf(label, sizeof(label), "%d", f.arrayValues[i]);
            Ui::Label(bar.x, r.y + r.height - 28, label, 15);
        }
    }
}

void LabRenderers::Table(Rectangle r, const LabFrame& f, int row, int column)
{
    Ui::Card(r);
    if (f.tableRows == 0 || f.tableCols == 0)
    {
        Ui::Wrap({ r.x + 16, r.y + 20, r.width - 32, r.height - 40 }, "Greedy has no DP table. See selected and rejected item IDs with the result and counters.", 19);
        return;
    }
    int visibleCols = static_cast<int>((r.width - 54) / 64);
    int visibleRows = static_cast<int>((r.height - 50) / 30);
    for (int c = 0; c < visibleCols && column + c < f.tableCols; ++c)
    {
        char text[32];
        snprintf(text, sizeof(text), "%d", column + c);
        Ui::Label(r.x + 54 + c * 64, r.y + 12, text, 15, Ui::Gold);
    }
    for (int i = 0; i < visibleRows && row + i < f.tableRows; ++i)
    {
        char text[32];
        snprintf(text, sizeof(text), "%d", row + i);
        Ui::Label(r.x + 12, r.y + 48 + i * 30, text, 15, Ui::Gold);
        for (int c = 0; c < visibleCols && column + c < f.tableCols; ++c)
        {
            int index = (row + i) * f.tableCols + column + c;
            if (index < 0 || index >= f.tableCells.count) continue;
            int value = f.tableCells[index];
            if (value < 0) snprintf(text, sizeof(text), ".");
            else if (value >= 1000000000) snprintf(text, sizeof(text), "INF");
            else snprintf(text, sizeof(text), "%d", value);
            bool active = f.a == row + i && f.b == column + c;
            Ui::Label(r.x + 54 + c * 64, r.y + 48 + i * 30, text, 15, active ? Ui::Accent : Ui::Muted);
        }
    }
}

void LabRenderers::Events(Rectangle r, const LabFrame& f, ArrayView<LabEventView> events, int offset)
{
    Ui::Card(r);
    Ui::Label(r.x + 16, r.y + 16, "HEAP ORDER / time, priority, insertion sequence", 16, Ui::Accent);
    int visible = static_cast<int>((r.height - 64) / 46);
    for (int i = offset; i < events.count && i < offset + visible; ++i)
    {
        const LabEventView& e = events[i];
        const char* status = LabEventStatus::Label(f, e);
        char line[200];
        snprintf(line, sizeof(line), "#%d %s / %dms, P%d, seq%d / %s", e.id, e.type, e.triggerTimeMs, e.priority, e.sequence, status);
        Ui::Wrap({ r.x + 16, r.y + 52 + (i - offset) * 46, r.width - 32, 42 }, line, 15, LabEventStatus::Contains(f.rejectedIds, e.id) ? Ui::Danger : Ui::Text);
    }
}
