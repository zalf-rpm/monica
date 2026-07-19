/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <vector>

#include <kj/memory.h>

#include "soilcolumn_simple.h"
#include "monica-parameters.h"

namespace monica {

class MonicaModel;

namespace soiltemperature {

//! Calculation of soil temperature is based on a model developed by PIC
struct SoilTemperature {
  SoilColumn* soilColumn{nullptr};
  MonicaModel* monica{nullptr};
  SoilLayer soilColumnGroundLayer;
  SoilLayer soilColumnBottomLayer;
  SoilTemperatureModuleParameters params;

  std::size_t noOfTempLayers{0};
  std::size_t noOfSoilLayers{0};
  std::vector<double> soilTemperature;
  std::vector<double> V;
  std::vector<double> volumeMatrix;
  std::vector<double> volumeMatrixOld;
  std::vector<double> B;
  std::vector<double> matrixPrimaryDiagonal;
  std::vector<double> matrixSecondaryDiagonal;
  std::vector<double> heatConductivity;
  std::vector<double> heatConductivityMean;
  std::vector<double> heatCapacity;
  double dampingFactor{0.8};
  double soilSurfaceTemperature{0.0};
  std::vector<double> solution;
  std::vector<double> matrixDiagonal;
  std::vector<double> matrixLowerTriangle;
  std::vector<double> heatFlow;
};

kj::Own<SoilTemperature> makeSoilTemperature(MonicaModel& monica, const SoilTemperatureModuleParameters& params);
kj::Own<SoilTemperature> makeSoilTemperature(MonicaModel& monica,
                                             mas::schema::model::monica::SoilTemperatureModuleState::Reader reader);

void deserialize(SoilTemperature* st, mas::schema::model::monica::SoilTemperatureModuleState::Reader reader);
void serialize(const SoilTemperature* st, mas::schema::model::monica::SoilTemperatureModuleState::Builder builder);
void step(SoilTemperature* st, double tmin, double tmax, double globrad);
double calcSoilSurfaceTemperature(const SoilTemperature* st,
                                 double prevSoilSurfaceTemperature,
                                 double tmin,
                                 double tmax,
                                 double globrad);

} // namespace soiltemperature

using SoilTemperature = soiltemperature::SoilTemperature;

} // namespace monica
