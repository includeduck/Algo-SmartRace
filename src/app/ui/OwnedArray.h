#pragma once
#include "api/SmartRaceApi.h"

template <typename T>
class OwnedArray
{
public:
    OwnedArray() = default;
    ~OwnedArray()
    {
        delete[] m_data;
    }
    OwnedArray(const OwnedArray&) = delete;
    OwnedArray& operator=(const OwnedArray&) = delete;
    void Copy(ArrayView<T> source)
    {
        if (source.count > m_capacity)
        {
            delete[] m_data;
            m_data = new T[source.count];
            m_capacity = source.count;
        }
        m_count = source.count;
        for (int i = 0; i < source.count; ++i) m_data[i] = source[i];
    }
    ArrayView<T> View() const
    {
        return { m_data, m_count };
    }
private:
    T* m_data = nullptr;
    int m_capacity = 0;
    int m_count = 0;
};
