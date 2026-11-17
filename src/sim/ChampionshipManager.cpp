#include "ChampionshipManager.h"
#include "../algo/Sorting.h"
#include "../algo/TopK.h"
#include "../model/Rules.h"

namespace sim
{
    ChampionshipManager::ChampionshipManager()
        : m_racesCompleted(0)
    {
    }

    void ChampionshipManager::Reset(int racerCount)
    {
        m_entries.Clear();
        m_history.Clear();
        m_raceStart.Clear();
        m_racesCompleted = 0;

        for (int i = 0; i < racerCount; ++i)
        {
            model::ChampionshipEntry e;
            e.racerId = i;
            e.totalPoints = 0;
            e.wins = 0;
            e.sumOfFinishPositions = 0;
            e.bestFastestLapMs = 0x7fffffff;
            m_entries.Push(e);
        }
    }

    void ChampionshipManager::ApplyRace(const DynArray<model::RaceResultRow>& results)
    {
        m_raceStart.Push(m_history.Size());

        for (int i = 0; i < results.Size(); ++i)
        {
            const model::RaceResultRow& row = results[i];
            m_history.Push(row);

            if (row.racerId < 0 || row.racerId >= m_entries.Size()) continue;
            model::ChampionshipEntry& entry = m_entries[row.racerId];

            entry.totalPoints += row.pointsAwarded;
            entry.sumOfFinishPositions += row.finishPosition;
            if (row.finishPosition == 1 && row.status == model::RaceStatus::Finished) ++entry.wins;
            if (row.bestLapTimeMs < entry.bestFastestLapMs) entry.bestFastestLapMs = row.bestLapTimeMs;
        }

        ++m_racesCompleted;
    }

    bool ChampionshipManager::IsComplete() const
    {
        return m_racesCompleted >= Rules::kChampionshipRaces;
    }

    int ChampionshipManager::ChampionId() const
    {
        if (!IsComplete() || m_entries.Size() == 0) return -1;
        DynArray<model::ChampionshipEntry> standings;
        Standings(SortMethod::Merge, &standings);
        return standings[0].racerId;
    }

    void ChampionshipManager::Standings(SortMethod method, DynArray<model::ChampionshipEntry>* out,
        algo::AlgoStats* stats) const
    {
        out->Clear();
        for (int i = 0; i < m_entries.Size(); ++i) out->Push(m_entries[i]);

        switch (method)
        {
            case SortMethod::Quick:
                algo::QuickSort<model::ChampionshipEntry, algo::StandingOrder>(out->Data(), out->Size(), stats);
                break;
            case SortMethod::Heap:
                algo::HeapSort<model::ChampionshipEntry, algo::StandingOrder>(out->Data(), out->Size(), stats);
                break;
            case SortMethod::Merge:
            default:
                algo::MergeSort<model::ChampionshipEntry, algo::StandingOrder>(out->Data(), out->Size(), stats);
                break;
        }
    }

    bool ChampionshipManager::SelfCheck() const
    {
        DynArray<model::ChampionshipEntry> merge;
        DynArray<model::ChampionshipEntry> quick;
        DynArray<model::ChampionshipEntry> heap;
        Standings(SortMethod::Merge, &merge);
        Standings(SortMethod::Quick, &quick);
        Standings(SortMethod::Heap, &heap);

        for (int i = 0; i < merge.Size(); ++i)
        {
            if (merge[i].racerId != quick[i].racerId || merge[i].racerId != heap[i].racerId) return false;
        }
        return true;
    }

    int ChampionshipManager::TopK(int k, DynArray<model::ChampionshipEntry>* out, algo::AlgoStats* stats) const
    {
        out->Clear();
        if (k <= 0 || m_entries.Size() == 0) return 0;
        if (k > m_entries.Size()) k = m_entries.Size();

        out->Resize(k);
        return algo::TopKSizeKHeap<model::ChampionshipEntry, algo::StandingOrder>(
            m_entries.Data(), m_entries.Size(), k, out->Data(), stats);
    }

    algo::TieBreakReason ChampionshipManager::TieBreak(int racerA, int racerB) const
    {
        if (racerA < 0 || racerA >= m_entries.Size() || racerB < 0 || racerB >= m_entries.Size())
        {
            return algo::TieBreakReason::Identical;
        }
        return algo::StandingTieBreakReason(m_entries[racerA], m_entries[racerB]);
    }

    int ChampionshipManager::RaceHistory(int raceIndex, const model::RaceResultRow** outRows) const
    {
        *outRows = nullptr;
        if (raceIndex < 0 || raceIndex >= m_raceStart.Size()) return 0;

        int start = m_raceStart[raceIndex];
        int end = (raceIndex + 1 < m_raceStart.Size()) ? m_raceStart[raceIndex + 1] : m_history.Size();
        *outRows = m_history.Data() + start;
        return end - start;
    }
}
