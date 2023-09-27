/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
* @file soiltemperature.h
 *
 * @brief This file contains declarations of class SoilTemperature and SC,
 * a soil solumn wrapper.
 *
 * Soil element definition by Claas Nendel.
 * This file contains class definitions of SoilTemperature and the wrapper
 * class SC that simulates a soil column with two extra layers.
 *
 * @see SoilTemperature
 * @see SC

Authors: 
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg-mohnicke@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#pragma once

#include <vector>

#include "soilcolumn.h"
#include "monica-parameters.h"

#ifdef AMEI
#include "SoilTemperatureCompComponent.h"
#endif

namespace monica
{
class MonicaModel;

//! Calculation of soil temperature is based on a model developed by PIC
class SoilTemperature
{
public:
  SoilTemperature(MonicaModel& monica, const SoilTemperatureModuleParameters& params);

  SoilTemperature(MonicaModel& monica, mas::schema::model::monica::SoilTemperatureModuleState::Reader reader);
  void deserialize(mas::schema::model::monica::SoilTemperatureModuleState::Reader reader);
  void serialize(mas::schema::model::monica::SoilTemperatureModuleState::Builder builder) const;

  void step(double tmin, double tmax, double globrad);

  double getSoilSurfaceTemperature() const { return _soilSurfaceTemperature; }
  double getSoilTemperature(int layer) const { return soilColumn.at(layer).get_Vs_SoilTemperature();}

private:
  double calcSoilSurfaceTemperature(double prevSoilSurfaceTemperature, double tmin, double tmax, double globrad) const;

  SoilColumn& _soilColumn;
  MonicaModel& _monica;
  SoilLayer _soilColumnGroundLayer;
  SoilLayer _soilColumnBottomLayer;
  SoilTemperatureModuleParameters _params;

#ifdef AMEI
  Monica_SoilTemp::SoilTemperatureCompComponent _soilTempComp;
  Monica_SoilTemp::SoilTemperatureCompState _soilTempState;
  Monica_SoilTemp::SoilTemperatureCompState _soilTempState1;
  Monica_SoilTemp::SoilTemperatureCompExogenous _soilTempExo;
  Monica_SoilTemp::SoilTemperatureCompRate _soilTempRate;
  Monica_SoilTemp::SoilTemperatureCompAuxiliary _soilTempAux;
#endif

  struct SC {
    SoilColumn& sc;
    SoilLayer& gl;
    SoilLayer& bl;
    std::size_t vs_nols;
    SC(SoilColumn& sc,
      SoilLayer& gl,
      SoilLayer& bl,
      size_t vs_nols)
      : sc(sc)
      , gl(gl)
      , bl(bl)
      , vs_nols(vs_nols) {}

    SoilLayer& operator[](std::size_t i) const {
      if (i < vs_nols)
        return sc[i];
      else if (i < vs_nols + 1)
        return gl;

      return bl;
    }

    SoilLayer& at(std::size_t i) { return (*this)[i]; }
    inline const SoilLayer& at(std::size_t i) const { return (*this)[i]; }
  } soilColumn;

  std::size_t _noOfTempLayers;
  std::size_t _noOfSoilLayers;
  //std::vector<double> vs_SoilMoisture_const;
  std::vector<double> _soilTemperature;
  std::vector<double> _V;
  std::vector<double> _volumeMatrix;
  std::vector<double> _volumeMatrixOld;
  std::vector<double> _B;
  std::vector<double> _matrixPrimaryDiagonal;
  std::vector<double> _matrixSecondaryDiagonal;
  std::vector<double> _heatConductivity;
  std::vector<double> _heatConductivityMean;
  std::vector<double> _heatCapacity;
  double _dampingFactor{0.8};
  double _soilSurfaceTemperature {0.0};

  // moved to instance level from step() to avoid reallocation
  std::vector<double> _solution;
  std::vector<double> _matrixDiagonal;
  std::vector<double> _matrixLowerTriangle;
  std::vector<double> _heatFlow;
};

} // namespace monica

