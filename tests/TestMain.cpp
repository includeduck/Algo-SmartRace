#include "TestFramework.h"

int g_testFailures = 0;
int g_testChecksRun = 0;

int main()
{
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
