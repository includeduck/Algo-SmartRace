#include "BenchUtil.h"
#include "../src/algo/Sorting.h"

#include <cmath>
#include <cstdarg>
#include <direct.h>   // _mkdir (MSVC C runtime)

volatile long long g_benchSink = 0;

namespace
{
    struct DoubleOrder
    {
        static int Compare(const double& a, const double& b)
        {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        }

        static int Key(const double& v)
        {
            return static_cast<int>(v);
        }
    };

    CsvFile g_checkFile;
    int g_checkFailures = 0;
    int g_checkCount = 0;
}

SampleStats ComputeStats(const double* samples, int n)
{
    SampleStats s;
    s.mean = 0.0;
    s.median = 0.0;
    s.stddev = 0.0;
    s.min = 0.0;
    s.max = 0.0;
    if (n <= 0) return s;

    double sorted[kMaxRuns];
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        sorted[i] = samples[i];
        sum += samples[i];
    }
    algo::MergeSort<double, DoubleOrder>(sorted, n);

    s.mean = sum / n;
    s.median = (n % 2 == 1) ? sorted[n / 2] : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
    s.min = sorted[0];
    s.max = sorted[n - 1];
    if (n > 1)
    {
        double sq = 0.0;
        for (int i = 0; i < n; ++i) sq += (samples[i] - s.mean) * (samples[i] - s.mean);
        s.stddev = sqrt(sq / (n - 1));
    }
    return s;
}

bool EnsureDirectory(const char* path)
{
    _mkdir(path);   // fails harmlessly when it already exists
    FILE* probe = nullptr;
    char test[300];
    snprintf(test, sizeof(test), "%s/.probe", path);
    probe = fopen(test, "w");
    if (probe == nullptr) return false;
    fclose(probe);
    remove(test);
    return true;
}

CsvFile::CsvFile()
    : m_file(nullptr)
{
}

CsvFile::~CsvFile()
{
    Close();
}

bool CsvFile::Open(const BenchConfig& cfg, const char* sub, const char* name, const char* header)
{
    Close();
    char dir[300];
    snprintf(dir, sizeof(dir), "%s/%s", cfg.outDir, sub);
    EnsureDirectory(cfg.outDir);
    if (!EnsureDirectory(dir))
    {
        printf("cannot create directory %s\n", dir);
        return false;
    }

    char path[400];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    m_file = fopen(path, "w");
    if (m_file == nullptr)
    {
        printf("cannot write %s\n", path);
        return false;
    }
    fprintf(m_file, "%s\n", header);
    return true;
}

void CsvFile::Row(const char* fmt, ...)
{
    if (m_file == nullptr) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(m_file, fmt, args);
    va_end(args);
    fputc('\n', m_file);
    fflush(m_file);
}

void CsvFile::Close()
{
    if (m_file != nullptr)
    {
        fclose(m_file);
        m_file = nullptr;
    }
}

bool ExperimentOutput::Open(const BenchConfig& cfg, const char* expName, const char* configCols, const char* counterCols)
{
    char name[128];
    char header[512];

    snprintf(name, sizeof(name), "%s_raw.csv", expName);
    snprintf(header, sizeof(header), "%s,reps,run,micros,%s", configCols, counterCols);
    if (!m_raw.Open(cfg, "raw", name, header)) return false;

    snprintf(name, sizeof(name), "%s_summary.csv", expName);
    snprintf(header, sizeof(header), "%s,reps,runs,mean_us,median_us,std_us,min_us,max_us,%s", configCols, counterCols);
    return m_summary.Open(cfg, "summary", name, header);
}

void ExperimentOutput::Emit(const BenchConfig& cfg, const char* config, int reps, const double* samples, const char* counters)
{
    for (int r = 0; r < cfg.runs; ++r)
    {
        m_raw.Row("%s,%d,%d,%.4f,%s", config, reps, r, samples[r], counters);
    }
    SampleStats s = ComputeStats(samples, cfg.runs);
    m_summary.Row("%s,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%s", config, reps, cfg.runs, s.mean, s.median, s.stddev,
        s.min, s.max, counters);
}

void CheckLogOpen(const BenchConfig& cfg)
{
    g_checkFailures = 0;
    g_checkCount = 0;
    g_checkFile.Open(cfg, "summary", "correctness.csv", "experiment,check,passed,detail");
}

void CheckLogRecord(const char* experiment, const char* check, bool passed, const char* fmt, ...)
{
    char detail[300];
    va_list args;
    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);

    ++g_checkCount;
    if (!passed) ++g_checkFailures;
    g_checkFile.Row("%s,%s,%s,\"%s\"", experiment, check, passed ? "yes" : "NO", detail);
    printf("  [%s] %s: %s\n", passed ? "ok" : "FAIL", check, detail);
}

int CheckLogFailures()
{
    return g_checkFailures;
}

void CheckLogClose()
{
    printf("\nCorrectness: %d check(s), %d failure(s)\n", g_checkCount, g_checkFailures);
    g_checkFile.Close();
}
