#pragma once

// Stack<T>: our own LIFO stack (the no-STL replacement for std::stack).
// Used by iterative DFS and for path reconstruction.
template <typename T>
class Stack
{
public:
    Stack()
        : m_data(nullptr), m_size(0), m_capacity(0)
    {
    }

    Stack(const Stack&) = delete;
    Stack& operator=(const Stack&) = delete;

    ~Stack()
    {
        delete[] m_data;
    }

    void Push(const T& value)
    {
        if (m_size >= m_capacity) Reserve(m_capacity == 0 ? 8 : m_capacity * 2);
        m_data[m_size] = value;
        ++m_size;
    }

    T Pop()
    {
        --m_size;
        return m_data[m_size];
    }

    T& Top()
    {
        return m_data[m_size - 1];
    }

    const T& Top() const
    {
        return m_data[m_size - 1];
    }

    bool Empty() const
    {
        return m_size == 0;
    }

    int Size() const
    {
        return m_size;
    }

    void Clear()
    {
        m_size = 0;
    }

private:
    void Reserve(int newCapacity)
    {
        T* newData = new T[newCapacity];
        for (int i = 0; i < m_size; ++i) newData[i] = m_data[i];
        delete[] m_data;
        m_data = newData;
        m_capacity = newCapacity;
    }

    T* m_data;
    int m_size;
    int m_capacity;
};
