#pragma once

// LinkedList<T>: our own singly linked list of raw, individually `new`'d
// nodes (the no-STL replacement for std::list / std::forward_list). Used
// for adjacency lists and per-edge active-effect lists, where elements are
// added and removed one at a time and order of the remaining elements does
// not matter.
template <typename T>
struct LinkedListNode
{
    T value;
    LinkedListNode<T>* next;
};

template <typename T>
class LinkedList
{
public:
    LinkedList()
        : m_head(nullptr), m_count(0)
    {
    }

    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    ~LinkedList()
    {
        Clear();
    }

    LinkedListNode<T>* PushFront(const T& value)
    {
        LinkedListNode<T>* node = new LinkedListNode<T>();
        node->value = value;
        node->next = m_head;
        m_head = node;
        ++m_count;
        return node;
    }

    // Removes every node for which pred(value) is true. Pred is any
    // callable (function pointer, lambda, or a struct with operator()) —
    // a plain template parameter, not std::function. Returns true if at
    // least one node was removed.
    template <typename Pred>
    bool RemoveIf(Pred pred)
    {
        LinkedListNode<T>* prev = nullptr;
        LinkedListNode<T>* cur = m_head;
        bool removedAny = false;

        while (cur != nullptr)
        {
            LinkedListNode<T>* next = cur->next;
            if (pred(cur->value))
            {
                if (prev == nullptr)
                {
                    m_head = next;
                }
                else
                {
                    prev->next = next;
                }
                delete cur;
                --m_count;
                removedAny = true;
            }
            else
            {
                prev = cur;
            }
            cur = next;
        }

        return removedAny;
    }

    LinkedListNode<T>* Head() const
    {
        return m_head;
    }

    int Count() const
    {
        return m_count;
    }

    bool Empty() const
    {
        return m_head == nullptr;
    }

    void Clear()
    {
        LinkedListNode<T>* cur = m_head;
        while (cur != nullptr)
        {
            LinkedListNode<T>* next = cur->next;
            delete cur;
            cur = next;
        }
        m_head = nullptr;
        m_count = 0;
    }

private:
    LinkedListNode<T>* m_head;
    int m_count;
};
