#pragma once

// DynArray<T>: our own dynamic array (the no-STL replacement for
// std::vector). Grows by doubling. T must be default-constructible and
// copy-assignable (we allocate with `new T[n]` and assign into slots,
// rather than using placement-new, to keep this simple and obviously
// correct for the plain structs and primitive types this project uses).
//
// Raw pointers only, no STL, manually implemented (ProjectRestrictions.txt).
template <typename T>
class DynArray
{
public:
    DynArray()
        : m_data(nullptr), m_size(0), m_capacity(0)
    {
    }

    DynArray(const DynArray&) = delete;
    DynArray& operator=(const DynArray&) = delete;

    // Move-only: lets result structs built from several DynArray fields
    // (BfsResult, DfsTraversalResult, ...) be returned by value without a
    // deep copy. This is a core C++ language feature, not STL.
    DynArray(DynArray&& other) noexcept
        : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity)
    {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    DynArray& operator=(DynArray&& other) noexcept
    {
        if (this != &other)
        {
            delete[] m_data;
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    ~DynArray()
    {
        delete[] m_data;
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

    void Push(const T& value)
    {
        GrowIfNeeded(m_size + 1);
        m_data[m_size] = value;
        ++m_size;
    }

    T Pop()
    {
        --m_size;
        return m_data[m_size];
    }

    const T& Back() const
    {
        return m_data[m_size - 1];
    }

    T& Back()
    {
        return m_data[m_size - 1];
    }

    // Removes the element at `index` by swapping the last element into its
    // place. O(1), but does not preserve order.
    void RemoveAtSwap(int index)
    {
        m_data[index] = m_data[m_size - 1];
        --m_size;
    }

    // Removes the element at `index`, preserving order. O(n).
    void RemoveAtOrdered(int index)
    {
        for (int i = index; i < m_size - 1; ++i) m_data[i] = m_data[i + 1];
        --m_size;
    }

    void InsertAt(int index, const T& value)
    {
        GrowIfNeeded(m_size + 1);
        for (int i = m_size; i > index; --i) m_data[i] = m_data[i - 1];
        m_data[index] = value;
        ++m_size;
    }

    // Grows or shrinks the logical size. New slots after a grow hold
    // whatever `new T[]` default-constructed (0 for ints/doubles, etc.).
    void Resize(int newSize)
    {
        if (newSize > m_capacity) Reserve(newSize);
        m_size = newSize;
    }

    void Clear()
    {
        m_size = 0;
    }

    T& operator[](int index)
    {
        return m_data[index];
    }

    const T& operator[](int index) const
    {
        return m_data[index];
    }

    int Size() const
    {
        return m_size;
    }

    int Capacity() const
    {
        return m_capacity;
    }

    bool Empty() const
    {
        return m_size == 0;
    }

    T* Data()
    {
        return m_data;
    }

    const T* Data() const
    {
        return m_data;
    }

private:
    void GrowIfNeeded(int needed)
    {
        if (needed <= m_capacity) return;
        int newCap = (m_capacity == 0) ? 4 : m_capacity * 2;
        if (newCap < needed) newCap = needed;
        Reserve(newCap);
    }

    T* m_data;
    int m_size;
    int m_capacity;
};
