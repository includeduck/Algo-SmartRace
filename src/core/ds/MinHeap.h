#pragma once

// MinHeap<T, Comparator>: our own binary min-heap (the no-STL replacement
// for std::priority_queue). Array-backed, 0-indexed.
//
// `Comparator` is a policy type with a static method:
//     static bool Less(const T& a, const T& b);
// so one implementation serves every use (Dijkstra/A* frontier, the race
// event queue, top-k retrieval) by swapping the comparator.
//
// `comparisons` and `swaps` are running counters for the experiments
// (Act 4 priority-queue study) and for the Algorithm Lab.
template <typename T, typename Comparator>
class MinHeap
{
public:
    MinHeap()
        : m_data(nullptr), m_size(0), m_capacity(0), comparisons(0), swaps(0)
    {
    }

    MinHeap(const MinHeap&) = delete;
    MinHeap& operator=(const MinHeap&) = delete;

    ~MinHeap()
    {
        delete[] m_data;
    }

    void Push(const T& value)
    {
        if (m_size >= m_capacity) Reserve(m_capacity == 0 ? 16 : m_capacity * 2);
        m_data[m_size] = value;
        SiftUp(m_size);
        ++m_size;
    }

    T Pop()
    {
        T top = m_data[0];
        --m_size;
        m_data[0] = m_data[m_size];
        SiftDown(0);
        return top;
    }

    const T& Top() const
    {
        return m_data[0];
    }

    int Size() const
    {
        return m_size;
    }

    bool Empty() const
    {
        return m_size == 0;
    }

    void Clear()
    {
        m_size = 0;
    }

    // Builds a heap from `n` existing items in O(n) (bottom-up heapify).
    void Heapify(const T* items, int n)
    {
        Reserve(n);
        for (int i = 0; i < n; ++i) m_data[i] = items[i];
        m_size = n;
        for (int i = m_size / 2 - 1; i >= 0; --i) SiftDown(i);
    }

    long long comparisons;
    long long swaps;

private:
    bool Less(const T& a, const T& b)
    {
        ++comparisons;
        return Comparator::Less(a, b);
    }

    void SwapAt(int i, int j)
    {
        T temp = m_data[i];
        m_data[i] = m_data[j];
        m_data[j] = temp;
        ++swaps;
    }

    void SiftUp(int i)
    {
        while (i > 0)
        {
            int parent = (i - 1) / 2;
            if (!Less(m_data[i], m_data[parent])) break;
            SwapAt(i, parent);
            i = parent;
        }
    }

    void SiftDown(int i)
    {
        for (;;)
        {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;
            if (left < m_size && Less(m_data[left], m_data[smallest])) smallest = left;
            if (right < m_size && Less(m_data[right], m_data[smallest])) smallest = right;
            if (smallest == i) break;
            SwapAt(i, smallest);
            i = smallest;
        }
    }

    void Reserve(int newCapacity)
    {
        if (newCapacity <= m_capacity) return;
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
