/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#ifndef __SOIL_TEMPERATURE_H
#define __SOIL_TEMPERATURE_H

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
 */

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <iomanip>

#include "soilcolumn.h"

namespace Monica
{
  // forward declaration
  class SoilColumn;
  class MonicaModel;

  //! Calculation of soil temperature is based on a model developed by PIC
  class SoilTemperature
  {
  public:
    SoilTemperature(MonicaModel& monica, const SoilTemperatureModuleParameters& params);

    SoilTemperature(MonicaModel& monica, mas::models::monica::SoilTemperatureModuleState::Reader reader);
    void deserialize(mas::models::monica::SoilTemperatureModuleState::Reader reader);
    void serialize(mas::models::monica::SoilTemperatureModuleState::Builder builder) const;

    void step(double tmin, double tmax, double globrad);

    double f_SoilSurfaceTemperature(double tmin, double tmax, double globrad);
    double get_SoilSurfaceTemperature() const;
    double get_SoilTemperature(int layer) const;
    double get_HeatConductivity(int layer) const;
    double get_AvgTopSoilTemperature(double sumUpLayerThickness = 0.3) const;

    double dampingFactor() const { return _dampingFactor; }
    void setDampingFactor(double factor) { _dampingFactor = factor; }

    double vt_SoilSurfaceTemperature;

  private:
    SoilColumn& _soilColumn;
    MonicaModel& monica;
    SoilLayer _soilColumn_vt_GroundLayer;
    SoilLayer _soilColumn_vt_BottomLayer;
    SoilTemperatureModuleParameters _params;

    /*
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
    };*/
    struct SC {
      SoilColumn* sc;
      SoilLayer* gl;
      SoilLayer* bl;
      std::size_t vs_nols;
      SC(SoilColumn* sc,
        SoilLayer* gl,
        SoilLayer* bl,
        size_t vs_nols)
        : sc(sc)
        , gl(gl)
        , bl(bl)
        , vs_nols(vs_nols) {}

      SoilLayer& operator[](std::size_t i) const {
        if (i < vs_nols)
          return sc->at(i);
        else if (i < vs_nols + 1)
          return *gl;

        return *bl;
      }

      /*
      SC& operator=(SC& other) {
        sc = other.sc;
        gl = other.gl;
        bl = other.bl;
        vs_nols = other.vs_nols;
        return *this;
      }
      */

      SoilLayer& at(std::size_t i) { return (*this)[i]; }
      inline const SoilLayer& at(std::size_t i) const { return (*this)[i]; }
    };
    kj::Own<SC> soilColumn;

    const std::size_t vt_NumberOfLayers;
    const std::size_t vs_NumberOfLayers;
    std::vector<double> vs_SoilMoisture_const;
    std::vector<double> vt_SoilTemperature;
    std::vector<double> vt_V;
    std::vector<double> vt_VolumeMatrix;
    std::vector<double> vt_VolumeMatrixOld;
    std::vector<double> vt_B;
    std::vector<double> vt_MatrixPrimaryDiagonal;
    std::vector<double> vt_MatrixSecundaryDiagonal;
    double vt_HeatFlow{ 0.0 };
    std::vector<double> vt_HeatConductivity;
    std::vector<double> vt_HeatConductivityMean;
    std::vector<double> vt_HeatCapacity;
    double _dampingFactor{0.8};
  };
}
#endif

