#pragma once
#include "api/SmartRaceApi.h"
#include "raylib.h"

namespace LabRenderers
{
    void Array(Rectangle bounds, const LabFrame& frame, bool racerIds);
    void Table(Rectangle bounds, const LabFrame& frame, int row, int column);
    void Events(Rectangle bounds, const LabFrame& frame, ArrayView<LabEventView> events, int offset);
}
