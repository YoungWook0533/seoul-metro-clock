#pragma once
#include <algorithm>

inline int metroMainTrain(int firstEta,int secondEta,int walking) {
  return firstEta>=walking?0:secondEta>=walking?1:-1;
}
inline int metroCatchableTrain(const int* eta,int count,int walking) {
  for(int i=0;i<count;i++) if(eta[i]>=0 && eta[i]>=walking) return i;
  return -1;
}
// Unknown, too late, two-minute margin, comfortable margin.
inline int metroBoardingState(int eta,int walking) {
  return eta<0?-1:eta<walking?0:eta-walking<=120?1:2;
}

inline int metroSynchronizedGauge(int start, int end, int initialEta, int remaining) {
  if (remaining < 0) return start;
  return std::min(end,start+(end-start)*std::max(0,initialEta-remaining)/std::max(1,initialEta));
}

// Location-based estimates include a short dwell allowance when the source says
// the train is entering/stopped at an upstream station. They never imply arrival.
inline int metroEstimateSeconds(int travelSeconds, bool atTarget, int code, int ageSeconds) {
  if (atTarget) return (code == 0 || code == 1) ? 0 : -1;
  if (travelSeconds <= 0) return -1;
  int dwell = (code == 4 || code == 5 || code == 99) ? 30 : 0;
  return std::max(1, travelSeconds + dwell - std::max(0, ageSeconds));
}

inline int metroGaugeX(int remainingSeconds, int finalLegSeconds, int precedingLegSeconds) {
  if (remainingSeconds <= 0) return 201;
  if (finalLegSeconds <= 0 || precedingLegSeconds <= 0) return 39;
  if (remainingSeconds <= finalLegSeconds)
    return 201 - 81 * remainingSeconds / finalLegSeconds;
  int previousRemaining = remainingSeconds - finalLegSeconds;
  return std::max(39, 120 - 81 * previousRemaining / precedingLegSeconds);
}

inline int metroArrivalEstimate(int departureInterval, bool atTarget, int code, int ageSeconds, int dwellSeconds) {
  if (atTarget) return metroEstimateSeconds(0, true, code, ageSeconds);
  if (departureInterval <= 0) return -1;
  int travel = std::max(1, departureInterval-dwellSeconds);
  if (code == 4 || code == 5 || code == 99) travel += dwellSeconds;
  return metroEstimateSeconds(travel, false, 3, ageSeconds);
}

inline int metroLearnedSeconds(int previous, int measured, int baseline) {
  if (baseline <= 0 || measured < baseline * 6 / 10 || measured > baseline * 16 / 10)
    return previous;
  return previous > 0 ? (previous * 3 + measured) / 4 : measured;
}
