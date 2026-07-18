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

#pragma once

#include <vector>
#include <kj/memory.h>
#include "monica-parameters.h"

namespace monica {
  
// forward declarations
struct SoilColumn;
struct CropModule;

/**
* @brief Soil matter transport part of model
*
* @author Claas Nendel, Michael Berg
*
*/
struct SoilTransport {
  SoilColumn* soilColumn{nullptr};
  SoilTransportModuleParameters _params;
  //const size_t vs_NumberOfLayers;
  std::vector<double> vq_Convection;
  std::vector<double> vq_DiffusionCoeff;
  std::vector<double> vq_Dispersion;
  std::vector<double> vq_DispersionCoeff;
  //std::vector<double> vq_FieldCapacity;
  double vs_LeachingDepth{ 0.0 }; //!< [m]
  double vq_LeachingAtBoundary{ 0.0 };
  double vs_NDeposition{ 0.0 }; //!< [kg N ha-1 y-1]
  std::vector<double> vc_NUptakeFromLayer;	//! Pflanzenaufnahme aus der Tiefe Z; C1 N-Konzentration [kg N ha-1]
  std::vector<double> vq_PoreWaterVelocity;
  std::vector<double> vs_SoilMineralNContent;
  //std::vector<double> vq_SoilMoisture;
  std::vector<double> vq_SoilNO3;
  std::vector<double> vq_SoilNO3_aq;
  double vq_TimeStep{ 1.0 };
  std::vector<double> vq_TotalDispersion;
  std::vector<double> vq_PercolationRate; //!< Soil water flux from above [mm d-1]

  double pc_MinimumAvailableN{ 0.0 }; //! kg m-2

  CropModule* cropModule{nullptr};
};

kj::Own<SoilTransport> makeSoilTransport(SoilColumn& soilColumn,
                                         const SiteParameters& sps,
                                         const SoilTransportModuleParameters& params,
                                         double leachingDepth,
                                         double timeStep,
                                         double minimumAvailableN);
kj::Own<SoilTransport> makeSoilTransport(SoilColumn& soilColumn,
                                         mas::schema::model::monica::SoilTransportModuleState::Reader reader,
                                         CropModule* cropModule = nullptr);
void soilTransportDeserialize(SoilTransport* st, mas::schema::model::monica::SoilTransportModuleState::Reader reader);
void soilTransportSerialize(const SoilTransport* st,
                            mas::schema::model::monica::SoilTransportModuleState::Builder builder);
void soilTransportStep(SoilTransport* st);
void soilTransportNDeposition(SoilTransport* st);
void soilTransportNUptake(SoilTransport* st);
void soilTransportNTransport(SoilTransport* st, double leachingDepth, double timeStepFactor);
void soilTransportPutCrop(SoilTransport* st, CropModule* cm);
void soilTransportRemoveCrop(SoilTransport* st);
double soilTransportGetSoilNO3(const SoilTransport* st, int iLayer);
double soilTransportGetNLeaching(const SoilTransport* st);
double soilTransportGetVqDispersion(const SoilTransport* st, int iLayer);
double soilTransportGetVqConvection(const SoilTransport* st, int iLayer);

} // namespace monica
