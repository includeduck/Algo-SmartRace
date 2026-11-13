#include "Log.h"

#include <cstdio>
#include <cstdarg>

Log::Log(int capacity)
    : m_entries(new LogEntry[capacity]), m_capacity(capacity),
      m_count(0), m_head(0), m_nextIndex(0)
{
}

Log::~Log()
{
    delete[] m_entries;
}

void Log::Write(LogLevel level, int timeMs, const char* fmt, ...)
{
    int slot;
    if (m_count == m_capacity)
    {
        slot = m_head;
        m_head = (m_head + 1) % m_capacity;
    }
    else
    {
        slot = (m_head + m_count) % m_capacity;
        ++m_count;
    }

    LogEntry& entry = m_entries[slot];
    entry.index = m_nextIndex;
    ++m_nextIndex;
    entry.level = level;
    entry.timeMs = timeMs;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.text, sizeof(entry.text), fmt, args);
    va_end(args);
}

void Log::Clear()
{
    m_count = 0;
    m_head = 0;
    m_nextIndex = 0;
}

int Log::Since(int sinceIndex, LogEntry* out, int outCapacity) const
{
    int written = 0;
    for (int i = 0; i < m_count && written < outCapacity; ++i)
    {
        const LogEntry& entry = m_entries[(m_head + i) % m_capacity];
        if (entry.index > sinceIndex)
        {
            out[written] = entry;
            ++written;
        }
    }
    return written;
}

const LogEntry* Log::Last() const
{
    if (m_count == 0) return nullptr;
    return &m_entries[(m_head + m_count - 1) % m_capacity];
}
