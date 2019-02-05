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

#ifndef SOILTRANSPORT_H
#define SOILTRANSPORT_H

/**
 * @file soiltransport.h
 */

#include <vector>
#include "monica-parameters.h"

namespace Monica 
{
  // forward declarations
  class SoilColumn;
  class CropGrowth;

  /**
  * @brief Soil matter transport part of model
  *
  * @author Claas Nendel, Michael Berg
  *
  */
  class SoilTransport
  {
  public:
    SoilTransport(SoilColumn& soilColumn,
                  const SiteParameters& sps,
                  const UserSoilTransportParameters& stPs,
                  double p_LeachingDepth,
                  double p_timeStep,
                  double pc_MinimumAvailableN);

    void step();

    //! calculates daily N deposition
    void fq_NDeposition(double vs_NDeposition);

    //! puts crop N uptake into effect
    void fq_NUptake();

    //! calcuates N transport in soil
    void fq_NTransport (double vs_LeachingDepth, double vq_TimeStep);

    void put_Crop(CropGrowth* crop);

    void remove_Crop();

    double get_SoilNO3(int i_Layer) const;

    double get_NLeaching() const;

	//debug
	double get_vq_Dispersion(int i_Layer) const;
	double get_vq_Convection(int i_Layer) const;

  private:
    //methods
    void calculateSoilTransportStep();

    // members
    SoilColumn& soilColumn;
    const UserSoilTransportParameters& stPs;
    const int vs_NumberOfLayers;
    std::vector<double> vq_Convection;
    double vq_CropNUptake;
    std::vector<double> vq_DiffusionCoeff;
    std::vector<double> vq_Dispersion;
    std::vector<double> vq_DispersionCoeff;
    std::vector<double> vq_FieldCapacity;
    std::vector<double> vq_LayerThickness;
    double vs_LeachingDepth; //!< [m]
    double vq_LeachingAtBoundary;
    double vs_NDeposition; //!< [kg N ha-1 y-1]
    std::vector<double> vc_NUptakeFromLayer;	//! Pflanzenaufnahme aus der Tiefe Z; C1 N-Konzentration [kg N ha-1]
    std::vector<double> vq_PoreWaterVelocity;
    std::vector<double> vs_SoilMineralNContent;
    std::vector<double> vq_SoilMoisture;
    std::vector<double> vq_SoilNO3;
    std::vector<double> vq_SoilNO3_aq;
    double vq_TimeStep;
    double vq_CurrentTimeStep;
    std::vector<double> vq_TotalDispersion;
    std::vector<double> vq_PercolationRate; //!< Soil water flux from above [mm d-1]

    const double pc_MinimumAvailableN; //! kg m-2

    CropGrowth* crop{nullptr};
  };

} /* namespace Monica */


#endif // SOILTRANSPORT_H
