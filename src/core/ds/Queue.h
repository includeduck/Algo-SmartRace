#pragma once

// Queue<T>: our own FIFO ring buffer (the no-STL replacement for
// std::queue). Used by BFS for the frontier.
template <typename T>
class Queue
{
public:
    Queue()
        : m_data(nullptr), m_capacity(0), m_head(0), m_count(0)
    {
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    ~Queue()
    {
        delete[] m_data;
    }

    void Enqueue(const T& value)
    {
        if (m_count >= m_capacity) Grow();
        int tail = (m_head + m_count) % m_capacity;
        m_data[tail] = value;
        ++m_count;
    }

    T Dequeue()
    {
        T value = m_data[m_head];
        m_head = (m_head + 1) % m_capacity;
        --m_count;
        return value;
    }

    const T& Front() const
    {
        return m_data[m_head];
    }

    bool Empty() const
    {
        return m_count == 0;
    }

    int Size() const
    {
        return m_count;
    }

    void Clear()
    {
        m_head = 0;
        m_count = 0;
    }

private:
    void Grow()
    {
        int newCap = (m_capacity == 0) ? 8 : m_capacity * 2;
        T* newData = new T[newCap];
        for (int i = 0; i < m_count; ++i) newData[i] = m_data[(m_head + i) % m_capacity];
        delete[] m_data;
        m_data = newData;
        m_capacity = newCap;
        m_head = 0;
    }

    T* m_data;
    int m_capacity;
    int m_head;
    int m_count;
};
