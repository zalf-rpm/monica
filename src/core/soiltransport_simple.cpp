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

#include "soiltransport_simple.h"

#include <algorithm> //for min, max
#include <iostream>
#include <cmath>
#include <math.h>

#include "soilmoisture_simple.h"
#include "soilcolumn_simple.h"
#include "crop-module_simple.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

namespace monica {
namespace soiltransport {

kj::Own<SoilTransport> makeSoilTransport(SoilColumn& soilColumn,
                                                 const SiteParameters& sps,
                                                 const SoilTransportModuleParameters& params,
                                                 double leachingDepth,
                                                 double timeStep,
                                                 double minimumAvailableN) {
  auto st = kj::heap<SoilTransport>();
  st->soilColumn = &soilColumn;
  st->_params = params;
  st->vq_Convection.resize(soilColumn.size(), 0.0);
  st->vq_DiffusionCoeff.resize(soilColumn.size(), 0.0);
  st->vq_Dispersion.resize(soilColumn.size(), 0.0);
  st->vq_DispersionCoeff.resize(soilColumn.size(), 1.0);
  st->vs_LeachingDepth = leachingDepth;
  st->vs_NDeposition = sps.vq_NDeposition;
  st->vc_NUptakeFromLayer.resize(soilColumn.size(), 0.0);
  st->vq_PoreWaterVelocity.resize(soilColumn.size(), 0.0);
  st->vq_SoilNO3.resize(soilColumn.size(), 0.0);
  st->vq_SoilNO3_aq.resize(soilColumn.size(), 0.0);
  st->vq_TimeStep = timeStep;
  st->vq_TotalDispersion.resize(soilColumn.size(), 0.0);
  st->vq_PercolationRate.resize(soilColumn.size(), 0.0);
  st->pc_MinimumAvailableN = minimumAvailableN;

  debug() << "!!! N Deposition: " << st->vs_NDeposition << endl;
  return st;
}

kj::Own<SoilTransport> makeSoilTransport(SoilColumn& soilColumn,
                                                 mas::schema::model::monica::SoilTransportModuleState::Reader reader,
                                                 CropModule* cropModule) {
  auto st = kj::heap<SoilTransport>();
  st->soilColumn = &soilColumn;
  st->cropModule = cropModule;
  deserialize(st.get(), reader);
  return st;
}

void deserialize(SoilTransport* st, mas::schema::model::monica::SoilTransportModuleState::Reader reader) {
  st->_params.deserialize(reader.getModuleParams());
  setFromCapnpList(st->vq_Convection, reader.getConvection());
  setFromCapnpList(st->vq_DiffusionCoeff, reader.getDiffusionCoeff());
  setFromCapnpList(st->vq_Dispersion, reader.getDispersion());
  setFromCapnpList(st->vq_DispersionCoeff, reader.getDispersionCoeff());
  st->vs_LeachingDepth = reader.getVsLeachingDepth();
  st->vq_LeachingAtBoundary = reader.getLeachingAtBoundary();
  st->vs_NDeposition = reader.getVsNDeposition();
  setFromCapnpList(st->vc_NUptakeFromLayer, reader.getVcNUptakeFromLayer());
  setFromCapnpList(st->vq_PoreWaterVelocity, reader.getPoreWaterVelocity());
  setFromCapnpList(st->vs_SoilMineralNContent, reader.getVsSoilMineralNContent());
  setFromCapnpList(st->vq_SoilNO3, reader.getSoilNO3());
  setFromCapnpList(st->vq_SoilNO3_aq, reader.getSoilNO3aq());
  st->vq_TimeStep = reader.getTimeStep();
  setFromCapnpList(st->vq_TotalDispersion, reader.getTotalDispersion());
  setFromCapnpList(st->vq_PercolationRate, reader.getPercolationRate());
  st->pc_MinimumAvailableN = reader.getPcMinimumAvailableN();
}

void serialize(const SoilTransport* st, mas::schema::model::monica::SoilTransportModuleState::Builder builder) {
  st->_params.serialize(builder.initModuleParams());
  setCapnpList(st->vq_Convection, builder.initConvection((capnp::uint)st->vq_Convection.size()));
  setCapnpList(st->vq_DiffusionCoeff, builder.initDiffusionCoeff((capnp::uint)st->vq_DiffusionCoeff.size()));
  setCapnpList(st->vq_Dispersion, builder.initDispersion((capnp::uint)st->vq_Dispersion.size()));
  setCapnpList(st->vq_DispersionCoeff, builder.initDispersionCoeff((capnp::uint)st->vq_DispersionCoeff.size()));
  builder.setVsLeachingDepth(st->vs_LeachingDepth);
  builder.setLeachingAtBoundary(st->vq_LeachingAtBoundary);
  builder.setVsNDeposition(st->vs_NDeposition);
  setCapnpList(st->vc_NUptakeFromLayer, builder.initVcNUptakeFromLayer((capnp::uint)st->vc_NUptakeFromLayer.size()));
  setCapnpList(st->vq_PoreWaterVelocity, builder.initPoreWaterVelocity((capnp::uint)st->vq_PoreWaterVelocity.size()));
  setCapnpList(st->vs_SoilMineralNContent, builder.initVsSoilMineralNContent((capnp::uint)st->vs_SoilMineralNContent.size()));
  setCapnpList(st->vq_SoilNO3, builder.initSoilNO3((capnp::uint)st->vq_SoilNO3.size()));
  setCapnpList(st->vq_SoilNO3_aq, builder.initSoilNO3aq((capnp::uint)st->vq_SoilNO3_aq.size()));
  builder.setTimeStep(st->vq_TimeStep);
  setCapnpList(st->vq_TotalDispersion, builder.initTotalDispersion((capnp::uint)st->vq_TotalDispersion.size()));
  setCapnpList(st->vq_PercolationRate, builder.initPercolationRate((capnp::uint)st->vq_PercolationRate.size()));
  builder.setPcMinimumAvailableN(st->pc_MinimumAvailableN);
}


/**
 * @brief Computes a soil transport step
 */
void step(SoilTransport* st) {
  double minTimeStepFactor = 1.0; // [t t-1]
  const auto nols = st->soilColumn->size();

  for (size_t i = 0; i < nols; i++) {
    //vq_FieldCapacity[i] = soilColumn[i]._sps.vs_FieldCapacity;
    //vq_SoilMoisture[i] = soilColumn[i].vs_SoilMoisture_m3;
    st->vq_SoilNO3[i] = (*st->soilColumn)[i].vs_SoilNO3;

    st->vc_NUptakeFromLayer[i] = st->cropModule ? st->cropModule->get_NUptakeFromLayer(i) : 0;
    if (i == nols - 1) 
      st->vq_PercolationRate[i] = st->soilColumn->vs_FluxAtLowerBoundary; //[mm]
    else
      st->vq_PercolationRate[i] = (*st->soilColumn)[i + 1].vs_SoilWaterFlux; //[mm]

    // Variable time step in case of high water fluxes to ensure stable numerics
    auto pri = st->vq_PercolationRate[i];
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

  nDeposition(st);
  nUptake(st);

  // Nitrate transport is called according to the set time step
  st->vq_LeachingAtBoundary = 0.0;
  for (int i_TimeStep = 0; i_TimeStep < (1.0 / minTimeStepFactor); i_TimeStep++) 
    nTransport(st, st->vs_LeachingDepth, minTimeStepFactor);

  for (int i = 0; i < nols; i++) {
    st->vq_SoilNO3[i] = st->vq_SoilNO3_aq[i] * (*st->soilColumn)[i].vs_SoilMoisture_m3;

    if (st->vq_SoilNO3[i] < 0.0) 
      st->vq_SoilNO3[i] = 0.0;

    (*st->soilColumn)[i].vs_SoilNO3 = st->vq_SoilNO3[i];
  } 

}


/**
 * @brief Calculation of N deposition
 * Transformation of annual N Deposition into a daily value,
 * that can be used in MONICAs calculations. Addition of this
 * transformed N deposition to ammonium pool of top soil layer.
 *
 * Kersebaum 1989
 */
void nDeposition(SoilTransport* st) {
  //Daily N deposition in [kg N ha-1 d-1]
  double dailyNDeposition = st->vs_NDeposition / 365.0;

  // Addition of N deposition to top layer [kg N m-3]
  st->vq_SoilNO3[0] += dailyNDeposition / (10000.0 * (*st->soilColumn)[0].vs_LayerThickness);
}

/**
 * @brief Calculation of crop N uptake
 * @param
 *
 * Kersebaum 1989
 */
void nUptake(SoilTransport* st) {
  const auto nols = st->soilColumn->size();
  double cropNUptake = 0.0;
  for (size_t i = 0; i < nols; i++) {
    const auto lti = (*st->soilColumn)[i].vs_LayerThickness;
    const auto smi = (*st->soilColumn)[i].vs_SoilMoisture_m3;

    // Lower boundary for N exploitation per layer
    if (st->vc_NUptakeFromLayer[i] > ((st->vq_SoilNO3[i] * lti) - st->pc_MinimumAvailableN)) {
      st->vc_NUptakeFromLayer[i] = ((st->vq_SoilNO3[i] * lti) - st->pc_MinimumAvailableN);
    } // Crop N uptake from layer i [kg N m-2]

    if (st->vc_NUptakeFromLayer[i] < 0) st->vc_NUptakeFromLayer[i] = 0;

    cropNUptake += st->vc_NUptakeFromLayer[i];

    // Subtracting crop N uptake
    st->vq_SoilNO3[i] -= st->vc_NUptakeFromLayer[i] / lti;

    // Calculation of solute NO3 concentration on the basis of the soil moisture
    // content before movement of current time step (kg m soil-3 --> kg m solute-3)
    st->vq_SoilNO3_aq[i] = st->vq_SoilNO3[i] / smi;
  }

  st->soilColumn->vq_CropNUptake = cropNUptake; // [kg m-2]
}

/**
 * @brief Calculation of N transport
 * @param vs_LeachingDepth
 *
 * Kersebaum 1989
 */
void nTransport(SoilTransport* st, double leachingDepth, double timeStepFactor) {
  double diffusionCoeffStandard = st->_params.pq_DiffusionCoefficientStandard; // [m2 d-1]; old D0
  double AD = st->_params.pq_AD; // Factor a in Kersebaum 1989 p.24 for Loess soils
  double dispersionLength = st->_params.pq_DispersionLength; // [m]
  double soilProfile = 0.0;
  size_t leachingDepthLayerIndex = 0;
  const auto nols = st->soilColumn->size();
  std::vector<double> soilMoistureGradient(nols, 0.0);

  for (size_t i = 0; i < nols; i++) {
    soilProfile += (*st->soilColumn)[i].vs_LayerThickness;
    if ((soilProfile - 0.001) < leachingDepth) 
      leachingDepthLayerIndex = i;
  }

  // Caluclation of convection for different cases of flux direction
  for (size_t i = 0; i < nols; i++) {
    const auto wf0 = (*st->soilColumn)[0].vs_SoilWaterFlux;
    const auto lt = (*st->soilColumn)[i].vs_LayerThickness;
    const auto NO3 = st->vq_SoilNO3_aq[i];
		
    if (i == 0) {
      const double pr = st->vq_PercolationRate[i] / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1]
      const double NO3_u = st->vq_SoilNO3_aq[i + 1];

      if (pr >= 0.0 && wf0 >= 0.0) {
        st->vq_Convection[i] = (NO3 * pr) / lt; //[kg m-3] * [m t-1] / [m] // old KONV = Konvektion Diss S. 23
      } else if (pr >= 0 && wf0 < 0) {
        st->vq_Convection[i] = (NO3 * pr) / lt;
      } else if (pr < 0 && wf0 < 0) {
        st->vq_Convection[i] = (NO3_u * pr) / lt;
      } else if (pr < 0 && wf0 >= 0) {
        st->vq_Convection[i] = (NO3_u * pr) / lt;
      }

    } else if (i < nols - 1) {
      // layer > 0 && < bottom
      const double pr_o = st->vq_PercolationRate[i - 1] / 1000.0 * timeStepFactor; //[mm t-1 --> m t-1] * [t t-1]
      const double pr = st->vq_PercolationRate[i] / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
      const double NO3_u = st->vq_SoilNO3_aq[i + 1];
	  
      if (pr >= 0.0 && pr_o >= 0.0) {
        const double NO3_o = st->vq_SoilNO3_aq[i - 1];
        st->vq_Convection[i] = ((NO3 * pr) - (NO3_o * pr_o)) / lt; // old KONV = Konvektion Diss S. 23
      } else if (pr >= 0 && pr_o < 0) {
        st->vq_Convection[i] = ((NO3 * pr) - (NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o < 0) {
        st->vq_Convection[i] = ((NO3_u * pr) - (NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o >= 0) {
        const double NO3_o = st->vq_SoilNO3_aq[i - 1];
        st->vq_Convection[i] = ((NO3_u * pr) - (NO3_o * pr_o)) / lt;
      }
    } else {
      // bottom layer
      const double pr_o = st->vq_PercolationRate[i - 1] / 1000.0 * timeStepFactor; // [m t-1] * [t t-1]
      const double pr = st->soilColumn->vs_FluxAtLowerBoundary / 1000.0 * timeStepFactor; // [m t-1] * [t t-1]

      if (pr >= 0.0 && pr_o >= 0.0) {
        const double NO3_o = st->vq_SoilNO3_aq[i - 1];
        st->vq_Convection[i] = ((NO3 * pr) - (NO3_o * pr_o)) / lt; // KONV = Konvektion Diss S. 23
      } else if (pr >= 0 && pr_o < 0) {
        st->vq_Convection[i] = ((NO3 * pr) - (NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o < 0) {
        st->vq_Convection[i] = (-(NO3 * pr_o)) / lt;
      } else if (pr < 0 && pr_o >= 0) {
        const double NO3_o = st->vq_SoilNO3_aq[i - 1];
        st->vq_Convection[i] = (-(NO3_o * pr_o)) / lt;
      }
    }// else
  } // for

  // Calculation of dispersion depending of pore water velocity
  for (size_t i = 0; i < nols; i++) {
    const auto pri = st->vq_PercolationRate[i] / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
    const auto pr0 = (*st->soilColumn)[0].vs_SoilWaterFlux / 1000.0 * timeStepFactor; // [mm t-1 --> m t-1] * [t t-1]
    const auto lti = (*st->soilColumn)[i].vs_LayerThickness;
    const auto NO3i = st->vq_SoilNO3_aq[i];
    const auto fci = (*st->soilColumn)[i]._sps.vs_FieldCapacity;
    const auto smi = (*st->soilColumn)[i].vs_SoilMoisture_m3;

    // Original: W(I) --> um Steingehalt korrigierte Feldkapazität
    /** @todo Claas: generelle Korrektur der Feldkapazität durch den Steingehalt */
    if (i == nols - 1) {
      st->vq_PoreWaterVelocity[i] = fabs((pri) / fci); // [m t-1]
      soilMoistureGradient[i] = smi; //[m3 m-3]
    }
    else {
      const auto fcip1 = (*st->soilColumn)[i + 1]._sps.vs_FieldCapacity;
      const auto smip1 = (*st->soilColumn)[i + 1].vs_SoilMoisture_m3;
      st->vq_PoreWaterVelocity[i] = fabs((pri) / ((fci + fcip1) * 0.5)); // [m t-1]
      soilMoistureGradient[i] = (smi + smip1) * 0.5; //[m3 m-3]
    }

    st->vq_DiffusionCoeff[i] = 
      diffusionCoeffStandard
      * (AD * exp(soilMoistureGradient[i] * 2.0 * 5.0)
        / soilMoistureGradient[i]) * timeStepFactor; //[m2 t-1] * [t t-1]

   // Dispersion coefficient, old DB
    if (i == 0) {
      st->vq_DispersionCoeff[i] = soilMoistureGradient[i] * (st->vq_DiffusionCoeff[i] // [m2 t-1]
        + dispersionLength * st->vq_PoreWaterVelocity[i]) // [m] * [m t-1]
        - (0.5 * lti * fabs(pri)) // [m] * [m t-1]
        + ((0.5 * st->vq_TimeStep * timeStepFactor * fabs((pri + pr0) / 2.0))  // [t] * [t t-1] * [m t-1]
          * st->vq_PoreWaterVelocity[i]); // * [m t-1]
          //-->[m2 t-1]
    }
    else {
      const double pr_o = st->vq_PercolationRate[i - 1] / 1000.0 * timeStepFactor; // [m t-1]

      st->vq_DispersionCoeff[i] = soilMoistureGradient[i] * (st->vq_DiffusionCoeff[i]
        + dispersionLength * st->vq_PoreWaterVelocity[i]) - (0.5 * lti * fabs(pri))
        + ((0.5 * st->vq_TimeStep * timeStepFactor * fabs((pri + pr_o) / 2.0)) * st->vq_PoreWaterVelocity[i]);
    }

    //old DISP = Gesamt-Dispersion (D in Diss S. 23)
    if (i == 0) {
      const double NO3_u = st->vq_SoilNO3_aq[i + 1];
      // vq_Dispersion = Dispersion upwards or downwards, depending on the position in the profile [kg m-3]
      st->vq_Dispersion[i] = -st->vq_DispersionCoeff[i] * (NO3i - NO3_u) / (lti * lti); // [m2] * [kg m-3] / [m2]
    }
    else if (i < nols - 1) {
      const double NO3_o = st->vq_SoilNO3_aq[i - 1];
      const double NO3_u = st->vq_SoilNO3_aq[i + 1];
      st->vq_Dispersion[i] = (st->vq_DispersionCoeff[i - 1] * (NO3_o - NO3i) / (lti * lti))
        - (st->vq_DispersionCoeff[i] * (NO3i - NO3_u) / (lti * lti));
    }
    else {
      const double NO3_o = st->vq_SoilNO3_aq[i - 1];
      st->vq_Dispersion[i] = st->vq_DispersionCoeff[i - 1] * (NO3_o - NO3i) / (lti * lti);
    }
  } // for
  
  if (st->vq_PercolationRate[leachingDepthLayerIndex] > 0.0) {
    //vq_LeachingDepthLayerIndex = gewählte Auswaschungstiefe
    const auto lt = (*st->soilColumn)[leachingDepthLayerIndex].vs_LayerThickness;
    const auto NO3 = st->vq_SoilNO3_aq[leachingDepthLayerIndex];

    if (leachingDepthLayerIndex < nols - 1) {
      const double pr_u = st->vq_PercolationRate[leachingDepthLayerIndex + 1] / 1000.0 * timeStepFactor;// [m t-1]
      const double NO3_u = st->vq_SoilNO3_aq[leachingDepthLayerIndex + 1]; // [kg m-3]
      //vq_LeachingAtBoundary: Summe für Auswaschung (Diff + Konv), old OUTSUM
      st->vq_LeachingAtBoundary += ((pr_u * NO3) / lt * 10000.0 * lt) + ((st->vq_DispersionCoeff[leachingDepthLayerIndex]
        * (NO3 - NO3_u)) / (lt * lt) * 10000.0 * lt); //[kg ha-1]
    } else {
      const double pr_u = st->soilColumn->vs_FluxAtLowerBoundary / 1000.0 * timeStepFactor; // [m t-1]
      st->vq_LeachingAtBoundary += pr_u * NO3 / lt * 10000.0 * lt; //[kg ha-1]
    }
  } else {
    const auto pr_u = st->vq_PercolationRate[leachingDepthLayerIndex] / 1000.0 * timeStepFactor;
    const auto lt = (*st->soilColumn)[leachingDepthLayerIndex].vs_LayerThickness;
    const auto NO3 = st->vq_SoilNO3_aq[leachingDepthLayerIndex];

    if (leachingDepthLayerIndex < nols - 1) {
      const double NO3_u = st->vq_SoilNO3_aq[leachingDepthLayerIndex + 1];
      st->vq_LeachingAtBoundary += ((pr_u * NO3_u) / (lt * 10000.0 * lt)) + st->vq_DispersionCoeff[leachingDepthLayerIndex]
        * (NO3 - NO3_u) / ((lt * lt) * 10000.0 * lt); //[kg ha-1]
    }
  }

  st->vq_LeachingAtBoundary = max(0.0, st->vq_LeachingAtBoundary);

  // Update of NO3 concentration
  // including transfomation back into [kg NO3-N m soil-3]
  for (size_t i = 0; i < nols; i++) {
    const auto smi = (*st->soilColumn)[i].vs_SoilMoisture_m3;
    st->vq_SoilNO3_aq[i] += (st->vq_Dispersion[i] - st->vq_Convection[i]) / smi;
  }
}

double getSoilNO3(const SoilTransport* st, int iLayer) {
  return st->vq_SoilNO3[iLayer];
}

double getVqDispersion(const SoilTransport* st, int iLayer) {
  return st->vq_Dispersion[iLayer];
}

double getVqConvection(const SoilTransport* st, int iLayer) {
  return st->vq_Convection[iLayer];
}

double getNLeaching(const SoilTransport* st) {
  return st->vq_LeachingAtBoundary;
}

void putCrop(SoilTransport* st, CropModule* cm) {
  st->cropModule = cm;
}

void removeCrop(SoilTransport* st) {
  st->cropModule = nullptr;
}

} // namespace soiltransport
} // namespace monica
