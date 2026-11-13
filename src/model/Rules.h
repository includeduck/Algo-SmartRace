#pragma once

// Frozen game parameters (Project.pdf section 4.3, plan section 8). These
// are mirrored in data/rules.cfg for display (report, a future GUI "About"
// screen) without linking the backend, but gameplay code always uses these
// compiled-in constants, so the two can never drift apart silently. If you
// change a value here, update data/rules.cfg to match.
namespace Rules
{
    const int kSimStepMs = 20;

    const int kDefaultLaps = 3;
    const int kMinLaps = 2;
    const int kMaxLaps = 3;

    const int kMaxFuelTenths = 1000;            // 100.0 fuel units
    const int kMaxStationPurchaseTenths = 400;   // 40.0 units per stop
    const int kStationStopTimeMs = 3000;

    const double kFuelDifficultyMultiplierBase = 1.0;
    const double kFuelDifficultyMultiplierStep = 0.1;   // per difficulty level above 1

    const int kMaxNitroCharges = 3;
    const int kDefaultStartNitroCharges = 1;
    const double kNitroSpeedMultiplier = 1.5;
    const int kNitroDurationMs = 4000;
    const double kNitroFuelMultiplier = 2.0;

    const double kOpponentLowFuelThreshold = 0.25;   // 25%

    const int kOilSpillDurationMs = 20000;
    const int kOilSpillDifficultyBonus = 2;
    const double kOilSpillObstaclePenalty = 0.5;

    const int kSpeedRestrictionDurationMs = 15000;
    const double kSpeedRestrictionCapFraction = 0.5;

    const int kFuelStationOutageDurationMs = 30000;
    const int kNitroBoosterDurationMs = 25000;

    const int kRecoveryWaitTimeoutMs = 60000;     // -> DNF if still stuck
    const int kRaceFinishGraceMs = 60000;          // leader finish + this
    const int kRaceHardCapMs = 15 * 60 * 1000;

    const int kMandatoryCheckpointsPerCircuit = 10;
    const int kMinShortcutsPerCircuit = 2;

    const int kKnapsackDefaultBudgetTenthsSec = 150;   // 15.0s detour allowance

    const int kAlternativeRouteCap = 8;
    const int kAlternativeRouteMaxDepthMultiplier = 2;   // depth <= this * V

    const int kGeneratorMaxRetries = 50;

    // Championship points for positions 1..10 (index 0 = 1st place), plus a
    // fixed bonus for the fastest lap among finishers.
    const int kChampionshipPointsCount = 10;
    const int kChampionshipPoints[kChampionshipPointsCount] =
    {
        25, 18, 15, 12, 10, 8, 6, 4, 2, 1,
    };
    const int kFastestLapBonusPoints = 1;
}
