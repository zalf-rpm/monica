/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
    SoilTransport(SoilColumn& soilColumn, const SiteParameters& sps, const CentralParameterProvider& cpp);
    ~SoilTransport();

    void step();
    void fq_NDeposition(double vs_NDeposition);  // calculates daily N deposition
    void fq_NUptake(); // puts crop N uptake into effect
    void fq_NTransport (double vs_LeachingDepth, double vq_TimeStep);  // calcuates N transport in soil
    void put_Crop(CropGrowth* crop);
    void remove_Crop();

    double get_SoilNO3(int i_Layer) const;
    double get_NLeaching() const;

  private:
    //methods
    void calculateSoilTransportStep();

    // members
    SoilColumn& soilColumn;
    const CentralParameterProvider& centralParameterProvider;
    const int vs_NumberOfLayers;
    std::vector<double> vq_Convection;
    double vq_CropNUptake;
    std::vector<double> vq_DiffusionCoeff;
    std::vector<double> vq_Dispersion;
    std::vector<double> vq_DispersionCoeff;
    std::vector<double> vq_FieldCapacity;
    std::vector<double> vq_LayerThickness;
    double vs_LeachingDepth; 						/**< [m] */
    double vq_LeachingAtBoundary;
    double vs_NDeposition; 						/**< [kg N ha-1 y-1] */
    std::vector<double> vc_NUptakeFromLayer;		/** Pflanzenaufnahme aus der Tiefe Z; C1 N-Konzentration [kg N ha-1] */
    std::vector<double> vq_PoreWaterVelocity;
    std::vector<double> vs_SoilMineralNContent;
    std::vector<double> vq_SoilMoisture;
    std::vector<double> vq_SoilNO3;
    std::vector<double> vq_SoilNO3_aq;
    double vq_TimeStep;
    double vq_CurrentTimeStep;
    std::vector<double> vq_TotalDispersion;
    std::vector<double> vq_PercolationRate;               /**< Soil water flux from above [mm d-1] */

    CropGrowth* crop;
  };

} /* namespace Monica */


#endif // SOILTRANSPORT_H
