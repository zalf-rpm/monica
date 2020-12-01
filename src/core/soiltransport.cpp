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

#include <algorithm> //for min, max
#include <iostream>
#include <cmath>
#include <math.h>

#include "soilmoisture.h"
#include "soilcolumn.h"
#include "soiltransport.h"
#include "crop-module.h"
#include "tools/debug.h"
#include "tools/debug.h"

using namespace std;
using namespace Monica;
using namespace Tools;

/**
 * @brief Constructor
 * @param sc Soil column
 *
 * Parameter initialization
 *
 * @author Claas Nendel
 */
SoilTransport::SoilTransport(SoilColumn& sc, const SiteParameters& sps, const SoilTransportModuleParameters& params,
  double p_LeachingDepth, double p_timeStep, double pc_MinimumAvailableN)
  : soilColumn(sc)
  , _params(params)
  //, vs_NumberOfLayers(sc.vs_NumberOfLayers()) //extern
  , vq_Convection(sc.vs_NumberOfLayers(), 0.0)
  , vq_DiffusionCoeff(sc.vs_NumberOfLayers(), 0.0)
  , vq_Dispersion(sc.vs_NumberOfLayers(), 0.0)
  , vq_DispersionCoeff(sc.vs_NumberOfLayers(), 1.0)
  //, vq_FieldCapacity(sc.vs_NumberOfLayers(), 0.0)
  , vs_LeachingDepth(p_LeachingDepth)
  , vs_NDeposition(sps.vq_NDeposition)
  , vc_NUptakeFromLayer(sc.vs_NumberOfLayers(), 0.0)
  , vq_PoreWaterVelocity(sc.vs_NumberOfLayers(), 0.0)
  //, vq_SoilMoisture(sc.vs_NumberOfLayers(), 0.2)
  , vq_SoilNO3(sc.vs_NumberOfLayers(), 0.0)
  , vq_SoilNO3_aq(sc.vs_NumberOfLayers(), 0.0)
  , vq_TimeStep(p_timeStep)
  , vq_TotalDispersion(sc.vs_NumberOfLayers(), 0.0)
  , vq_PercolationRate(sc.vs_NumberOfLayers(), 0.0)
  , pc_MinimumAvailableN(pc_MinimumAvailableN)
{
  debug() << "!!! N Deposition: " << vs_NDeposition << endl;
}

void SoilTransport::deserialize(mas::models::monica::SoilTransportModuleState::Reader reader) {

}

void SoilTransport::serialize(mas::models::monica::SoilTransportModuleState::Builder builder) const {
  builder.initModuleParams();
  _params.serialize(builder.getModuleParams());
}


/**
 * @brief Computes a soil transport step
 */
void SoilTransport::step() {
  double minTimeStepFactor = 1.0; // [t t-1]
  const auto nols = soilColumn.vs_NumberOfLayers();

  for (size_t i = 0; i < nols; i++) {
    //vq_FieldCapacity[i] = soilColumn[i].vs_FieldCapacity();
    //vq_SoilMoisture[i] = soilColumn[i].get_Vs_SoilMoisture_m3();
    vq_SoilNO3[i] = soilColumn[i].vs_SoilNO3;

    vc_NUptakeFromLayer[i] = cropModule ? cropModule->get_NUptakeFromLayer(i) : 0;
    if (i == nols - 1) 
      vq_PercolationRate[i] = soilColumn.vs_FluxAtLowerBoundary; //[mm]
    else
      vq_PercolationRate[i] = soilColumn[i + 1].vs_SoilWaterFlux; //[mm]

    // Variable time step in case of high water fluxes to ensure stable numerics
    auto pri = vq_PercolationRate[i];
    auto timeStepFactorCurrentLayer = minTimeStepFactor;
    if (-5.0 <= pri && pri <= 5.0 && minTimeStepFactor > 1.0)
      timeStepFactorCurrentLayer = 1.0;
    else if ((-10.0 <= pri && pri < -5.0) || (5.0 < pri && pri <= 10.0))
      timeStepFactorCurrentLayer = 0.5;
    else if ((-15.0 <= pri && pri < -10.0) || (10.0 < pri && pri <= 15.0))
      timeStepFactorCurrentLayer = 0.25;
    else if (pri < -15.0 || pri > 15.0)
      timeStepFactorCurrentLayer = 0.125;

    minTimeStepFactor = min(minTimeStepFactor, timeStepFactorCurrentLayer);
  }

  fq_NDeposition(vs_NDeposition);
  fq_NUptake();

  // Nitrate transport is called according to the set time step
  vq_LeachingAtBoundary = 0.0;
  for (int i_TimeStep = 0; i_TimeStep < (1.0 / minTimeStepFactor); i_TimeStep++) 
    fq_NTransport(vs_LeachingDepth, minTimeStepFactor);

  for (int i = 0; i < nols; i++) {
    vq_SoilNO3[i] = vq_SoilNO3_aq[i] * soilColumn[i].get_Vs_SoilMoisture_m3();

    if (vq_SoilNO3[i] < 0.0) 
      vq_SoilNO3[i] = 0.0;

    soilColumn[i].vs_SoilNO3 = vq_SoilNO3[i];
  } 

}


/**
 * @brief Calculation of N deposition
 * Transformation of annual N Deposition into a daily value,
 * that can be used in MONICAs calculations. Addition of this
 * transformed N deposition to ammonium pool of top soil layer.
 *
 * @param vs_NDeposition
 *
 * Kersebaum 1989
 */
void SoilTransport::fq_NDeposition(double vs_NDeposition) {
  //Daily N deposition in [kg N ha-1 d-1]
  double vq_DailyNDeposition = vs_NDeposition / 365.0;

  // Addition of N deposition to top layer [kg N m-3]
  vq_SoilNO3[0] += vq_DailyNDeposition / (10000.0 * soilColumn[0].vs_LayerThickness);
}

/**
 * @brief Calculation of crop N uptake
 * @param
 *
 * Kersebaum 1989
 */
void SoilTransport::fq_NUptake() {
  const auto nols = soilColumn.vs_NumberOfLayers();
  double vq_CropNUptake = 0.0;
  for (int i = 0; i < nols; i++)
  {
    const auto lti = soilColumn[i].vs_LayerThickness;
    const auto smi = soilColumn[i].get_Vs_SoilMoisture_m3();

    // Lower boundary for N exploitation per layer
    if (vc_NUptakeFromLayer[i] > ((vq_SoilNO3[i] * lti) - pc_MinimumAvailableN))
      vc_NUptakeFromLayer[i] = ((vq_SoilNO3[i] * lti) - pc_MinimumAvailableN); // Crop N uptake from layer i [kg N m-2]

    if (vc_NUptakeFromLayer[i] < 0) 
      vc_NUptakeFromLayer[i] = 0;

    vq_CropNUptake += vc_NUptakeFromLayer[i];

    // Subtracting crop N uptake
    vq_SoilNO3[i] -= vc_NUptakeFromLayer[i] / lti;

    // Calculation of solute NO3 concentration on the basis of the soil moisture
    // content before movement of current time step (kg m soil-3 --> kg m solute-3)
    vq_SoilNO3_aq[i] = vq_SoilNO3[i] / smi;
  } 

  soilColumn.vq_CropNUptake = vq_CropNUptake; // [kg m-2]
}

/**
 * @brief Calculation of N transport
 * @param vs_LeachingDepth
 *
 * Kersebaum 1989
 */
void SoilTransport::fq_NTransport(double leachingDepth, double timeStepFactor) {
  double diffusionCoeffStandard = _params.pq_DiffusionCoefficientStandard; // [m2 d-1]; old D0
  double AD = _params.pq_AD; // Factor a in Kersebaum 1989 p.24 for Loess soils
  double dispersionLength = _params.pq_DispersionLength; // [m]
  double soilProfile = 0.0;
  size_t leachingDepthLayerIndex = 0;
  const auto nols = soilColumn.vs_NumberOfLayers();
  std::vector<double> soilMoistureGradient(nols, 0.0);

  for (size_t i = 0; i < nols; i++) {
    soilProfile += soilColumn[i].vs_LayerThickness;
    if ((soilProfile - 0.001) < leachingDepth) 
      leachingDepthLayerIndex = i;
  }

  // Caluclation of convection for different cases of flux direction
  for (size_t i = 0; i < nols; i++) {
    const auto wf0 = soilColumn[0].vs_SoilWaterFlux;
    const auto lt = soilColumn[i].vs_LayerThickness;
    const auto NO3 = vq_SoilNO3_aq[i];
		
    if (i == 0) {
      const double pr = vq_PercolationRate[i] / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1]
      const double NO3_u = vq_SoilNO3_aq[i + 1];

      if (pr >= 0.0 && wf0 >= 0.0) {
        vq_Convection[i] = (NO3 * pr) / lt; //[kg m-3] * [m t-1] / [m] // old KONV = Konvektion Diss S. 23
      } else if (pr >= 0 && wf0 < 0) {
        vq_Convection[i] = (NO3 * pr) / lt;
      } else if (pr < 0 && wf0 < 0) {
        vq_Convection[i] = (NO3_u * pr) / lt;
      } else if (pr < 0 && wf0 >= 0) {
        vq_Convection[i] = (NO3_u * pr) / lt;
      }

    } else if (i < nols - 1) {
      // layer > 0 && < bottom
      const double pr_o = vq_PercolationRate[i - 1] / 1000.0 * timeStepFactor; //[mm t-1 --> m t-1] * [t t-1]
      const double pr = vq_PercolationRate[i] / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
      const double NO3_u = vq_SoilNO3_aq[i + 1];
	  
      if (pr >= 0.0 && pr_o >= 0.0) {
        const double NO3_o = vq_SoilNO3_aq[i - 1];
        vq_Convection[i] = ((NO3 * pr) - (NO3_o * pr_o)) / lt; // old KONV = Konvektion Diss S. 23
      } else if (pr >= 0 && pr_o < 0) {
        vq_Convection[i] = ((NO3 * pr) - (NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o < 0) {
        vq_Convection[i] = ((NO3_u * pr) - (NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o >= 0) {
        const double NO3_o = vq_SoilNO3_aq[i - 1];
        vq_Convection[i] = ((NO3_u * pr) - (NO3_o * pr_o)) / lt;
      }
    } else {
      // bottom layer
      const double pr_o = vq_PercolationRate[i - 1] / 1000.0 * timeStepFactor; // [m t-1] * [t t-1]
      const double pr = soilColumn.vs_FluxAtLowerBoundary / 1000.0 * timeStepFactor; // [m t-1] * [t t-1]

      if (pr >= 0.0 && pr_o >= 0.0) {
        const double NO3_o = vq_SoilNO3_aq[i - 1];
        vq_Convection[i] = ((NO3 * pr) - (NO3_o * pr_o)) / lt; // KONV = Konvektion Diss S. 23
      } else if (pr >= 0 && pr_o < 0) {
        vq_Convection[i] = ((NO3 * pr) - (NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o < 0) {
        vq_Convection[i] = (-(NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o >= 0) {
        const double NO3_o = vq_SoilNO3_aq[i - 1];
        vq_Convection[i] = (-(NO3_o * pr_o)) / lt;
      }
    }// else
  } // for

  // Calculation of dispersion depending of pore water velocity
  for (size_t i = 0; i < nols; i++) {
    const auto pri = vq_PercolationRate[i] / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
    const auto pr0 = soilColumn[0].vs_SoilWaterFlux / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
    const auto lti = soilColumn[i].vs_LayerThickness;
    const auto NO3i = vq_SoilNO3_aq[i];
    const auto fci = soilColumn[i].vs_FieldCapacity();
    const auto smi = soilColumn[i].get_Vs_SoilMoisture_m3();

    // Original: W(I) --> um Steingehalt korrigierte Feldkapazität
    /** @todo Claas: generelle Korrektur der Feldkapazität durch den Steingehalt */
    if (i == nols - 1) {
      vq_PoreWaterVelocity[i] = fabs((pri) / fci); // [m t-1]
      soilMoistureGradient[i] = smi; //[m3 m-3]
    }
    else {
      const auto fcip1 = soilColumn[i + 1].vs_FieldCapacity();
      const auto smip1 = soilColumn[i + 1].get_Vs_SoilMoisture_m3();
      vq_PoreWaterVelocity[i] = fabs((pri) / ((fci + fcip1) * 0.5)); // [m t-1]
      soilMoistureGradient[i] = (smi + smip1) * 0.5; //[m3 m-3]
    }

    vq_DiffusionCoeff[i] = 
      diffusionCoeffStandard
      * (AD * exp(soilMoistureGradient[i] * 2.0 * 5.0)
        / soilMoistureGradient[i]) * timeStepFactor; //[m2 t-1] * [t t-1]

   // Dispersion coefficient, old DB
    if (i == 0) {
      vq_DispersionCoeff[i] = soilMoistureGradient[i] * (vq_DiffusionCoeff[i] // [m2 t-1]
        + dispersionLength * vq_PoreWaterVelocity[i]) // [m] * [m t-1]
        - (0.5 * lti * fabs(pri)) // [m] * [m t-1]
        + ((0.5 * vq_TimeStep * timeStepFactor * fabs((pri + pr0) / 2.0))  // [t] * [t t-1] * [m t-1]
          * vq_PoreWaterVelocity[i]); // * [m t-1]
          //-->[m2 t-1]
    }
    else {
      const double pr_o = vq_PercolationRate[i - 1] / 1000.0 * timeStepFactor; // [m t-1]

      vq_DispersionCoeff[i] = soilMoistureGradient[i] * (vq_DiffusionCoeff[i]
        + dispersionLength * vq_PoreWaterVelocity[i]) - (0.5 * lti * fabs(pri))
        + ((0.5 * vq_TimeStep * timeStepFactor * fabs((pri + pr_o) / 2.0)) * vq_PoreWaterVelocity[i]);
    }

    //old DISP = Gesamt-Dispersion (D in Diss S. 23)
    if (i == 0) {
      const double NO3_u = vq_SoilNO3_aq[i + 1];
      // vq_Dispersion = Dispersion upwards or downwards, depending on the position in the profile [kg m-3]
      vq_Dispersion[i] = -vq_DispersionCoeff[i] * (NO3i - NO3_u) / (lti * lti); // [m2] * [kg m-3] / [m2]
    }
    else if (i < nols - 1) {
      const double NO3_o = vq_SoilNO3_aq[i - 1];
      const double NO3_u = vq_SoilNO3_aq[i + 1];
      vq_Dispersion[i] = (vq_DispersionCoeff[i - 1] * (NO3_o - NO3i) / (lti * lti))
        - (vq_DispersionCoeff[i] * (NO3i - NO3_u) / (lti * lti));
    }
    else {
      const double NO3_o = vq_SoilNO3_aq[i - 1];
      vq_Dispersion[i] = vq_DispersionCoeff[i - 1] * (NO3_o - NO3i) / (lti * lti);
    }
  } // for
  
  if (vq_PercolationRate[leachingDepthLayerIndex] > 0.0) {
    //vq_LeachingDepthLayerIndex = gewählte Auswaschungstiefe
    const auto lt = soilColumn[leachingDepthLayerIndex].vs_LayerThickness;
    const auto NO3 = vq_SoilNO3_aq[leachingDepthLayerIndex];

    if (leachingDepthLayerIndex < nols - 1) {
      const double pr_u = vq_PercolationRate[leachingDepthLayerIndex + 1] / 1000.0 * timeStepFactor;// [m t-1]
      const double NO3_u = vq_SoilNO3_aq[leachingDepthLayerIndex + 1]; // [kg m-3]
      //vq_LeachingAtBoundary: Summe für Auswaschung (Diff + Konv), old OUTSUM
      vq_LeachingAtBoundary += ((pr_u * NO3) / lt * 10000.0 * lt) + ((vq_DispersionCoeff[leachingDepthLayerIndex]
        * (NO3 - NO3_u)) / (lt * lt) * 10000.0 * lt); //[kg ha-1]
    } else {
      const double pr_u = soilColumn.vs_FluxAtLowerBoundary / 1000.0 * timeStepFactor; // [m t-1]
      vq_LeachingAtBoundary += pr_u * NO3 / lt * 10000.0 * lt; //[kg ha-1]
    }
  } else {
    const auto pr_u = vq_PercolationRate[leachingDepthLayerIndex] / 1000.0 * timeStepFactor;
    const auto lt = soilColumn[leachingDepthLayerIndex].vs_LayerThickness;
    const auto NO3 = vq_SoilNO3_aq[leachingDepthLayerIndex];

    if (leachingDepthLayerIndex < nols - 1) {
      const double NO3_u = vq_SoilNO3_aq[leachingDepthLayerIndex + 1];
      vq_LeachingAtBoundary += ((pr_u * NO3_u) / (lt * 10000.0 * lt)) + vq_DispersionCoeff[leachingDepthLayerIndex]
        * (NO3 - NO3_u) / ((lt * lt) * 10000.0 * lt); //[kg ha-1]
    }
  }

  vq_LeachingAtBoundary = max(0.0, vq_LeachingAtBoundary);

  // Update of NO3 concentration
  // including transfomation back into [kg NO3-N m soil-3]
  for (size_t i = 0; i < nols; i++) {
    const auto smi = soilColumn[i].get_Vs_SoilMoisture_m3();
    vq_SoilNO3_aq[i] += (vq_Dispersion[i] - vq_Convection[i]) / smi;
  }
}

/**
 * @brief Returns Nitrate content for each layer [i]
 * @return Soil NO3 content
 */
double SoilTransport::get_SoilNO3(int i_Layer) const {
  return vq_SoilNO3[i_Layer];
}

/**
* @brief Returns Nitrate dispersion for each layer [i]
* @return Soil NO3 dispersion
*/
double SoilTransport::get_vq_Dispersion(int i_Layer) const {
	return vq_Dispersion[i_Layer];
}

/**
* @brief Returns Nitrate convection for each layer [i]
* @return Soil NO3 dispersion
*/
double SoilTransport::get_vq_Convection(int i_Layer) const {
	return vq_Convection[i_Layer];
}

/**
 * @brief Returns N leaching at leaching depth [kg ha-1]
 * @return Soil NO3 content
 */
double SoilTransport::get_NLeaching() const {
  return vq_LeachingAtBoundary;
}

