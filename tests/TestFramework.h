#pragma once

#include <cstdio>

// A tiny no-STL test framework: TEST(name) registers a function pointer
// into a static array at startup (via a file-scope TestRegistrar whose
// constructor runs before main); CHECK/CHECK_EQ record failures without
// throwing. The process exit code is the number of failed checks.

typedef void (*TestFn)();

struct TestCase
{
    const char* name;
    TestFn fn;
};

class TestRegistry
{
public:
    static TestRegistry& Instance()
    {
        static TestRegistry instance;
        return instance;
    }

    void Add(const char* name, TestFn fn)
    {
        if (m_count < kMaxTests)
        {
            m_tests[m_count].name = name;
            m_tests[m_count].fn = fn;
            ++m_count;
        }
    }

    int Count() const
    {
        return m_count;
    }

    const TestCase& Get(int i) const
    {
        return m_tests[i];
    }

private:
    TestRegistry()
        : m_count(0)
    {
    }

    static const int kMaxTests = 1024;
    TestCase m_tests[kMaxTests];
    int m_count;
};

struct TestRegistrar
{
    TestRegistrar(const char* name, TestFn fn)
    {
        TestRegistry::Instance().Add(name, fn);
    }
};

extern int g_testFailures;
extern int g_testChecksRun;

#define TEST(name) \
    static void Test_##name(); \
    static TestRegistrar Registrar_##name(#name, Test_##name); \
    static void Test_##name()

#define CHECK(cond) \
    do \
    { \
        ++g_testChecksRun; \
        if (!(cond)) \
        { \
            ++g_testFailures; \
            printf("  FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do \
    { \
        ++g_testChecksRun; \
        if (!((a) == (b))) \
        { \
            ++g_testFailures; \
            printf("  FAIL %s:%d: CHECK_EQ(%s, %s)\n", __FILE__, __LINE__, #a, #b); \
        } \
    } while (0)
