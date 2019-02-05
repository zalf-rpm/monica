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
#include "crop-growth.h"
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
SoilTransport::SoilTransport(SoilColumn& sc, const SiteParameters& sps, const UserSoilTransportParameters& stPs,
                             double p_LeachingDepth, double p_timeStep, double pc_MinimumAvailableN)
  : soilColumn(sc),
    stPs(stPs),
    vs_NumberOfLayers(sc.vs_NumberOfLayers()), //extern
    vq_Convection(vs_NumberOfLayers, 0.0),
    vq_CropNUptake(0.0),
    vq_DiffusionCoeff(vs_NumberOfLayers, 0.0),
    vq_Dispersion(vs_NumberOfLayers, 0.0),
    vq_DispersionCoeff(vs_NumberOfLayers, 1.0),
    vq_FieldCapacity(vs_NumberOfLayers, 0.0),
    vq_LayerThickness(vs_NumberOfLayers,0.1),
    vq_LeachingAtBoundary(0.0),
    vs_NDeposition(sps.vq_NDeposition),
    vc_NUptakeFromLayer(vs_NumberOfLayers, 0.0),
    vq_PoreWaterVelocity(vs_NumberOfLayers, 0.0),
    vq_SoilMoisture(vs_NumberOfLayers, 0.2),
    vq_SoilNO3(vs_NumberOfLayers, 0.0),
    vq_SoilNO3_aq(vs_NumberOfLayers, 0.0),
    vq_TimeStep(1.0),
    vq_TotalDispersion(vs_NumberOfLayers, 0.0),
    vq_PercolationRate(vs_NumberOfLayers, 0.0),
    pc_MinimumAvailableN(pc_MinimumAvailableN)
{
  debug() << "!!! N Deposition: " << vs_NDeposition << endl;
  vs_LeachingDepth = p_LeachingDepth;
  vq_TimeStep = p_timeStep;
}

/**
 * @brief Single calculation step that is called by monica model.
 */
void SoilTransport::step() {
  calculateSoilTransportStep();
}

/**
 * @brief Computes a soil transport step
 */
void SoilTransport::calculateSoilTransportStep() {

  double vq_TimeStepFactor = 1.0; // [t t-1]

  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {
    vq_FieldCapacity[i_Layer] = soilColumn[i_Layer].vs_FieldCapacity();
    vq_SoilMoisture[i_Layer] = soilColumn[i_Layer].get_Vs_SoilMoisture_m3();
    vq_SoilNO3[i_Layer] = soilColumn[i_Layer].vs_SoilNO3;

    vq_LayerThickness[i_Layer] = soilColumn[0].vs_LayerThickness;
    vc_NUptakeFromLayer[i_Layer] = crop ? crop->get_NUptakeFromLayer(i_Layer) : 0;
    if (i_Layer == (vs_NumberOfLayers - 1)){
      vq_PercolationRate[i_Layer] = soilColumn.vs_FluxAtLowerBoundary ; //[mm]
    } else {
      vq_PercolationRate[i_Layer] = soilColumn[i_Layer + 1].vs_SoilWaterFlux; //[mm]
    }
    // Variable time step in case of high water fluxes to ensure stable numerics
    if ((vq_PercolationRate[i_Layer] <= 5.0) && (vq_TimeStepFactor >= 1.0))
      vq_TimeStepFactor = 1.0;
    else if ((vq_PercolationRate[i_Layer] > 5.0) && (vq_PercolationRate[i_Layer] <= 10.0) && (vq_TimeStepFactor >= 1.0))
      vq_TimeStepFactor = 0.5;
    else if ((vq_PercolationRate[i_Layer] > 10.0) && (vq_PercolationRate[i_Layer] <= 15.0) && (vq_TimeStepFactor >= 0.5))
      vq_TimeStepFactor = 0.25;
    else if ((vq_PercolationRate[i_Layer] > 15.0) && (vq_TimeStepFactor >= 0.25))
      vq_TimeStepFactor = 0.125;
	//vq_TimeStepFactor = 1.0; //debug
	//vq_PercolationRate[i_Layer] = 0.0; //debug
  }
//  cout << "vq_SoilNO3[0]: " << vq_SoilNO3[0] << endl;

//  if (isnan(vq_SoilNO3[0])) {
//      cout << "vq_SoilNO3[0]: " << "NAN" << endl;
//  }

  fq_NDeposition(vs_NDeposition);
  fq_NUptake();

  // Nitrate transport is called according to the set time step
  vq_LeachingAtBoundary = 0.0;
  for (int i_TimeStep = 0; i_TimeStep < (1.0 / vq_TimeStepFactor); i_TimeStep++) {
    fq_NTransport(vs_LeachingDepth, vq_TimeStepFactor);
  }

  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {

    vq_SoilNO3[i_Layer] = vq_SoilNO3_aq[i_Layer] * vq_SoilMoisture[i_Layer];

    if (vq_SoilNO3[i_Layer] < 0.0) {
      vq_SoilNO3[i_Layer] = 0.0;
    }

    soilColumn[i_Layer].vs_SoilNO3 = vq_SoilNO3[i_Layer];
  } // for

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
  double vq_CropNUptake = 0.0;

  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++)
  {
    // Lower boundary for N exploitation per layer
    if (vc_NUptakeFromLayer[i_Layer] > ((vq_SoilNO3[i_Layer] * vq_LayerThickness[i_Layer]) - pc_MinimumAvailableN)) {

      // Crop N uptake from layer i [kg N m-2]
      vc_NUptakeFromLayer[i_Layer] = ((vq_SoilNO3[i_Layer] * vq_LayerThickness[i_Layer]) - pc_MinimumAvailableN);
    }

    if (vc_NUptakeFromLayer[i_Layer] < 0) {
      vc_NUptakeFromLayer[i_Layer] = 0;
    }

    vq_CropNUptake += vc_NUptakeFromLayer[i_Layer];

    // Subtracting crop N uptake
    vq_SoilNO3[i_Layer] -= vc_NUptakeFromLayer[i_Layer] / vq_LayerThickness[i_Layer];

    // Calculation of solute NO3 concentration on the basis of the soil moisture
    // content before movement of current time step (kg m soil-3 --> kg m solute-3)
    vq_SoilNO3_aq[i_Layer] = vq_SoilNO3[i_Layer] / vq_SoilMoisture[i_Layer];
    if (vq_SoilNO3_aq[i_Layer] < 0) {
//        cout << "vq_SoilNO3_aq[i_Layer] < 0 " << endl;
    }

  } // for

  soilColumn.vq_CropNUptake = vq_CropNUptake; // [kg m-2]

}


/**
 * @brief Calculation of N transport
 * @param vs_LeachingDepth
 *
 * Kersebaum 1989
 */
void SoilTransport::fq_NTransport(double vs_LeachingDepth, double vq_TimeStepFactor) {

  double vq_DiffusionCoeffStandard = stPs.pq_DiffusionCoefficientStandard;// [m2 d-1]; old D0
  double AD = stPs.pq_AD; // Factor a in Kersebaum 1989 p.24 for Loess soils
  double vq_DispersionLength = stPs.pq_DispersionLength; // [m]
  double vq_SoilProfile = 0.0;
  int vq_LeachingDepthLayerIndex = 0;
  

  std::vector<double> vq_SoilMoistureGradient(vs_NumberOfLayers, 0.0);

  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {
    vq_SoilProfile += vq_LayerThickness[i_Layer];

    if ((vq_SoilProfile - 0.001) < vs_LeachingDepth) {
      vq_LeachingDepthLayerIndex = i_Layer;
    }
  }

  // Caluclation of convection for different cases of flux direction
  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++)
  {
    const double wf0 = soilColumn[0].vs_SoilWaterFlux;
    const double lt = soilColumn[i_Layer].vs_LayerThickness;
    const double NO3 = vq_SoilNO3_aq[i_Layer];
		
    if (i_Layer == 0) {
      const double pr = vq_PercolationRate[i_Layer] / 1000.0 * vq_TimeStepFactor; // [mm t-1 --> m t-1]
	  const double NO3_u = vq_SoilNO3_aq[i_Layer + 1];

      if (pr >= 0.0 && wf0 >= 0.0) {

        // old KONV = Konvektion Diss S. 23
        vq_Convection[i_Layer] = (NO3 * pr) / lt; //[kg m-3] * [m t-1] / [m]

      } else if (pr >= 0 && wf0 < 0) {

        vq_Convection[i_Layer] = (NO3 * pr) / lt;

      } else if (pr < 0 && wf0 < 0) {
        vq_Convection[i_Layer] = (NO3_u * pr) / lt;

      } else if (pr < 0 && wf0 >= 0) {

        vq_Convection[i_Layer] = (NO3_u * pr) / lt;
      }

    } else if (i_Layer < vs_NumberOfLayers - 1) {

      // layer > 0 && < bottom
      const double pr_o = vq_PercolationRate[i_Layer - 1] / 1000.0 * vq_TimeStepFactor; //[mm t-1 --> m t-1] * [t t-1]
      const double pr = vq_PercolationRate[i_Layer] / 1000.0 * vq_TimeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
      const double NO3_u = vq_SoilNO3_aq[i_Layer + 1];
	  
      if (pr >= 0.0 && pr_o >= 0.0) {
        const double NO3_o = vq_SoilNO3_aq[i_Layer - 1];

        // old KONV = Konvektion Diss S. 23
        vq_Convection[i_Layer] = ((NO3 * pr) - (NO3_o * pr_o)) / lt;

      } else if (pr >= 0 && pr_o < 0) {

        vq_Convection[i_Layer] = ((NO3 * pr) - (NO3 * pr_o)) / lt;

      } else if (pr < 0 && pr_o < 0) {

        vq_Convection[i_Layer] = ((NO3_u * pr) - (NO3 * pr_o)) / lt;

      } else if (pr < 0 && pr_o >= 0) {
        const double NO3_o = vq_SoilNO3_aq[i_Layer - 1];
        vq_Convection[i_Layer] = ((NO3_u * pr) - (NO3_o * pr_o)) / lt;
      }

    } else {

      // bottom layer
      const double pr_o = vq_PercolationRate[i_Layer - 1] / 1000.0 * vq_TimeStepFactor; // [m t-1] * [t t-1]
      const double pr = soilColumn.vs_FluxAtLowerBoundary / 1000.0 * vq_TimeStepFactor; // [m t-1] * [t t-1]

      if (pr >= 0.0 && pr_o >= 0.0) {
        const double NO3_o = vq_SoilNO3_aq[i_Layer - 1];

        // KONV = Konvektion Diss S. 23
        vq_Convection[i_Layer] = ((NO3 * pr) - (NO3_o * pr_o)) / lt;

      } else if (pr >= 0 && pr_o < 0) {

        vq_Convection[i_Layer] = ((NO3 * pr) - (NO3 * pr_o)) / lt;

      } else if (pr < 0 && pr_o < 0) {

        vq_Convection[i_Layer] = (-(NO3 * pr_o)) / lt;

      } else if (pr < 0 && pr_o >= 0) {
        const double NO3_o = vq_SoilNO3_aq[i_Layer - 1];
        vq_Convection[i_Layer] = (-(NO3_o * pr_o)) / lt;
      }

	  //vq_Convection[i_Layer] = 0.0;//debug

    }// else
  } // for


  // Calculation of dispersion depending of pore water velocity
  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {

    const double pr = vq_PercolationRate[i_Layer] / 1000.0 * vq_TimeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
    const double pr0 = soilColumn[0].vs_SoilWaterFlux / 1000.0 * vq_TimeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
    const double lt = soilColumn[i_Layer].vs_LayerThickness;
    const double NO3 = vq_SoilNO3_aq[i_Layer];


    // Original: W(I) --> um Steingehalt korrigierte Feldkapazität
    /** @todo Claas: generelle Korrektur der Feldkapazität durch den Steingehalt */
    if (i_Layer == vs_NumberOfLayers - 1) {
      vq_PoreWaterVelocity[i_Layer] = fabs((pr) / vq_FieldCapacity[i_Layer]); // [m t-1]
      vq_SoilMoistureGradient[i_Layer] = (vq_SoilMoisture[i_Layer]); //[m3 m-3]
    } else {
      vq_PoreWaterVelocity[i_Layer] = fabs((pr) / ((vq_FieldCapacity[i_Layer]
			        + vq_FieldCapacity[i_Layer + 1]) * 0.5)); // [m t-1]
      vq_SoilMoistureGradient[i_Layer] = ((vq_SoilMoisture[i_Layer])
				 + (vq_SoilMoisture[i_Layer + 1])) * 0.5; //[m3 m-3]
    }

    vq_DiffusionCoeff[i_Layer] = vq_DiffusionCoeffStandard
			   * (AD * exp(vq_SoilMoistureGradient[i_Layer] * 2.0 * 5.0)
			   / vq_SoilMoistureGradient[i_Layer]) * vq_TimeStepFactor; //[m2 t-1] * [t t-1]

    // Dispersion coefficient, old DB
    if (i_Layer == 0) {

      vq_DispersionCoeff[i_Layer] = vq_SoilMoistureGradient[i_Layer] * (vq_DiffusionCoeff[i_Layer] // [m2 t-1]
	+ vq_DispersionLength * vq_PoreWaterVelocity[i_Layer]) // [m] * [m t-1]
	- (0.5 * lt * fabs(pr)) // [m] * [m t-1]
	+ ((0.5 * vq_TimeStep * vq_TimeStepFactor * fabs((pr + pr0) / 2.0))  // [t] * [t t-1] * [m t-1]
	* vq_PoreWaterVelocity[i_Layer]); // * [m t-1]
	//-->[m2 t-1]
    } else {
      const double pr_o = vq_PercolationRate[i_Layer - 1] / 1000.0 * vq_TimeStepFactor; // [m t-1]

      vq_DispersionCoeff[i_Layer] = vq_SoilMoistureGradient[i_Layer] * (vq_DiffusionCoeff[i_Layer]
	+ vq_DispersionLength * vq_PoreWaterVelocity[i_Layer]) - (0.5 * lt * fabs(pr))
	+ ((0.5 * vq_TimeStep * vq_TimeStepFactor * fabs((pr + pr_o) / 2.0)) * vq_PoreWaterVelocity[i_Layer]);
    }

    //old DISP = Gesamt-Dispersion (D in Diss S. 23)
    if (i_Layer == 0) {
      const double NO3_u = vq_SoilNO3_aq[i_Layer + 1];
      // vq_Dispersion = Dispersion upwards or downwards, depending on the position in the profile [kg m-3]
      vq_Dispersion[i_Layer] = -vq_DispersionCoeff[i_Layer] * (NO3 - NO3_u) / (lt * lt); // [m2] * [kg m-3] / [m2]

    } else if (i_Layer < vs_NumberOfLayers - 1) {
      const double NO3_o = vq_SoilNO3_aq[i_Layer - 1];
      const double NO3_u = vq_SoilNO3_aq[i_Layer + 1];
      vq_Dispersion[i_Layer] = (vq_DispersionCoeff[i_Layer - 1] * (NO3_o - NO3) / (lt * lt))
	- (vq_DispersionCoeff[i_Layer] * (NO3 - NO3_u) / (lt * lt));
    } else {
      const double NO3_o = vq_SoilNO3_aq[i_Layer - 1];
      vq_Dispersion[i_Layer] = vq_DispersionCoeff[i_Layer - 1] * (NO3_o - NO3) / (lt * lt);
    }
	//vq_Dispersion[i_Layer] = 0.0; //debug
  } // for
  
  
  if (vq_PercolationRate[vq_LeachingDepthLayerIndex] > 0.0) {

    //vq_LeachingDepthLayerIndex = gewählte Auswaschungstiefe
    const double lt = soilColumn[vq_LeachingDepthLayerIndex].vs_LayerThickness;
    const double NO3 = vq_SoilNO3_aq[vq_LeachingDepthLayerIndex];

    if (vq_LeachingDepthLayerIndex < vs_NumberOfLayers - 1) {
      const double pr_u = vq_PercolationRate[vq_LeachingDepthLayerIndex + 1] / 1000.0 * vq_TimeStepFactor;// [m t-1]
      const double NO3_u = vq_SoilNO3_aq[vq_LeachingDepthLayerIndex + 1]; // [kg m-3]
      //vq_LeachingAtBoundary: Summe für Auswaschung (Diff + Konv), old OUTSUM
      vq_LeachingAtBoundary += ((pr_u * NO3) / lt * 10000.0 * lt) + ((vq_DispersionCoeff[vq_LeachingDepthLayerIndex]
	* (NO3 - NO3_u)) / (lt * lt) * 10000.0 * lt); //[kg ha-1]
    } else {
      const double pr_u = soilColumn.vs_FluxAtLowerBoundary / 1000.0 * vq_TimeStepFactor; // [m t-1]
      vq_LeachingAtBoundary += pr_u * NO3 / lt * 10000.0 * lt; //[kg ha-1]
    }

  } else {

    const double pr_u = vq_PercolationRate[vq_LeachingDepthLayerIndex] / 1000.0 * vq_TimeStepFactor;
    const double lt = soilColumn[vq_LeachingDepthLayerIndex].vs_LayerThickness;
    const double NO3 = vq_SoilNO3_aq[vq_LeachingDepthLayerIndex];

    if (vq_LeachingDepthLayerIndex < vs_NumberOfLayers - 1) {
      const double NO3_u = vq_SoilNO3_aq[vq_LeachingDepthLayerIndex + 1];
      vq_LeachingAtBoundary += ((pr_u * NO3_u) / (lt * 10000.0 * lt)) + vq_DispersionCoeff[vq_LeachingDepthLayerIndex]
	* (NO3 - NO3_u) / ((lt * lt) * 10000.0 * lt); //[kg ha-1]
    }
  }

  // Update of NO3 concentration
  // including transfomation back into [kg NO3-N m soil-3]
  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {


	  vq_SoilNO3_aq[i_Layer] += (vq_Dispersion[i_Layer] - vq_Convection[i_Layer]) / vq_SoilMoisture[i_Layer];
	  //    double no3 = vq_SoilNO3_aq[i_Layer];
	  //    double disp = vq_Dispersion[i_Layer];
	  //    double conv = vq_Convection[i_Layer];
	  //    double sm = vq_SoilMoisture[i_Layer];
	  //    cout << i_Layer << "\t" << no3 << "\t" << disp << "\t" << conv << "\t" <<  sm << endl;
  }

  

//  cout << "vq_LeachingAtBoundary: " << vq_LeachingAtBoundary << endl;
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

void SoilTransport::put_Crop(CropGrowth* c) {
  crop = c;
}

void SoilTransport::remove_Crop() {
  crop = NULL;
}

