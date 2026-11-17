#pragma once

#include "TrackLoader.h"

namespace io
{
    const char* VertexTypeName(model::VertexType type);

    // Writes `data` in the .trk format TrackLoader reads (see the grammar at
    // the top of TrackLoader.cpp), so a generated track can be saved and then
    // loaded and raced like a hand-made one. Distances, times and fuel are
    // written to 4 decimals and coordinates to 3, so the reloaded track
    // still satisfies distance >= straight line. Returns false (with a
    // reason in errorMessage) if the file cannot be written.
    bool WriteTrackFile(const char* path, const TrackData& data, char* errorMessage, int errorMessageCapacity);
}
