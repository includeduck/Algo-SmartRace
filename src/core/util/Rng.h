#pragma once

#include <cstdint>

// Rng: a small, fast, fully deterministic xorshift64* generator. We never
// use <random> or rand()/srand() — every randomized feature (track
// generation, scripted event streams, benchmark inputs) must reproduce
// exactly from its seed (Project.pdf: deterministic opponents/races).
class Rng
{
public:
    explicit Rng(uint64_t seed)
        : m_state(seed == 0 ? 0x9E3779B97F4A7C15ULL : seed)
    {
    }

    uint64_t NextU64()
    {
        m_state ^= m_state >> 12;
        m_state ^= m_state << 25;
        m_state ^= m_state >> 27;
        return m_state * 0x2545F4914F6CDD1DULL;
    }

    uint32_t NextU32()
    {
        return static_cast<uint32_t>(NextU64() >> 32);
    }

    // Inclusive range [lo, hi].
    int NextInt(int lo, int hi)
    {
        if (hi <= lo) return lo;
        uint32_t range = static_cast<uint32_t>(hi - lo + 1);
        return lo + static_cast<int>(NextU32() % range);
    }

    // [0, 1)
    double NextDouble()
    {
        return static_cast<double>(NextU64() >> 11) * (1.0 / 9007199254740992.0);
    }

private:
    uint64_t m_state;
};
