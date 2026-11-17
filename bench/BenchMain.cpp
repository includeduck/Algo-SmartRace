// smartrace_bench: the five required experimental studies (Project.pdf
// section 7). Usage:
//
//   smartrace_bench <exp1|exp2|exp3|exp4|exp5|all> [--runs N] [--warmup N]
//                   [--seed S] [--out DIR] [--quick]
//
// Defaults: --runs 7 --warmup 2 --seed 42 --out results. Run the Release
// build from the repository root (Experiment 2 reads data/circuits). Raw
// samples go to DIR/raw/expN_*_raw.csv, per-configuration statistics to
// DIR/summary/expN_*_summary.csv, untimed correctness checks to
// DIR/summary/correctness.csv and the run environment to
// DIR/summary/environment.txt. The exit code is the number of failed
// correctness checks.
#include "BenchUtil.h"
#include "../src/core/util/String.h"

#include <cstdio>

namespace
{
    bool ParseUnsigned(const char* s, unsigned long long* out)
    {
        if (s == nullptr || *s == '\0') return false;
        unsigned long long v = 0;
        for (const char* p = s; *p != '\0'; ++p)
        {
            if (*p < '0' || *p > '9') return false;
            v = v * 10 + static_cast<unsigned long long>(*p - '0');
        }
        *out = v;
        return true;
    }

    void Usage()
    {
        printf("usage: smartrace_bench <exp1|exp2|exp3|exp4|exp5|all> [--runs N] [--warmup N] [--seed S] [--out DIR] [--quick]\n");
    }

    void WriteEnvironment(const BenchConfig& cfg)
    {
        char dir[300];
        snprintf(dir, sizeof(dir), "%s/summary", cfg.outDir);
        EnsureDirectory(cfg.outDir);
        EnsureDirectory(dir);

        char path[400];
        snprintf(path, sizeof(path), "%s/environment.txt", dir);
        FILE* f = fopen(path, "w");
        if (f == nullptr) return;

#if defined(_MSC_FULL_VER)
        fprintf(f, "compiler: MSVC %d\n", _MSC_FULL_VER);
#else
        fprintf(f, "compiler: unknown\n");
#endif
#if defined(NDEBUG)
        fprintf(f, "build: Release (optimised)\n");
#else
        fprintf(f, "build: Debug (NOT for reported timings)\n");
#endif
        fprintf(f, "pointer size: %d bits\n", static_cast<int>(sizeof(void*) * 8));
        fprintf(f, "runs per configuration: %d (after %d warm-up runs)\n", cfg.runs, cfg.warmup);
        fprintf(f, "input seed: %llu\n", cfg.seed);
        fprintf(f, "quick mode: %s\n", cfg.quick ? "yes (reduced sizes)" : "no");
        fprintf(f, "timer: QueryPerformanceCounter, microseconds\n");
        fclose(f);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        Usage();
        return 1;
    }

    BenchConfig cfg;
    cfg.runs = 7;
    cfg.warmup = 2;
    cfg.seed = 42;
    cfg.quick = false;
    String::StrCopy(cfg.outDir, sizeof(cfg.outDir), "results");

    for (int i = 2; i < argc; ++i)
    {
        unsigned long long v = 0;
        bool hasValue = (i + 1 < argc);
        if (String::StrEquals(argv[i], "--quick"))
        {
            cfg.quick = true;
        }
        else if (String::StrEquals(argv[i], "--runs") && hasValue && ParseUnsigned(argv[i + 1], &v))
        {
            cfg.runs = static_cast<int>(v);
            ++i;
        }
        else if (String::StrEquals(argv[i], "--warmup") && hasValue && ParseUnsigned(argv[i + 1], &v))
        {
            cfg.warmup = static_cast<int>(v);
            ++i;
        }
        else if (String::StrEquals(argv[i], "--seed") && hasValue && ParseUnsigned(argv[i + 1], &v))
        {
            cfg.seed = v;
            ++i;
        }
        else if (String::StrEquals(argv[i], "--out") && hasValue)
        {
            String::StrCopy(cfg.outDir, sizeof(cfg.outDir), argv[i + 1]);
            ++i;
        }
        else
        {
            Usage();
            return 1;
        }
    }
    if (cfg.runs < 1) cfg.runs = 1;
    if (cfg.runs > kMaxRuns) cfg.runs = kMaxRuns;

    const char* which = argv[1];
    bool all = String::StrEquals(which, "all");
    bool any = all;

#if !defined(NDEBUG)
    printf("WARNING: Debug build -- timings are not representative; use the Release build for the report.\n");
#endif

    WriteEnvironment(cfg);
    CheckLogOpen(cfg);

    if (all || String::StrEquals(which, "exp1"))
    {
        RunExperiment1(cfg);
        any = true;
    }
    if (all || String::StrEquals(which, "exp2"))
    {
        RunExperiment2(cfg);
        any = true;
    }
    if (all || String::StrEquals(which, "exp3"))
    {
        RunExperiment3(cfg);
        any = true;
    }
    if (all || String::StrEquals(which, "exp4"))
    {
        RunExperiment4(cfg);
        any = true;
    }
    if (all || String::StrEquals(which, "exp5"))
    {
        RunExperiment5(cfg);
        any = true;
    }

    if (!any)
    {
        Usage();
        return 1;
    }

    int failures = CheckLogFailures();
    CheckLogClose();
    return failures;
}
