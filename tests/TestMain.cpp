#include "TestFramework.h"

int g_testFailures = 0;
int g_testChecksRun = 0;

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace
{
    // Debug builds: the MSVC CRT debug heap reports every block still
    // allocated when the process exits (after static destructors), on
    // stderr, as "Detected memory leaks!". Release builds: no-op.
    void EnableLeakReport()
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
    }
}

int main()
{
    EnableLeakReport();
    TestRegistry& registry = TestRegistry::Instance();
    int total = registry.Count();
    printf("Running %d test(s)...\n\n", total);

    int passed = 0;
    for (int i = 0; i < total; ++i)
    {
        const TestCase& test = registry.Get(i);
        int failuresBefore = g_testFailures;
        test.fn();
        if (g_testFailures == failuresBefore)
        {
            printf("[PASS] %s\n", test.name);
            ++passed;
        }
        else
        {
            printf("[FAIL] %s\n", test.name);
        }
    }

    printf("\n%d/%d test(s) passed. %d check(s) run, %d failure(s).\n",
        passed, total, g_testChecksRun, g_testFailures);

    return g_testFailures;
}
