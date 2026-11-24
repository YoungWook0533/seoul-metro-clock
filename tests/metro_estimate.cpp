#include "metro_estimate.h"
#include <cassert>
#include <iostream>
int main() {
  const int arrivals[]={120,240,420,600};
  assert(metroCatchableTrain(arrivals,4,300)==2);
  assert(metroCatchableTrain(arrivals,4,700)==-1);
  assert(metroCatchableTrain(arrivals,4,240)==1);
  assert(metroMainTrain(299,600,300)==1);
  assert(metroMainTrain(300,600,300)==0);
  assert(metroMainTrain(120,240,300)==-1);
  assert(metroMainTrain(120,-1,300)==-1);
  assert(metroBoardingState(299,300)==0);
  assert(metroBoardingState(300,300)==1);
  assert(metroBoardingState(420,300)==1);
  assert(metroBoardingState(421,300)==2);
  assert(metroBoardingState(-1,300)==-1);
  assert(metroSynchronizedGauge(120,201,300,300)==120);
  assert(metroSynchronizedGauge(120,201,300,150)==160);
  assert(metroSynchronizedGauge(120,201,300,0)==201);
  assert(metroSynchronizedGauge(39,201,300,0)==201);
  assert(metroSynchronizedGauge(39,201,300,150)==120);
  assert(metroSynchronizedGauge(120,201,300,350)==120);
  int prior=39;
  for(int remaining=300;remaining>=0;--remaining){int x=metroSynchronizedGauge(39,119,300,remaining);assert(x>=prior&&x<=119);prior=x;}
  assert(metroEstimateSeconds(450, false, 3, 90) == 360);
  assert(metroEstimateSeconds(450, false, 5, 0) == 480);
  assert(metroEstimateSeconds(690, false, 99, 40) == 680);
  assert(metroEstimateSeconds(450, false, 3, 900) == 1);
  assert(metroEstimateSeconds(0, true, 0, 10) == 0);
  assert(metroEstimateSeconds(0, true, 1, 10) == 0);
  assert(metroEstimateSeconds(0, true, 99, 10) == -1);
  assert(metroEstimateSeconds(0, false, 99, 0) == -1);
  assert(metroEstimateSeconds(450, false, 3, -10) == 450);
  assert(metroGaugeX(690,450,240) == 39);
  assert(metroGaugeX(450,450,240) == 120);
  assert(metroGaugeX(225,450,240) > 120);
  assert(metroGaugeX(225,450,240) < 201);
  assert(metroGaugeX(0,450,240) == 201);
  assert(metroGaugeX(900,450,240) == 39);
  assert(metroLearnedSeconds(0,400,450) == 400);
  assert(metroLearnedSeconds(400,440,450) == 410);
  assert(metroLearnedSeconds(400,1200,450) == 400);
  assert(metroArrivalEstimate(450,false,3,90,30) == 330);
  assert(metroArrivalEstimate(450,false,5,90,30) == 360);
  assert(metroArrivalEstimate(690,false,3,0,30) == 660);
  assert(metroArrivalEstimate(690,false,5,0,30) == 690);
  assert(metroArrivalEstimate(0,true,1,0,30) == 0);
  assert(metroArrivalEstimate(0,false,3,0,30) == -1);
  std::cout << "PASS: source-age correction, station dwell, arrival, unknown and overdue estimates\n";
}
