#pragma once
#include "api/SmartRaceApi.h"

namespace LabEventStatus
{
    inline bool Contains(ArrayView<int> ids, int id)
    {
        for (int i = 0; i < ids.count; ++i)
        {
            if (ids[i] == id)
            {
                return true;
            }
        }
        return false;
    }

    inline const char* Label(const LabFrame& frame, const LabEventView& event)
    {
        if (Contains(frame.rejectedIds, event.id))
        {
            return "CANCELLED";
        }
        if (Contains(frame.selectedIds, event.id))
        {
            return "APPLIED";
        }
        // RunEventQueue records one PushHeap for each Schedule, in insertion
        // sequence order starting at zero. The frame counter includes only
        // those pushes already reached, even after seeking backwards.
        if (event.sequence >= 0 && event.sequence < frame.heapPushes)
        {
            return "QUEUED";
        }
        return "NOT YET SCHEDULED";
    }
}
