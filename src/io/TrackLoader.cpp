// .trk grammar (one directive per line, '#' starts a line comment, blank
// lines ignored, a quoted "..." token may contain spaces):
//
//   CIRCUIT <number> "<name>"
//   LAPS <n>
//   VERTEX <id> <x> <y> <Start|Finish|Checkpoint|Intersection|FuelStation|NitroPickup|DeadEnd> [cpOrder]
//   EDGE   <id> <from> <to> <distance> <baseTime> <fuelCost> <difficulty> <shortcut 0|1> <directed 0|1> [Blocked]
//   MAINROUTE <v0> <v1> ... <vN>
//   DETOUR    <blockedEdgeId> <v...>
//   SHORTCUT  <edgeId>
//   EVENT     <triggerTimeMs> <Type> <target> <durationMs>
//
// VERTEX and EDGE ids must be declared in order starting at 0 (the loader
// uses that to validate the file matches the graph it is building).
#include "TrackLoader.h"
#include "../model/Rules.h"

#include <cstdio>

namespace io
{
    namespace
    {
        const int kMaxTokens = 32;
        const int kMaxTokenLen = 128;
        typedef char TokenBuf[kMaxTokenLen];

        // Splits one line into whitespace-separated tokens. A token may
        // instead be a "quoted string" (kept together, quotes stripped). A
        // '#' outside quotes starts a comment and ends tokenizing for that
        // line.
        int SplitTokens(const char* line, TokenBuf* tokens, int maxTokens)
        {
            int count = 0;
            int i = 0;

            while (line[i] != '\0' && count < maxTokens)
            {
                while (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n') ++i;
                if (line[i] == '\0' || line[i] == '#') break;

                int len = 0;
                if (line[i] == '"')
                {
                    ++i;
                    while (line[i] != '\0' && line[i] != '"' && len < kMaxTokenLen - 1)
                    {
                        tokens[count][len] = line[i];
                        ++len;
                        ++i;
                    }
                    if (line[i] == '"') ++i;
                }
                else
                {
                    while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t'
                        && line[i] != '\r' && line[i] != '\n' && line[i] != '#' && len < kMaxTokenLen - 1)
                    {
                        tokens[count][len] = line[i];
                        ++len;
                        ++i;
                    }
                }

                tokens[count][len] = '\0';
                ++count;
            }

            return count;
        }

        int ParseIntToken(const char* tok)
        {
            String s(tok);
            return s.ToInt();
        }

        double ParseDoubleToken(const char* tok)
        {
            String s(tok);
            return s.ToDouble();
        }

        model::VertexType ParseVertexType(const char* s, bool* ok)
        {
            if (ok != nullptr) *ok = true;

            if (String::StrEquals(s, "Start")) return model::VertexType::Start;
            if (String::StrEquals(s, "Finish")) return model::VertexType::Finish;
            if (String::StrEquals(s, "Checkpoint")) return model::VertexType::Checkpoint;
            if (String::StrEquals(s, "Intersection")) return model::VertexType::Intersection;
            if (String::StrEquals(s, "FuelStation")) return model::VertexType::FuelStation;
            if (String::StrEquals(s, "NitroPickup")) return model::VertexType::NitroPickup;
            if (String::StrEquals(s, "DeadEnd")) return model::VertexType::DeadEnd;

            if (ok != nullptr) *ok = false;
            return model::VertexType::Intersection;
        }

        void ClearTrackData(TrackData* data)
        {
            delete data->graph;
            data->graph = nullptr;
            data->name.Set("");
            data->laps = Rules::kDefaultLaps;
            data->mainRoute.Clear();
            for (int i = 0; i < data->detours.Size(); ++i) delete data->detours[i];
            data->detours.Clear();
            data->shortcutPreference.Clear();
            data->events.Clear();
        }

        LoadResult Fail(int* errorLine, char* errorMessage, int errorMessageCapacity,
            int lineNumber, const char* message)
        {
            if (errorLine != nullptr) *errorLine = lineNumber;
            if (errorMessage != nullptr) String::StrCopy(errorMessage, errorMessageCapacity, message);
            return LoadResult::ParseError;
        }
    }

    TrackData::TrackData()
        : graph(nullptr), laps(Rules::kDefaultLaps)
    {
    }

    TrackData::~TrackData()
    {
        delete graph;
        for (int i = 0; i < detours.Size(); ++i) delete detours[i];
    }

    LoadResult LoadTrackFile(const char* path, TrackData* outData,
        int* errorLine, char* errorMessage, int errorMessageCapacity)
    {
        FILE* file = fopen(path, "r");
        if (file == nullptr)
        {
            return Fail(errorLine, errorMessage, errorMessageCapacity, 0, "could not open file");
        }

        ClearTrackData(outData);
        outData->graph = new model::Graph();

        char lineBuf[512];
        int lineNumber = 0;
        int nextVertexId = 0;
        int nextEdgeId = 0;

        while (fgets(lineBuf, static_cast<int>(sizeof(lineBuf)), file) != nullptr)
        {
            ++lineNumber;

            TokenBuf tokens[kMaxTokens];
            int tokenCount = SplitTokens(lineBuf, tokens, kMaxTokens);
            if (tokenCount == 0) continue;

            const char* keyword = tokens[0];

            if (String::StrEquals(keyword, "CIRCUIT"))
            {
                if (tokenCount >= 3) outData->name.Set(tokens[2]);
            }
            else if (String::StrEquals(keyword, "LAPS"))
            {
                if (tokenCount >= 2) outData->laps = ParseIntToken(tokens[1]);
            }
            else if (String::StrEquals(keyword, "VERTEX"))
            {
                if (tokenCount < 5)
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "VERTEX needs: id x y type [cpOrder]");
                }

                int id = ParseIntToken(tokens[1]);
                if (id != nextVertexId)
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "VERTEX ids must be declared in order starting at 0");
                }

                double x = ParseDoubleToken(tokens[2]);
                double y = ParseDoubleToken(tokens[3]);

                bool typeOk = false;
                model::VertexType type = ParseVertexType(tokens[4], &typeOk);
                if (!typeOk)
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "unknown vertex type");
                }

                int cpOrder = (tokenCount >= 6) ? ParseIntToken(tokens[5]) : -1;

                outData->graph->AddVertex(x, y, type, cpOrder);
                ++nextVertexId;
            }
            else if (String::StrEquals(keyword, "EDGE"))
            {
                if (tokenCount < 10)
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "EDGE needs: id from to distance baseTime fuelCost difficulty shortcut directed");
                }

                int id = ParseIntToken(tokens[1]);
                if (id != nextEdgeId)
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "EDGE ids must be declared in order starting at 0");
                }

                int from = ParseIntToken(tokens[2]);
                int to = ParseIntToken(tokens[3]);
                if (from < 0 || from >= outData->graph->VertexCount()
                    || to < 0 || to >= outData->graph->VertexCount())
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "EDGE references an undeclared vertex");
                }

                double distance = ParseDoubleToken(tokens[4]);
                double baseTime = ParseDoubleToken(tokens[5]);
                double fuelCost = ParseDoubleToken(tokens[6]);
                int difficulty = ParseIntToken(tokens[7]);
                bool isShortcut = ParseIntToken(tokens[8]) != 0;
                bool directed = ParseIntToken(tokens[9]) != 0;

                model::EdgeStatus baseStatus = model::EdgeStatus::Open;
                if (tokenCount >= 11 && String::StrEquals(tokens[10], "Blocked"))
                {
                    baseStatus = model::EdgeStatus::Blocked;
                }

                outData->graph->AddEdge(from, to, distance, baseTime, fuelCost,
                    difficulty, isShortcut, directed, baseStatus);
                ++nextEdgeId;
            }
            else if (String::StrEquals(keyword, "MAINROUTE"))
            {
                for (int i = 1; i < tokenCount; ++i) outData->mainRoute.Push(ParseIntToken(tokens[i]));
            }
            else if (String::StrEquals(keyword, "DETOUR"))
            {
                if (tokenCount < 2)
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "DETOUR needs: blockedEdgeId v...");
                }

                DetourRule* rule = new DetourRule();
                rule->blockedEdgeId = ParseIntToken(tokens[1]);
                for (int i = 2; i < tokenCount; ++i) rule->viaVertices.Push(ParseIntToken(tokens[i]));
                outData->detours.Push(rule);
            }
            else if (String::StrEquals(keyword, "SHORTCUT"))
            {
                if (tokenCount >= 2) outData->shortcutPreference.Push(ParseIntToken(tokens[1]));
            }
            else if (String::StrEquals(keyword, "EVENT"))
            {
                if (tokenCount < 5)
                {
                    fclose(file);
                    return Fail(errorLine, errorMessage, errorMessageCapacity, lineNumber,
                        "EVENT needs: triggerTimeMs Type target durationMs");
                }

                ScriptedEvent ev;
                ev.triggerTimeMs = ParseIntToken(tokens[1]);
                ev.type.Set(tokens[2]);
                ev.target = ParseIntToken(tokens[3]);
                ev.durationMs = ParseIntToken(tokens[4]);
                outData->events.Push(ev);
            }
            // Unknown keywords are ignored, so the format can grow without
            // breaking older files.
        }

        fclose(file);
        return LoadResult::Ok;
    }
}
