#pragma once

// Shared pieces of smartrace_bench (the 5 required experiments, Project.pdf
// section 7): run configuration, timing with warm-up and repetitions,
// sample statistics, CSV output and the untimed correctness log.
//
// Timing rules used by every experiment:
//   * Inputs are generated, and correctness is checked, OUTSIDE the timed
//     region. Only the algorithm call is timed.
//   * Each configuration runs `warmup` untimed runs, then `runs` timed ones.
//   * Very fast calls are repeated `reps` times inside one timed region and
//     the total is divided by `reps`, so one sample is never below the
//     timer's resolution. `reps` is written to the CSV.
//   * Whether a sample includes the algorithm's own data-structure
//     initialisation (result arrays, heap, DP table) is stated per
//     experiment in a CSV column; it does for every algorithm here, because
//     each call allocates its own working storage.
#include "../src/core/util/Timer.h"

#include <cstdio>

struct BenchConfig
{
    int runs;
    int warmup;
    unsigned long long seed;
    char outDir[256];
    bool quick;            // smaller sizes: a smoke test, e.g. on a Debug build
};

struct SampleStats
{
    double mean;
    double median;
    double stddev;
    double min;
    double max;
};

// Mean, median (via the project's Merge Sort), sample standard deviation.
SampleStats ComputeStats(const double* samples, int n);

// Keeps results observable so the optimiser cannot drop a timed call.
extern volatile long long g_benchSink;

// Runs f() `cfg.warmup` times untimed, then `cfg.runs` timed samples of
// `reps` calls each. samplesOut[i] = microseconds per call.
template <typename F>
void Measure(const BenchConfig& cfg, int reps, F& f, double* samplesOut)
{
    if (reps < 1) reps = 1;
    for (int w = 0; w < cfg.warmup; ++w) f();
    for (int r = 0; r < cfg.runs; ++r)
    {
        double t0 = Timer::NowMicros();
        for (int k = 0; k < reps; ++k) f();
        double t1 = Timer::NowMicros();
        samplesOut[r] = (t1 - t0) / reps;
    }
}

const int kMaxRuns = 64;

// A CSV file under <outDir>/<sub>/<name>. Rows are printf-formatted.
class CsvFile
{
public:
    CsvFile();
    ~CsvFile();

    CsvFile(const CsvFile&) = delete;
    CsvFile& operator=(const CsvFile&) = delete;

    bool Open(const BenchConfig& cfg, const char* sub, const char* name, const char* header);
    void Row(const char* fmt, ...);
    void Close();

private:
    FILE* m_file;
};

// The raw and summary CSVs of one experiment. `configCols` names the
// columns that identify a configuration, `counterCols` the operation
// counters (which are identical for every run of one configuration: the
// inputs are fixed).
class ExperimentOutput
{
public:
    bool Open(const BenchConfig& cfg, const char* expName, const char* configCols, const char* counterCols);

    // config/counters are already-formatted CSV fragments (no leading or
    // trailing comma) matching the column lists given to Open.
    void Emit(const BenchConfig& cfg, const char* config, int reps, const double* samples, const char* counters);

private:
    CsvFile m_raw;
    CsvFile m_summary;
};

// Untimed correctness checks, written to <outDir>/summary/correctness.csv
// and echoed to the console.
void CheckLogOpen(const BenchConfig& cfg);
void CheckLogRecord(const char* experiment, const char* check, bool passed, const char* fmt, ...);
int CheckLogFailures();
void CheckLogClose();

bool EnsureDirectory(const char* path);

// Experiments (exp1..exp5.cpp).
void RunExperiment1(const BenchConfig& cfg);
void RunExperiment2(const BenchConfig& cfg);
void RunExperiment3(const BenchConfig& cfg);
void RunExperiment4(const BenchConfig& cfg);
void RunExperiment5(const BenchConfig& cfg);
