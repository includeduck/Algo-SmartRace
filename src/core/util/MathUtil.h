#pragma once

#include <cmath>

// Small hand-written replacements for <algorithm>'s std::min/std::max/
// std::swap and a couple of extras, so nothing in the project needs to
// include <algorithm>.
namespace MathUtil
{
    template <typename T>
    T Min(const T& a, const T& b)
    {
        if (a < b) return a;
        return b;
    }

    template <typename T>
    T Max(const T& a, const T& b)
    {
        if (a > b) return a;
        return b;
    }

    template <typename T>
    void Swap(T& a, T& b)
    {
        T temp = a;
        a = b;
        b = temp;
    }

    template <typename T>
    T Clamp(const T& value, const T& lo, const T& hi)
    {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
    }

    template <typename T>
    T Abs(const T& value)
    {
        if (value < static_cast<T>(0)) return -value;
        return value;
    }

    inline double Dist2D(double x1, double y1, double x2, double y2)
    {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return sqrt(dx * dx + dy * dy);
    }
}
