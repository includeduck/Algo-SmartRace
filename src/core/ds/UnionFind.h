#pragma once

// UnionFind: disjoint-set with path compression and union by rank.
// Used by Kruskal's MST and by connectivity checks in the track generator.
// Amortized O(alpha(n)) per operation.
class UnionFind
{
public:
    UnionFind();
    ~UnionFind();

    UnionFind(const UnionFind&) = delete;
    UnionFind& operator=(const UnionFind&) = delete;

    void Init(int n);

    int Find(int x);

    // Returns false if a and b were already in the same set (no-op).
    bool Union(int a, int b);

    int ComponentCount() const
    {
        return m_componentCount;
    }

private:
    int* m_parent;
    int* m_rank;
    int m_size;
    int m_componentCount;
};
