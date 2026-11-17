#include "WindowCanvas.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

bool WindowCanvas::ClientSize(void* handle, int* width, int* height)
{
    RECT bounds = {};
    if (!GetClientRect(static_cast<HWND>(handle), &bounds))
    {
        return false;
    }
    *width = bounds.right - bounds.left;
    *height = bounds.bottom - bounds.top;
    return *width > 0 && *height > 0;
}
