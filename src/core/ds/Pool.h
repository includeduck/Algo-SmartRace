#pragma once

// Pool<T>: a simple block allocator. Allocate() hands out raw T* from
// pre-allocated blocks, growing one block at a time as needed. There is
// deliberately no per-object Free(): this suits structures whose whole
// owner is torn down at once (adjacency-list nodes freed when a Graph is
// destroyed; pooled race Events freed when a race resets — individual
// events are never truly freed mid-race, only marked cancelled and
// skipped, see sim/EventQueue). Call Clear() to release every block.
template <typename T>
class Pool
{
public:
    explicit Pool(int blockSize = 256)
        : m_blockSize(blockSize < 1 ? 1 : blockSize),
          m_blocks(nullptr), m_blockCapacity(0), m_blockCount(0),
          m_current(nullptr), m_usedInCurrent(0)
    {
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    ~Pool()
    {
        Clear();
    }

    T* Allocate()
    {
        if (m_current == nullptr || m_usedInCurrent >= m_blockSize) AddBlock();
        T* slot = &m_current[m_usedInCurrent];
        ++m_usedInCurrent;
        return slot;
    }

    void Clear()
    {
        for (int i = 0; i < m_blockCount; ++i) delete[] m_blocks[i];
        delete[] m_blocks;
        m_blocks = nullptr;
        m_blockCapacity = 0;
        m_blockCount = 0;
        m_current = nullptr;
        m_usedInCurrent = 0;
    }

private:
    void AddBlock()
    {
        if (m_blockCount >= m_blockCapacity) GrowBlockTable();
        m_current = new T[m_blockSize];
        m_blocks[m_blockCount] = m_current;
        ++m_blockCount;
        m_usedInCurrent = 0;
    }

    void GrowBlockTable()
    {
        int newCap = (m_blockCapacity == 0) ? 4 : m_blockCapacity * 2;
        T** newBlocks = new T*[newCap];
        for (int i = 0; i < m_blockCount; ++i) newBlocks[i] = m_blocks[i];
        delete[] m_blocks;
        m_blocks = newBlocks;
        m_blockCapacity = newCap;
    }

    int m_blockSize;
    T** m_blocks;
    int m_blockCapacity;
    int m_blockCount;
    T* m_current;
    int m_usedInCurrent;
};
