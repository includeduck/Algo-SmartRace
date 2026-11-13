#include "TestFramework.h"

#include "../src/core/ds/DynArray.h"
#include "../src/core/ds/Queue.h"
#include "../src/core/ds/Stack.h"
#include "../src/core/ds/MinHeap.h"
#include "../src/core/ds/LinkedList.h"
#include "../src/core/ds/Pool.h"
#include "../src/core/ds/UnionFind.h"
#include "../src/core/util/String.h"
#include "../src/core/util/Rng.h"
#include "../src/core/util/MathUtil.h"
#include "../src/core/util/Log.h"

TEST(DynArray_PushAndIndex)
{
    DynArray<int> arr;
    for (int i = 0; i < 100; ++i) arr.Push(i * 2);

    CHECK_EQ(arr.Size(), 100);
    for (int i = 0; i < 100; ++i) CHECK_EQ(arr[i], i * 2);
}

TEST(DynArray_PopAndRemoveAtSwap)
{
    DynArray<int> arr;
    arr.Push(1);
    arr.Push(2);
    arr.Push(3);
    arr.Push(4);

    CHECK_EQ(arr.Pop(), 4);
    CHECK_EQ(arr.Size(), 3);

    arr.RemoveAtSwap(0);   // last element (3) swaps into slot 0
    CHECK_EQ(arr.Size(), 2);
    CHECK_EQ(arr[0], 3);
}

TEST(DynArray_InsertAt)
{
    DynArray<int> arr;
    arr.Push(1);
    arr.Push(2);
    arr.Push(4);
    arr.InsertAt(2, 3);

    CHECK_EQ(arr.Size(), 4);
    CHECK_EQ(arr[0], 1);
    CHECK_EQ(arr[1], 2);
    CHECK_EQ(arr[2], 3);
    CHECK_EQ(arr[3], 4);
}

TEST(Queue_FifoAndWraparound)
{
    Queue<int> q;
    for (int round = 0; round < 3; ++round)
    {
        for (int i = 0; i < 5; ++i) q.Enqueue(round * 10 + i);
        for (int i = 0; i < 5; ++i) CHECK_EQ(q.Dequeue(), round * 10 + i);
    }
    CHECK(q.Empty());
}

TEST(Stack_LifoOrder)
{
    Stack<int> s;
    s.Push(1);
    s.Push(2);
    s.Push(3);

    CHECK_EQ(s.Pop(), 3);
    CHECK_EQ(s.Pop(), 2);
    CHECK_EQ(s.Top(), 1);
    CHECK_EQ(s.Pop(), 1);
    CHECK(s.Empty());
}

struct IntLess
{
    static bool Less(const int& a, const int& b)
    {
        return a < b;
    }
};

TEST(MinHeap_SortedExtraction)
{
    Rng rng(12345);
    MinHeap<int, IntLess> heap;
    const int n = 500;
    for (int i = 0; i < n; ++i) heap.Push(rng.NextInt(0, 100000));

    int prev = -1;
    int count = 0;
    while (!heap.Empty())
    {
        int value = heap.Pop();
        CHECK(value >= prev);
        prev = value;
        ++count;
    }
    CHECK_EQ(count, n);
}

TEST(MinHeap_Heapify)
{
    int values[6] = { 5, 3, 8, 1, 9, 2 };
    MinHeap<int, IntLess> heap;
    heap.Heapify(values, 6);
    CHECK_EQ(heap.Size(), 6);

    int prev = -1;
    while (!heap.Empty())
    {
        int v = heap.Pop();
        CHECK(v >= prev);
        prev = v;
    }
}

TEST(UnionFind_UnionAndComponents)
{
    UnionFind uf;
    uf.Init(10);
    CHECK_EQ(uf.ComponentCount(), 10);

    CHECK(uf.Union(0, 1));
    CHECK(uf.Union(1, 2));
    CHECK(!uf.Union(0, 2));   // already connected

    CHECK_EQ(uf.Find(0), uf.Find(2));
    CHECK_EQ(uf.ComponentCount(), 8);
}

TEST(LinkedList_PushAndRemoveIf)
{
    LinkedList<int> list;
    list.PushFront(1);
    list.PushFront(2);
    list.PushFront(3);
    CHECK_EQ(list.Count(), 3);

    bool removed = list.RemoveIf([](const int& v)
    {
        return v == 2;
    });
    CHECK(removed);
    CHECK_EQ(list.Count(), 2);

    int sum = 0;
    for (LinkedListNode<int>* node = list.Head(); node != nullptr; node = node->next)
    {
        sum += node->value;
    }
    CHECK_EQ(sum, 4);   // 1 + 3
}

TEST(Pool_AllocatesDistinctAddresses)
{
    Pool<int> pool(4);   // small block size to force several blocks
    int* pointers[20];

    for (int i = 0; i < 20; ++i)
    {
        pointers[i] = pool.Allocate();
        *pointers[i] = i;
    }

    for (int i = 0; i < 20; ++i) CHECK_EQ(*pointers[i], i);

    for (int i = 0; i < 20; ++i)
    {
        for (int j = i + 1; j < 20; ++j) CHECK(pointers[i] != pointers[j]);
    }
}

TEST(String_BasicOps)
{
    String s("hello");
    CHECK_EQ(s.Length(), 5);
    s.Append(" world");
    CHECK(s.Equals("hello world"));

    String formatted;
    formatted.Format("%d-%d", 3, 4);
    CHECK(formatted.Equals("3-4"));

    bool ok = false;
    String num("42");
    CHECK_EQ(num.ToInt(&ok), 42);
    CHECK(ok);

    bool okNeg = false;
    String neg("-7");
    CHECK_EQ(neg.ToInt(&okNeg), -7);
    CHECK(okNeg);

    bool okBad = true;
    String bad("12x");
    bad.ToInt(&okBad);
    CHECK(!okBad);

    bool okDouble = false;
    String dbl("3.75");
    double value = dbl.ToDouble(&okDouble);
    CHECK(okDouble);
    CHECK(MathUtil::Abs(value - 3.75) < 1e-9);
}

TEST(Rng_DeterministicForSameSeed)
{
    Rng a(777);
    Rng b(777);
    for (int i = 0; i < 50; ++i) CHECK_EQ(a.NextU32(), b.NextU32());
}

TEST(MathUtil_MinMaxClamp)
{
    CHECK_EQ(MathUtil::Min(3, 5), 3);
    CHECK_EQ(MathUtil::Max(3, 5), 5);
    CHECK_EQ(MathUtil::Clamp(10, 0, 5), 5);
    CHECK_EQ(MathUtil::Clamp(-10, 0, 5), 0);
}

TEST(Log_RingBufferSince)
{
    Log log(4);
    log.Write(LogLevel::Info, 0, "one");
    log.Write(LogLevel::Info, 10, "two");
    log.Write(LogLevel::Info, 20, "three");

    LogEntry out[8];
    int n = log.Since(-1, out, 8);
    CHECK_EQ(n, 3);
    CHECK(String::StrEquals(out[0].text, "one"));
    CHECK(String::StrEquals(out[2].text, "three"));

    n = log.Since(out[0].index, out, 8);
    CHECK_EQ(n, 2);
}
