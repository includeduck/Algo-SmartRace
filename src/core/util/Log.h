#pragma once

enum class LogLevel
{
    Info,
    Warning,
    Error,
};

struct LogEntry
{
    int index;          // monotonically increasing, never reused
    LogLevel level;
    int timeMs;          // simulation time this entry refers to, -1 if n/a
    char text[160];
};

// Log: a small fixed-capacity ring buffer of log lines (fixed-size text, no
// per-line heap allocation). Used for the race event ticker (Racing screen)
// and for recording why a command/plan was rejected. The API layer exposes
// Since() so the GUI can poll for lines it hasn't drawn yet.
class Log
{
public:
    explicit Log(int capacity = 512);
    ~Log();

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    void Write(LogLevel level, int timeMs, const char* fmt, ...);
    void Clear();

    int Count() const
    {
        return m_count;
    }

    // Writes entries with index > sinceIndex (oldest first) into `out`, up
    // to outCapacity entries. Returns how many were written. Pass -1 as
    // sinceIndex to get everything currently buffered.
    int Since(int sinceIndex, LogEntry* out, int outCapacity) const;

    const LogEntry* Last() const;

private:
    LogEntry* m_entries;
    int m_capacity;
    int m_count;       // number of valid entries, <= m_capacity
    int m_head;         // index within m_entries of the oldest entry
    int m_nextIndex;   // next global index to assign
};
