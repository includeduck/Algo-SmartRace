#pragma once

class RaceClock
{
public:
    void Reset()
    {
        m_accumulator = 0;
    }
    int Accumulate(float dt, int speed)
    {
        if (dt < 0) dt = 0;
        if (dt > 0.25f) dt = 0.25f;
        m_accumulator += static_cast<double>(dt) * 1000.0 * speed;
        // Float frame deltas can land a few nanoseconds below a tick boundary.
        int ticks = static_cast<int>((m_accumulator + 0.00001) / 20.0);
        m_accumulator -= ticks * 20.0;
        if (m_accumulator < 0) m_accumulator = 0;
        return ticks;
    }
private:
    double m_accumulator = 0;
};
