#include "UnionFind.h"

UnionFind::UnionFind()
    : m_parent(nullptr), m_rank(nullptr), m_size(0), m_componentCount(0)
{
}

UnionFind::~UnionFind()
{
    delete[] m_parent;
    delete[] m_rank;
}

void UnionFind::Init(int n)
{
    delete[] m_parent;
    delete[] m_rank;

    m_size = n;
    m_parent = new int[n];
    m_rank = new int[n];

    for (int i = 0; i < n; ++i)
    {
        m_parent[i] = i;
        m_rank[i] = 0;
    }

    m_componentCount = n;
}

int UnionFind::Find(int x)
{
    while (m_parent[x] != x)
    {
        m_parent[x] = m_parent[m_parent[x]];   // path halving
        x = m_parent[x];
    }
    return x;
}

bool UnionFind::Union(int a, int b)
{
    int rootA = Find(a);
    int rootB = Find(b);
    if (rootA == rootB) return false;

    if (m_rank[rootA] < m_rank[rootB])
    {
        int temp = rootA;
        rootA = rootB;
        rootB = temp;
    }

    m_parent[rootB] = rootA;
    if (m_rank[rootA] == m_rank[rootB]) m_rank[rootA] = m_rank[rootA] + 1;

    --m_componentCount;
    return true;
}
