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

#include "soilmoisture_simple.h"

#include <algorithm> //for min, max
#include <iostream>
#include <cmath>

#include "frost-component.h"
#include "snow-component.h"
#include "soilcolumn_simple.h"
#include "crop-module_simple.h"
#include "monica-model.h"
#include "tools/debug.h"
#include "tools/algorithms.h"
#include "soil/conversion.h"

using namespace std;
using namespace monica;
using namespace Tools;

namespace monica {
namespace soilmoisture {

void initializeFromParams(SoilMoisture* sm) {
  auto& soilColumn = sm->soilColumn;
  auto& smPs = sm->_params;
  auto& envPs = sm->envPs;
  auto& mm = sm->monica;
  debug() << "Constructor: SoilMoisture" << endl;

  sm->numberOfMoistureLayers = soilColumn.size() + 1;
  sm->numberOfSoilLayers = soilColumn.size();

  sm->vm_AvailableWater.assign(sm->numberOfMoistureLayers, 0.0);
  sm->pm_CapillaryRiseRate.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_CapillaryWater.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_CapillaryWater70.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_Evaporation.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_Evapotranspiration.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_FieldCapacity.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_GravitationalWater.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_HeatConductivity.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_Lambda.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_LayerThickness.assign(sm->numberOfMoistureLayers, 0.01);
  sm->vm_PermanentWiltingPoint.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_PercolationRate.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_ResidualEvapotranspiration.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_SoilMoisture.assign(sm->numberOfMoistureLayers, 0.20);
  sm->vm_SoilPoreVolume.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_Transpiration.assign(sm->numberOfMoistureLayers, 0.0);
  sm->vm_WaterFlux.assign(sm->numberOfMoistureLayers, 0.0);

  sm->vs_Latitude = sm->siteParameters.vs_Latitude;
  sm->vm_HydraulicConductivityRedux = smPs.pm_HydraulicConductivityRedux;
  sm->pt_TimeStep = envPs.p_timeStep;
  sm->vm_SurfaceRoughness = smPs.pm_SurfaceRoughness;
  sm->vm_GroundwaterDischarge = smPs.pm_GroundwaterDischarge;
  sm->pm_MaxPercolationRate = smPs.pm_MaxPercolationRate;
  sm->pm_LeachingDepth = envPs.p_LeachingDepth;

  //  cout << "pm_LeachingDepth:\t" << pm_LeachingDepth << endl;
  sm->pm_LayerThickness = mm.simulationParameters().p_LayerThickness;

  sm->pm_LeachingDepthLayer = int(std::floor(0.5 + (sm->pm_LeachingDepth / sm->pm_LayerThickness))) - 1;

  for (int i = 0; i < sm->numberOfMoistureLayers; i++) {
    sm->vm_SaturatedHydraulicConductivity.resize(sm->numberOfMoistureLayers, smPs.pm_SaturatedHydraulicConductivity);
    // original [8640 mm d-1]
  }

  sm->snowComponent = kj::heap<SnowComponent>(soilColumn, smPs);
  sm->frostComponent = kj::heap<FrostComponent>(soilColumn, smPs.pm_HydraulicConductivityRedux, envPs.p_timeStep);

  //  double vm_GroundwaterDepth = 0.0;
  //  for (int i_Layer = 0; i_Layer < vs_NumberOfLayers; i_Layer++) {
  //    vm_GroundwaterDepth += soilColumn[i_Layer].vs_LayerThickness;
  //    if (vm_GroundwaterDepth <= siteParameters.vs_GroundwaterDepth) {
  //      vm_GroundwaterTable = i_Layer;
  //    }
  //  }
  //  if (vm_GroundwaterDepth < siteParameters.vs_GroundwaterDepth) {
  //    vm_GroundwaterTable = vs_NumberOfLayers + 2;
  //  }
  //  soilColumn.vm_GroundwaterTable = vm_GroundwaterTable;
  //
  //  for (int i_Layer = vm_NumberOfLayers - 1; i_Layer >= vm_GroundwaterTable; i_Layer--) {
  //    soilColumn[i_Layer].set_Vs_SoilMoisture_m3(soilColumn[i_Layer].get_Saturation());
  //  }
}

/*!
 * @param vs_GroundwaterDepth Depth of ground water table
 * @param vw_Precipitation Precipitation amount
 * @param vw_MinAirTemperature Minimal air temperature
 * @param vw_MaxAirTemperature Maximal air temperature
 * @param vw_RelativeHumidity Relative Humidity
 * @param vw_MeanAirTemperature Mean air temperature
 * @param vw_WindSpeed Speed of wind
 * @param vw_WindSpeedHeight Height for the measurement of the wind speed
 * @param vw_GlobalRadiation Global radiation
 */
void step(SoilMoisture* sm,
                              double vs_GroundwaterDepth,
                              double vw_Precipitation,
                              double vw_MaxAirTemperature,
                              double vw_MinAirTemperature,
                              double vw_RelativeHumidity,
                              double vw_MeanAirTemperature,
                              double vw_WindSpeed,
                              double vw_WindSpeedHeight,
                              double vw_GlobalRadiation,
                              int vs_JulianDay,
                              double vw_ReferenceEvapotranspiration) {
  for (int i = 0; i < sm->numberOfSoilLayers; i++) {
    // initialization with moisture values stored in the layer
    sm->vm_SoilMoisture[i] = sm->soilColumn[i].vs_SoilMoisture_m3;
    sm->vm_WaterFlux[i] = 0.0;
    sm->vm_FieldCapacity[i] = sm->soilColumn[i]._sps.vs_FieldCapacity;
    sm->vm_SoilPoreVolume[i] = sm->soilColumn[i]._sps.vs_Saturation;
    sm->vm_PermanentWiltingPoint[i] = sm->soilColumn[i]._sps.vs_PermanentWiltingPoint;
    sm->vm_LayerThickness[i] = sm->soilColumn[i].vs_LayerThickness;
    sm->vm_Lambda[i] = sm->soilColumn[i].vs_Lambda();
  }

  sm->vm_SoilMoisture[sm->numberOfMoistureLayers - 1] = sm->soilColumn[sm->numberOfMoistureLayers - 2].vs_SoilMoisture_m3;
  sm->vm_WaterFlux[sm->numberOfMoistureLayers - 1] = 0.0;
  sm->vm_FieldCapacity[sm->numberOfMoistureLayers - 1] = sm->soilColumn[sm->numberOfMoistureLayers - 2]._sps.vs_FieldCapacity;
  sm->vm_SoilPoreVolume[sm->numberOfMoistureLayers - 1] = sm->soilColumn[sm->numberOfMoistureLayers - 2]._sps.vs_Saturation;
  sm->vm_LayerThickness[sm->numberOfMoistureLayers - 1] = sm->soilColumn[sm->numberOfMoistureLayers - 2].vs_LayerThickness;
  sm->vm_Lambda[sm->numberOfMoistureLayers - 1] = sm->soilColumn[sm->numberOfMoistureLayers - 2].vs_Lambda();

  sm->vm_SurfaceWaterStorage = sm->soilColumn.vs_SurfaceWaterStorage;

  bool vc_CropPlanted = false;
  double vc_CropHeight = 0.0;
  int vc_DevelopmentalStage = 0;

  if (sm->monica.cropGrowth()) {
    vc_CropPlanted = true;
    sm->vc_PercentageSoilCoverage = sm->monica.cropGrowth()->get_SoilCoverage();
    sm->vc_KcFactor = sm->monica.cropGrowth()->get_KcFactor();
    vc_CropHeight = sm->monica.cropGrowth()->get_CropHeight();
    vc_DevelopmentalStage = (int)sm->monica.cropGrowth()->get_DevelopmentalStage();
    if (vc_DevelopmentalStage > 0) {
      sm->vc_NetPrecipitation = sm->monica.cropGrowth()->get_NetPrecipitation();
    } else {
      sm->vc_NetPrecipitation = vw_Precipitation;
    }
  } else {
    vc_CropPlanted = false;
    sm->vc_KcFactor = sm->_params.pm_KcFactor;
    sm->vc_NetPrecipitation = vw_Precipitation;
    sm->vc_PercentageSoilCoverage = 0.0;
  }

  // Recalculates current depth of groundwater table
  sm->vm_GroundwaterTableLayer = sm->numberOfSoilLayers + 2;
  int i = int(sm->numberOfSoilLayers - 1);
  while (i >= 0 && int(sm->vm_SoilMoisture[i] * 10000) == int(sm->vm_SoilPoreVolume[i] * 10000)) sm->vm_GroundwaterTableLayer = i--;

  auto oscillGroundWaterLayer = size_t(vs_GroundwaterDepth / sm->soilColumn[0].vs_LayerThickness);
  if ((sm->vm_GroundwaterTableLayer > oscillGroundWaterLayer && sm->vm_GroundwaterTableLayer < sm->numberOfSoilLayers + 2)
      || sm->vm_GroundwaterTableLayer >= sm->numberOfSoilLayers + 2) {
    sm->vm_GroundwaterTableLayer = oscillGroundWaterLayer;
  }

  sm->soilColumn.vm_GroundwaterTableLayer = sm->vm_GroundwaterTableLayer;

  // calculates snow layer water storage and release
  sm->snowComponent->calcSnowLayer(vw_MeanAirTemperature, sm->vc_NetPrecipitation);
  double vm_WaterToInfiltrate = sm->snowComponent->getWaterToInfiltrate();

  // Calculates frost and thaw depth and switches lambda
  sm->frostComponent->calcSoilFrost(vw_MeanAirTemperature, sm->snowComponent->getVm_SnowDepth());

  // calculates infiltration of water from surface
  infiltration(sm, vm_WaterToInfiltrate);

  if (0.0 < vs_GroundwaterDepth && vs_GroundwaterDepth <= 10.0) {
    percolationWithGroundwater(sm, oscillGroundWaterLayer);
    groundwaterReplenishment(sm);
  } else {
    percolationWithoutGroundwater(sm);
    backwaterReplenishment(sm);
  }

  // Cache gross precipitation for Dual Kc fw logic (accessible in fm_Evapotranspiration)
  sm->vm_GrossPrecipitation = vw_Precipitation;

  evapotranspiration(sm,
                                   sm->vc_PercentageSoilCoverage,
                                   sm->vc_KcFactor,
                                   sm->siteParameters.vs_HeightNN,
                                   vw_MaxAirTemperature,
                                   vw_MinAirTemperature,
                                   vw_RelativeHumidity,
                                   vw_MeanAirTemperature,
                                   vw_WindSpeed,
                                   vw_WindSpeedHeight,
                                   vw_GlobalRadiation,
                                   vc_DevelopmentalStage,
                                   vs_JulianDay,
                                   sm->vs_Latitude,
                                   vw_ReferenceEvapotranspiration);

  capillaryRise(sm);

  for (int i_Layer = 0; i_Layer < sm->numberOfSoilLayers; i_Layer++) {
    sm->soilColumn[i_Layer].vs_SoilMoisture_m3 = sm->vm_SoilMoisture[i_Layer];
    sm->soilColumn[i_Layer].vs_SoilWaterFlux = sm->vm_WaterFlux[i_Layer];
    //commented out because old calc_vs_SoilMoisture_pF algorithm is calcualted every time vs_SoilMoisture_pF is accessed
    //    soilColumn[i_Layer].calc_vs_SoilMoisture_pF();
  }
  sm->soilColumn.vs_SurfaceWaterStorage = sm->vm_SurfaceWaterStorage;
  sm->soilColumn.vs_FluxAtLowerBoundary = sm->vm_FluxAtLowerBoundary;
}

/*!
 * @brief Calculation of infiltration
 *
 * Calculation of infiltration according to:
 * Wegehenkel, M (2002): Estimating of the impact of land use
 * changes using the conceptual hydrological model THESEUS - a case
 * study. Physics and Chemistry of the Earth 27, 631-640
 *
 * @param vm_WaterToInfiltrate
 */
void infiltration(SoilMoisture* sm, double vm_WaterToInfiltrate) {
  auto& vm_Infiltration = sm->vm_Infiltration;
  auto& vm_Interception = sm->vm_Interception;
  auto& vm_SurfaceRunOff = sm->vm_SurfaceRunOff;
  auto& vm_CapillaryRise = sm->vm_CapillaryRise;
  auto& vm_GroundwaterAdded = sm->vm_GroundwaterAdded;
  auto& vm_ActualTranspiration = sm->vm_ActualTranspiration;
  auto& vm_SurfaceWaterStorage = sm->vm_SurfaceWaterStorage;
  auto& vm_SoilMoistureDeficit = sm->vm_SoilMoistureDeficit;
  auto& vm_SoilPoreVolume = sm->vm_SoilPoreVolume;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& vm_SaturatedHydraulicConductivity = sm->vm_SaturatedHydraulicConductivity;
  auto& vm_HydraulicConductivityRedux = sm->vm_HydraulicConductivityRedux;
  auto& soilColumn = sm->soilColumn;
  auto& vm_LayerThickness = sm->vm_LayerThickness;
  auto& vm_WaterFlux = sm->vm_WaterFlux;
  auto& vm_FieldCapacity = sm->vm_FieldCapacity;
  auto& vm_GravitationalWater = sm->vm_GravitationalWater;
  auto& frostComponent = sm->frostComponent;
  auto& vm_PercolationRate = sm->vm_PercolationRate;
  auto& pm_MaxPercolationRate = sm->pm_MaxPercolationRate;
  auto& vm_GroundwaterTableLayer = sm->vm_GroundwaterTableLayer;
  auto& siteParameters = sm->siteParameters;
  auto& vc_PercentageSoilCoverage = sm->vc_PercentageSoilCoverage;
  auto& vm_SumSurfaceRunOff = sm->vm_SumSurfaceRunOff;
  auto& vm_Lambda = sm->vm_Lambda;
  auto& vm_SurfaceRoughness = sm->vm_SurfaceRoughness;

  // For receiving daily precipitation data all variables have to be reset
  vm_Infiltration = 0.0;
  vm_Interception = 0.0;
  vm_SurfaceRunOff = 0.0;
  vm_CapillaryRise = 0.0;
  vm_GroundwaterAdded = 0.0;
  vm_ActualTranspiration = 0.0;

  double vm_SurfaceWaterStorageOld = vm_SurfaceWaterStorage;

  // add the netto precipitation to the virtual surface water storage
  vm_SurfaceWaterStorage += vm_WaterToInfiltrate;

  // Calculating potential infiltration in [mm d-1]
  vm_SoilMoistureDeficit = (vm_SoilPoreVolume[0] - vm_SoilMoisture[0]) / vm_SoilPoreVolume[0];
  auto vm_ReducedHydraulicConductivity = vm_SaturatedHydraulicConductivity[0] * vm_HydraulicConductivityRedux;

  if (vm_ReducedHydraulicConductivity > 0.0) {
    auto vm_PotentialInfiltration = vm_ReducedHydraulicConductivity * 0.2 * vm_SoilMoistureDeficit
                                    * vm_SoilMoistureDeficit;

    // minimum of the availabe amount of water and the amount, soil is able to assimilate water
    // überprüft, dass das zu infiltrierende Wasser nicht größer ist
    // als das Volumnen, welches es aufnehmen kann
    vm_Infiltration = min(vm_SurfaceWaterStorage, vm_PotentialInfiltration);

    /** @todo <b>Claas:</b> Mathematischer Sinn ist zu überprüfen */
    vm_Infiltration = min(vm_Infiltration, ((vm_SoilPoreVolume[0] - vm_SoilMoisture[0]) * 1000.0
                                            * soilColumn[0].vs_LayerThickness));

    // Limitation of airfilled pore space added to prevent water contents
    // above pore space in layers below (Claas Nendel)
    vm_Infiltration = max(0.0, vm_Infiltration);
  } else {
    vm_Infiltration = 0.0;
  }

  // Updating yesterday's surface water storage
  if (vm_Infiltration > 0.0) {
    // Reduce the water storage with the infiltration amount
    vm_SurfaceWaterStorage -= vm_Infiltration;
  }

  // Calculating overflow due to water level exceeding surface roughness [mm]
  if (vm_SurfaceWaterStorage > (10.0 * vm_SurfaceRoughness / (siteParameters.vs_Slope + 0.001))) {
    // Calculating surface run-off driven by slope and altered by surface roughness and soil coverage
    // minimal slope at which water will be run off the surface
    auto vm_RunOffFactor = 0.02 + (vm_SurfaceRoughness / 4.0) + (vc_PercentageSoilCoverage / 15.0);
    if (siteParameters.vs_Slope < 0.0 || siteParameters.vs_Slope > 1.0) {
      // no valid slope
      cerr << "Slope value out ouf boundary" << endl;
    } else if (siteParameters.vs_Slope == 0.0) {
      // no slope so there will be no loss of water
      vm_SurfaceRunOff = 0.0;
    } else if (siteParameters.vs_Slope > vm_RunOffFactor) {
      // add all water from the surface to the run-off storage
      vm_SurfaceRunOff += vm_SurfaceWaterStorage;
    } else {
      // some water is running off because of a sloped surface
      /** @todo Claas: Ist die Formel korrekt? vm_RunOffFactor wird einmal reduziert? */
      vm_SurfaceRunOff += ((siteParameters.vs_Slope * vm_RunOffFactor) / (vm_RunOffFactor * vm_RunOffFactor)) *
        vm_SurfaceWaterStorage;
    }

    // Update surface water storage
    vm_SurfaceWaterStorage -= vm_SurfaceRunOff;
  }

  // Adding infiltrating water to top layer soil moisture
  vm_SoilMoisture[0] += (vm_Infiltration / 1000.0 / vm_LayerThickness[0]);

  // [m3 m-3] += ([mm] - [mm]) / [] / [m]; --> Conversion into volumetric water content [m3 m-3]
  vm_WaterFlux[0] = vm_Infiltration; // flux in layer 0

  // Calculating excess soil moisture (water content exceeding field capacity) for percolation
  if (vm_SoilMoisture[0] > vm_FieldCapacity[0]) {
    vm_GravitationalWater[0] = (vm_SoilMoisture[0] - vm_FieldCapacity[0]) * 1000.0 * vm_LayerThickness[0];
    auto vm_LambdaReduced = vm_Lambda[0] * frostComponent->getLambdaRedux(0);
    auto vm_PercolationFactor = 1 + vm_LambdaReduced * vm_GravitationalWater[0];
    vm_PercolationRate[0] = (vm_GravitationalWater[0] * vm_GravitationalWater[0] * vm_LambdaReduced)
                            / vm_PercolationFactor;
    if (vm_PercolationRate[0] > pm_MaxPercolationRate) vm_PercolationRate[0] = pm_MaxPercolationRate;
    vm_GravitationalWater[0] = vm_GravitationalWater[0] - vm_PercolationRate[0];
    vm_GravitationalWater[0] = max(0.0, vm_GravitationalWater[0]);

    // Adding the excess water remaining after the percolation event to soil moisture
    vm_SoilMoisture[0] = vm_FieldCapacity[0] + (vm_GravitationalWater[0] / 1000.0 / vm_LayerThickness[0]);

    // For groundwater table in first or second top layer no percolation occurs
    if (vm_GroundwaterTableLayer <= 1) vm_PercolationRate[0] = 0.0;

    // For groundwater table at soil surface no percolation occurs
    if (vm_GroundwaterTableLayer == 0) {
      vm_PercolationRate[0] = 0.0;

      // For soil water volume exceeding total pore volume, surface runoff occurs
      if (vm_SoilMoisture[0] > vm_SoilPoreVolume[0]) {
        vm_SurfaceRunOff += (vm_SoilMoisture[0] - vm_SoilPoreVolume[0]) * 1000.0 * vm_LayerThickness[0];
        vm_SoilMoisture[0] = vm_SoilPoreVolume[0];
        return;
      }
    }
  } else if (vm_SoilMoisture[0] <= vm_FieldCapacity[0]) {
    // For soil moisture contents below field capacity no excess water and no fluxes occur
    vm_PercolationRate[0] = 0.0;
    vm_GravitationalWater[0] = 0.0;
  }

  // Check water balance
  if (fabs((vm_SurfaceWaterStorageOld + vm_WaterToInfiltrate) - (vm_SurfaceRunOff + vm_Infiltration
                                                                 + vm_SurfaceWaterStorage)) > 0.01) {
    cerr << "water balance wrong!" << endl;
  }

  // water flux of next layer equals percolation rate of layer above
  vm_WaterFlux[1] = vm_PercolationRate[0];
  vm_SumSurfaceRunOff += vm_SurfaceRunOff;
}

/*!
 * @brief Calculates capillary rise (flux), if no groundwater is within the profil
 *
 * Capillary rise only above groundwater table and within layer with a water content
 * less than 70 \% of the current availible field capacity
 *
 * Kapillarer Aufstieg nach hiesiger Methode bedeutet:
 * Suchen der ersten Rechenschicht mit bf \< nfk70 oberhalb GW-Spiegel, Zuordnen der
 * Aufstiegsrate. Kapillarer Aufstieg erfolgt nur in diese Schicht, falls noch eine
 * weitere Schicht diese Bedingung erfuellt, ist der kapillare Aufstieg = 0. (!!),
 * diese Bedingung muss also nach dem Auffinden der ersten Bodenschicht \< nFK70
 * uebersprungen werden !!
 *
 * @param vm_GroundwaterTable First layer that contains groundwater
 *
 */
void capillaryRise(SoilMoisture* sm) {
  auto& cropModule = sm->cropModule;
  auto& vm_GroundwaterTableLayer = sm->vm_GroundwaterTableLayer;
  auto& vm_LayerThickness = sm->vm_LayerThickness;
  auto& numberOfSoilLayers = sm->numberOfSoilLayers;
  auto& vm_CapillaryWater = sm->vm_CapillaryWater;
  auto& vm_FieldCapacity = sm->vm_FieldCapacity;
  auto& vm_PermanentWiltingPoint = sm->vm_PermanentWiltingPoint;
  auto& vm_AvailableWater = sm->vm_AvailableWater;
  auto& vm_CapillaryWater70 = sm->vm_CapillaryWater70;
  auto& soilColumn = sm->soilColumn;
  auto& _params = sm->_params;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& vm_WaterFlux = sm->vm_WaterFlux;

  auto vc_RootingDepth = cropModule ? cropModule->get_RootingDepth() : 0;
  auto vm_GroundwaterDistance = max(size_t(1), vm_GroundwaterTableLayer - vc_RootingDepth); // []

  if (double(vm_GroundwaterDistance) * vm_LayerThickness[0] <= 2.70) { // [m]
    // Capillary rise rates in table defined only until 2.70 m

    for (int i_Layer = 0; i_Layer < numberOfSoilLayers; i_Layer++) {
      // Define capillary water and available water

      vm_CapillaryWater[i_Layer] = vm_FieldCapacity[i_Layer] - vm_PermanentWiltingPoint[i_Layer];
      vm_AvailableWater[i_Layer] = vm_SoilMoisture[i_Layer] - vm_PermanentWiltingPoint[i_Layer];

      if (vm_AvailableWater[i_Layer] < 0.0) vm_AvailableWater[i_Layer] = 0.0;

      vm_CapillaryWater70[i_Layer] = 0.7 * vm_CapillaryWater[i_Layer];
    }

    // Find first layer above groundwater with 70% available water
    auto vm_StartLayer = min(vm_GroundwaterTableLayer, (numberOfSoilLayers - 1));
    for (int i = int(vm_StartLayer); i >= 0; i--) {
      std::string vs_SoilTexture = soilColumn[i].vs_SoilTexture();
      assert(!vs_SoilTexture.empty());
      double vm_CapillaryRiseRate = min(0.01, _params.getCapillaryRiseRate(vs_SoilTexture, vm_GroundwaterDistance));
      // [m d-1]
      if (vm_AvailableWater[i] < vm_CapillaryWater70[i]) {
        auto vm_WaterAddedFromCapillaryRise = vm_CapillaryRiseRate; // [m d-1]
        vm_SoilMoisture[i] += vm_WaterAddedFromCapillaryRise / vm_LayerThickness[i]; // [m3 per 10cm layer d-1]
        for (int j_Layer = int(vm_StartLayer); j_Layer >= i; j_Layer
             --)
          vm_WaterFlux[j_Layer] -= vm_WaterAddedFromCapillaryRise * 1000.0; // [mm d-1]
        break;
      }
    }
  }
}

double getSoilMoisture(const SoilMoisture* sm, int layer) {
  return sm->soilColumn[layer].vs_SoilMoisture_m3;
}

double getCapillaryRise(const SoilMoisture* sm, int layer) {
  return sm->vm_CapillaryWater.at(layer);
}

double getPercolationRate(const SoilMoisture* sm, int layer) {
  return sm->vm_PercolationRate.at(layer);
}

/**
 * @brief Calculation of percolation with groundwater influence
  */
void percolationWithGroundwater(SoilMoisture* sm, size_t oscillGroundwaterLayer) {
  auto& vm_GroundwaterAdded = sm->vm_GroundwaterAdded;
  auto& numberOfMoistureLayers = sm->numberOfMoistureLayers;
  auto& vm_GroundwaterTableLayer = sm->vm_GroundwaterTableLayer;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& vm_PercolationRate = sm->vm_PercolationRate;
  auto& vm_LayerThickness = sm->vm_LayerThickness;
  auto& vm_WaterFlux = sm->vm_WaterFlux;
  auto& vm_FieldCapacity = sm->vm_FieldCapacity;
  auto& vm_GravitationalWater = sm->vm_GravitationalWater;
  auto& vm_Lambda = sm->vm_Lambda;
  auto& frostComponent = sm->frostComponent;
  auto& vm_SoilPoreVolume = sm->vm_SoilPoreVolume;
  auto& vm_GroundwaterDischarge = sm->vm_GroundwaterDischarge;
  auto& vm_FluxAtLowerBoundary = sm->vm_FluxAtLowerBoundary;
  auto& pm_LeachingDepthLayer = sm->pm_LeachingDepthLayer;

  vm_GroundwaterAdded = 0.0;

  for (size_t i = 0; i < numberOfMoistureLayers - 1; i++) {
    auto indexOfLayerBelow = i + 1;
    if (vm_GroundwaterTableLayer > indexOfLayerBelow) {
      // well above groundwater table
      vm_SoilMoisture[indexOfLayerBelow] += vm_PercolationRate[i] / 1000.0 / vm_LayerThickness[i];
      vm_WaterFlux[indexOfLayerBelow] = vm_PercolationRate[i];

      if (vm_SoilMoisture[indexOfLayerBelow] > vm_FieldCapacity[indexOfLayerBelow]) {
        // Soil moisture exceeding field capacity
        vm_GravitationalWater[indexOfLayerBelow] =
          (vm_SoilMoisture[indexOfLayerBelow] - vm_FieldCapacity[indexOfLayerBelow]) * 1000.0 *
          vm_LayerThickness[i + 1];

        double vm_LambdaReduced = vm_Lambda[indexOfLayerBelow] * frostComponent->getLambdaRedux(indexOfLayerBelow);
        double vm_PercolationFactor = 1 + vm_LambdaReduced * vm_GravitationalWater[indexOfLayerBelow];
        vm_PercolationRate[indexOfLayerBelow] = (
          (vm_GravitationalWater[indexOfLayerBelow] * vm_GravitationalWater[indexOfLayerBelow]
           * vm_LambdaReduced) / vm_PercolationFactor);

        vm_GravitationalWater[indexOfLayerBelow] =
          vm_GravitationalWater[indexOfLayerBelow] - vm_PercolationRate[indexOfLayerBelow];

        if (vm_GravitationalWater[indexOfLayerBelow] < 0) vm_GravitationalWater[indexOfLayerBelow] = 0.0;

        vm_SoilMoisture[indexOfLayerBelow] = vm_FieldCapacity[indexOfLayerBelow] +
                                             (vm_GravitationalWater[indexOfLayerBelow] / 1000.0 /
                                              vm_LayerThickness[indexOfLayerBelow]);

        if (vm_SoilMoisture[indexOfLayerBelow] > vm_SoilPoreVolume[indexOfLayerBelow]) {
          // Soil moisture exceeding soil pore volume
          vm_GravitationalWater[indexOfLayerBelow] =
            (vm_SoilMoisture[indexOfLayerBelow] - vm_SoilPoreVolume[indexOfLayerBelow]) * 1000.0
            * vm_LayerThickness[indexOfLayerBelow];
          vm_SoilMoisture[indexOfLayerBelow] = vm_SoilPoreVolume[indexOfLayerBelow];
          vm_PercolationRate[indexOfLayerBelow] += vm_GravitationalWater[indexOfLayerBelow];
        }
      } else {
        // Soil moisture below field capacity
        vm_PercolationRate[indexOfLayerBelow] = 0.0;
        vm_GravitationalWater[indexOfLayerBelow] = 0.0;
      }
    }
    // when the layer directly above groundwater table is reached
    else if (vm_GroundwaterTableLayer == indexOfLayerBelow) {
      // groundwater table shall not undermatch the oscillating groundwater depth
      // which is generated within the outer framework
      // groundwater table is due to daily definition in this case equal to oscill groundwater level
      // -> below layer will discharge with GroundwaterDischarge and will receive normal percolation rate
      // -> thus flux below will consist of percolation rate
      if (vm_GroundwaterTableLayer >= oscillGroundwaterLayer) {
        vm_SoilMoisture[indexOfLayerBelow] += vm_PercolationRate[i] / 1000.0 / vm_LayerThickness[i];
        vm_PercolationRate[indexOfLayerBelow] = vm_GroundwaterDischarge;
        vm_WaterFlux[indexOfLayerBelow] = vm_PercolationRate[i];
      } else {
        // oscillating groundwater depth is actually lower than the filled profile, so profile will be drained
        // -> the below profile will lose full GroundwaterDischarge, but will receive the percolation rate
        // -> the profile below will have a GroundwaterDischarge percolation rate and flux
        vm_SoilMoisture[indexOfLayerBelow] +=
          (vm_PercolationRate[i] - vm_GroundwaterDischarge) / 1000.0 / vm_LayerThickness[i];
        vm_PercolationRate[indexOfLayerBelow] = vm_GroundwaterDischarge;
        vm_WaterFlux[indexOfLayerBelow] = vm_GroundwaterDischarge;
      }

      if (vm_SoilMoisture[indexOfLayerBelow] >= vm_SoilPoreVolume[indexOfLayerBelow]) {
        //vm_GroundwaterTable--; // Rising groundwater table if vm_SoilMoisture > soil pore volume

        // vm_GroundwaterAdded is the volume of water added to the groundwater body.
        // It does not correspond to groundwater replenishment in the technical sense !!!!!
        vm_GroundwaterAdded = (vm_SoilMoisture[indexOfLayerBelow] - vm_SoilPoreVolume[indexOfLayerBelow]) * 1000.0 *
                              vm_LayerThickness[indexOfLayerBelow];

        vm_SoilMoisture[indexOfLayerBelow] = vm_SoilPoreVolume[indexOfLayerBelow];

        if (vm_GroundwaterAdded <= 0.0) vm_GroundwaterAdded = 0.0;
      }
    }
    // when the groundwater table is reached
    else if (vm_GroundwaterTableLayer < indexOfLayerBelow) {
      vm_SoilMoisture[indexOfLayerBelow] = vm_SoilPoreVolume[indexOfLayerBelow];

      if (vm_GroundwaterTableLayer >= oscillGroundwaterLayer) {
        vm_PercolationRate[indexOfLayerBelow] = vm_PercolationRate[i];
        vm_WaterFlux[i] = vm_PercolationRate[indexOfLayerBelow];
      } else {
        vm_PercolationRate[indexOfLayerBelow] = vm_GroundwaterDischarge;
        vm_WaterFlux[i] = vm_GroundwaterDischarge;
      }
    }
  }

  vm_FluxAtLowerBoundary = vm_WaterFlux[pm_LeachingDepthLayer];
}

/**
 * @brief Calculation of groundwater replenishment
 *
 */
void groundwaterReplenishment(SoilMoisture* sm) {
  auto& vm_GroundwaterTableLayer = sm->vm_GroundwaterTableLayer;
  auto& numberOfMoistureLayers = sm->numberOfMoistureLayers;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& vm_GroundwaterAdded = sm->vm_GroundwaterAdded;
  auto& vm_LayerThickness = sm->vm_LayerThickness;
  auto& vm_PercolationRate = sm->vm_PercolationRate;
  auto& vm_GroundwaterDischarge = sm->vm_GroundwaterDischarge;
  auto& vm_WaterFlux = sm->vm_WaterFlux;
  auto& vm_SoilPoreVolume = sm->vm_SoilPoreVolume;
  auto& vm_SurfaceWaterStorage = sm->vm_SurfaceWaterStorage;
  auto& pm_LeachingDepthLayer = sm->pm_LeachingDepthLayer;
  auto& vm_FluxAtLowerBoundary = sm->vm_FluxAtLowerBoundary;

  // Auffuellschleife von GW-Oberflaeche in Richtung Oberflaeche
  auto vm_StartLayer = vm_GroundwaterTableLayer;

  if (vm_StartLayer > numberOfMoistureLayers - 2) {
    vm_StartLayer = numberOfMoistureLayers - 2;
  }

  for (int i = int(vm_StartLayer); i >= 0; i--) {
    auto indexOfLayerBelow = i + 1;
    vm_SoilMoisture[i] += vm_GroundwaterAdded / 1000.0 / vm_LayerThickness[indexOfLayerBelow];

    if (i == vm_StartLayer) {
      vm_PercolationRate[i] = vm_GroundwaterDischarge;
    } else {
      vm_PercolationRate[i] -= vm_GroundwaterAdded; // flux below by groundwater
      vm_WaterFlux[indexOfLayerBelow] = vm_PercolationRate[i]; // flux below by groundwater
    }

    if (vm_SoilMoisture[i] > vm_SoilPoreVolume[i]) {
      vm_GroundwaterAdded = (vm_SoilMoisture[i] - vm_SoilPoreVolume[i]) * 1000.0 * vm_LayerThickness[indexOfLayerBelow];
      vm_SoilMoisture[i] = vm_SoilPoreVolume[i];
      vm_GroundwaterTableLayer--; // Groundwater table rises

      if (i == 0 && vm_GroundwaterTableLayer == 0) {
        // if groundwater reaches surface
        vm_SurfaceWaterStorage += vm_GroundwaterAdded;
        vm_GroundwaterAdded = 0.0;
      }
    } else {
      vm_GroundwaterAdded = 0.0;
    }
  }

  if (pm_LeachingDepthLayer > vm_GroundwaterTableLayer - 1) {
    if (vm_GroundwaterTableLayer - 1 < 0) {
      vm_FluxAtLowerBoundary = 0.0;
    } else {
      vm_FluxAtLowerBoundary = vm_WaterFlux[vm_GroundwaterTableLayer - 1];
    }
  } else {
    vm_FluxAtLowerBoundary = vm_WaterFlux[pm_LeachingDepthLayer];
  }
}

/**
 * @brief Calculation of percolation without groundwater influence
 */
void percolationWithoutGroundwater(SoilMoisture* sm) {
  auto& numberOfMoistureLayers = sm->numberOfMoistureLayers;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& vm_PercolationRate = sm->vm_PercolationRate;
  auto& vm_LayerThickness = sm->vm_LayerThickness;
  auto& vm_FieldCapacity = sm->vm_FieldCapacity;
  auto& vm_GravitationalWater = sm->vm_GravitationalWater;
  auto& vm_Lambda = sm->vm_Lambda;
  auto& frostComponent = sm->frostComponent;
  auto& pm_MaxPercolationRate = sm->pm_MaxPercolationRate;
  auto& vm_WaterFlux = sm->vm_WaterFlux;
  auto& vm_GroundwaterAdded = sm->vm_GroundwaterAdded;
  auto& pm_LeachingDepthLayer = sm->pm_LeachingDepthLayer;
  auto& vm_FluxAtLowerBoundary = sm->vm_FluxAtLowerBoundary;

  for (size_t i = 0; i < numberOfMoistureLayers - 1; i++) {
    auto indexOfLayerBelow = i + 1;
    vm_SoilMoisture[indexOfLayerBelow] += vm_PercolationRate[i] / 1000.0 / vm_LayerThickness[i];

    if (vm_SoilMoisture[indexOfLayerBelow] > vm_FieldCapacity[indexOfLayerBelow]) {
      // too much water for this layer so some water is released to layers below
      vm_GravitationalWater[indexOfLayerBelow] =
        (vm_SoilMoisture[indexOfLayerBelow] - vm_FieldCapacity[indexOfLayerBelow]) * 1000.0 * vm_LayerThickness[0];
      auto vm_LambdaReduced = vm_Lambda[indexOfLayerBelow] * frostComponent->getLambdaRedux(indexOfLayerBelow);
      auto vm_PercolationFactor = 1.0 + (vm_LambdaReduced * vm_GravitationalWater[indexOfLayerBelow]);
      vm_PercolationRate[indexOfLayerBelow] =
      (vm_GravitationalWater[indexOfLayerBelow] * vm_GravitationalWater[indexOfLayerBelow]
       * vm_LambdaReduced) / vm_PercolationFactor;

      if (vm_PercolationRate[indexOfLayerBelow] > pm_MaxPercolationRate) {
        vm_PercolationRate[indexOfLayerBelow] = pm_MaxPercolationRate;
      }

      vm_GravitationalWater[indexOfLayerBelow] =
        vm_GravitationalWater[indexOfLayerBelow] - vm_PercolationRate[indexOfLayerBelow];

      if (vm_GravitationalWater[indexOfLayerBelow] < 0.0) vm_GravitationalWater[indexOfLayerBelow] = 0.0;

      vm_SoilMoisture[indexOfLayerBelow] = vm_FieldCapacity[indexOfLayerBelow] +
                                           (vm_GravitationalWater[indexOfLayerBelow] / 1000.0 /
                                            vm_LayerThickness[indexOfLayerBelow]);
    } else {
      // no water will be released in other layers
      vm_PercolationRate[indexOfLayerBelow] = 0.0;
      vm_GravitationalWater[indexOfLayerBelow] = 0.0;
    }

    vm_WaterFlux[indexOfLayerBelow] = vm_PercolationRate[i];
    vm_GroundwaterAdded = vm_PercolationRate[indexOfLayerBelow];
  }

  if (pm_LeachingDepthLayer > 0 && pm_LeachingDepthLayer < numberOfMoistureLayers - 1) {
    vm_FluxAtLowerBoundary = vm_WaterFlux[pm_LeachingDepthLayer];
  } else {
    vm_FluxAtLowerBoundary = vm_WaterFlux[numberOfMoistureLayers - 2];
  }
}

/**
 * @brief Calculation of backwater replenishment
 *
 */
void backwaterReplenishment(SoilMoisture* sm) {
  auto& numberOfMoistureLayers = sm->numberOfMoistureLayers;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& vm_SoilPoreVolume = sm->vm_SoilPoreVolume;
  auto& vm_LayerThickness = sm->vm_LayerThickness;
  auto& vm_WaterFlux = sm->vm_WaterFlux;
  auto& vm_SurfaceWaterStorage = sm->vm_SurfaceWaterStorage;

  auto vm_StartLayer = numberOfMoistureLayers - 1;
  auto vm_BackwaterTable = numberOfMoistureLayers - 1;
  double vm_BackwaterAdded = 0.0;

  // find first layer from top where the water content exceeds pore volume
  for (size_t i = 0; i < numberOfMoistureLayers - 1; i++) {
    if (vm_SoilMoisture[i] > vm_SoilPoreVolume[i]) {
      vm_StartLayer = i;
      vm_BackwaterTable = i;
    }
  }

  // if there is no such thing nothing will happen
  if (vm_BackwaterTable == 0) return;

  // Backwater replenishment upwards
  for (int i = int(vm_StartLayer); i >= 0; i--) {
    //!TODO check loop and whether it really should be i_Layer + 1 or the loop should start one layer higher ????!!!!
    vm_SoilMoisture[i] += vm_BackwaterAdded / 1000.0 / vm_LayerThickness[i]; // + 1];
    if (i > 0) vm_WaterFlux[i - 1] -= vm_BackwaterAdded;

    if (vm_SoilMoisture[i] > vm_SoilPoreVolume[i]) {
      //!TODO check also i_Layer + 1 here for same reason as above
      vm_BackwaterAdded = (vm_SoilMoisture[i] - vm_SoilPoreVolume[i]) * 1000.0 * vm_LayerThickness[i]; // + 1];
      vm_SoilMoisture[i] = vm_SoilPoreVolume[i];
      vm_BackwaterTable--; // Backwater table rises

      if (i == 0 && vm_BackwaterTable == 0) {
        // if backwater reaches surface
        vm_SurfaceWaterStorage += vm_BackwaterAdded;
        vm_BackwaterAdded = 0.0;
      }
    } else {
      vm_BackwaterAdded = 0.0;
    }
  } // for
}

// FAO-56 Dual Kc pathway — precompute potential soil evaporation (E_pot)
double dualKcPrecomputation(SoilMoisture* sm, double windSpeed, double tmin, double tmax) {
  auto& vm_ReferenceEvapotranspiration = sm->vm_ReferenceEvapotranspiration;
  auto& cropModule = sm->cropModule;
  auto& vm_FieldCapacity = sm->vm_FieldCapacity;
  auto& vm_PermanentWiltingPoint = sm->vm_PermanentWiltingPoint;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& soilColumn = sm->soilColumn;
  auto& vm_GrossPrecipitation = sm->vm_GrossPrecipitation;
  auto& monica = sm->monica;
  auto& vm_LastWettingWasRain = sm->vm_LastWettingWasRain;
  auto& vm_irrigFwEvent = sm->vm_irrigFwEvent;
  auto& vm_irrigIsDripEvent = sm->vm_irrigIsDripEvent;
  auto& vc_PercentageSoilCoverage = sm->vc_PercentageSoilCoverage;
  auto& vm_Ke = sm->vm_Ke;

  double E_pot_dualKc = 0.0; // [mm d-1] replaces (1-beta)*PET per layer

  // ET0 is already in vm_ReferenceEvapotranspiration [mm d-1]
  const double ET0 = vm_ReferenceEvapotranspiration;

  // --- Kcb: basal crop coefficient (interpolated in crop module) ---
  const double Kcb = cropModule->get_KcbFactor();

  // --- Calculate Depletion first (FAO-56 §8.3) to inform memory logic ---
  // Uses only the top soil layer (layer 0, typically 0-10 cm)
  const double FC0 = vm_FieldCapacity[0]; // [m3 m-3]
  const double WP0 = vm_PermanentWiltingPoint[0]; // [m3 m-3]
  const double SWC0 = vm_SoilMoisture[0]; // [m3 m-3]
  const double Ze = 0.1; // evaporation depth [m], FAO-56 typical top layer
  // TEW: total evaporable water [mm] from top layer
  const double TEW = 1000.0 * (FC0 - 0.5 * WP0) * Ze;

  // A. Dynamic REW (FAO-56 Table 19 Mapping via Pedology)
  double REW = 0.0;
  std::string ka5Texture = soilColumn[0].vs_SoilTexture();

  if (ka5Texture == "Ss") REW = 2.5;
  else if (ka5Texture == "Su2" || ka5Texture == "Sl2") REW = 3.5;
  else if (ka5Texture == "Su3" || ka5Texture == "Sl3") REW = 4.5;
  else if (ka5Texture == "Su4" || ka5Texture == "Sl4" || ka5Texture == "St2") REW = 5.5;
  else if (ka5Texture == "St3" || ka5Texture == "Ls2" || ka5Texture == "Us2") REW = 8.0;
  else if (ka5Texture == "Uu" || ka5Texture == "Us3" || ka5Texture == "Us4" ||
           ka5Texture == "Ul2" || ka5Texture == "Ul3" || ka5Texture == "Ul4")
    REW = 8.5;
  else if (ka5Texture == "Ls3" || ka5Texture == "Ls4" ||
           ka5Texture == "Lu2" || ka5Texture == "Lu3" || ka5Texture == "Lu4")
    REW = 9.0;
  else if (ka5Texture == "Ut2" || ka5Texture == "Ut3") REW = 9.5;
  else if (ka5Texture == "Ut4" || ka5Texture == "Lt2" || ka5Texture == "Lt3" || ka5Texture == "Lts") REW = 10.5;
  else if (ka5Texture == "Ts2" || ka5Texture == "Ts3" || ka5Texture == "Ts4" ||
           ka5Texture == "Tu2" || ka5Texture == "Tu3" || ka5Texture == "Tu4" || ka5Texture == "Tl")
    REW = 11.5;
  else if (ka5Texture == "Tt") REW = 12.0;

  // Fallback if KA5 lookup is unavailable or fails:
  if (REW == 0.0) {
    if (FC0 < 0.18) {
      // Coarse soils (Sand, Sandy Loams): REW ranges from 2.0 to 7.0 mm
      REW = 2.5 + 25.0 * (FC0 - 0.05);
      REW = std::max(2.0, std::min(REW, 7.0));
    } else if (FC0 >= 0.28) {
      // Fine soils (Clays): REW ranges from 8.0 to 12.0 mm
      REW = 8.0 + 20.0 * (FC0 - 0.28);
      REW = std::max(8.0, std::min(REW, 12.0));
    } else {
      REW = 8.0;
    }
  }
  REW = std::min(REW, TEW); // Hard boundary safety clamp

  // Current depletion De [mm] = what the top layer is missing compared to FC
  const double De = 1000.0 * std::max(0.0, FC0 - SWC0) * Ze;

  // --- Memory state update for wetting events ---
  const double precip = vm_GrossPrecipitation;
  const double irrigApplied = monica.dailySumIrrigationWater();
  if (precip > 0.0) {
    vm_LastWettingWasRain = true;
  } else if (irrigApplied > 0.0) {
    vm_LastWettingWasRain = false;
  }

  // --- fw: fraction of wetted soil surface (event-level, FAO-56 §8.3) ---
  // Priority: rain today > persistent rain drying (Stage 1) > irrigation event fw.
  // LIMITATION: Auto-irrigation uses defaults (fw=1.0, isDrip=false).
  double fw_today;
  if (precip > 0.0) {
    fw_today = 1.0; // rain wets the full surface
  } else if (De <= REW && vm_LastWettingWasRain) {
    fw_today = 1.0; // still in Stage 1 drying from recent rain
  } else {
    fw_today = vm_irrigFwEvent; // carry/use the last Irrigation workstep fw
  }
  fw_today = std::max(0.0, std::min(fw_today, 1.0));

  // Drip irrigation shading adjustment (FAO-56 §8.3):
  //   fw_adj = fw * (1 - (2/3) * fc), where fc = fractional ground cover
  // Applied if the current active wetting event was drip AND no rain today.
  double fw_adj = fw_today;
  if (vm_irrigIsDripEvent && !vm_LastWettingWasRain && precip == 0.0) {
    fw_adj = fw_today * (1.0 - (2.0 / 3.0) * vc_PercentageSoilCoverage);
    fw_adj = std::max(0.0, std::min(fw_adj, 1.0));
  }

  // --- Kr: evaporation reduction coefficient (FAO-56 §8.3) ---
  double Kr = 1.0;
  if (De > REW) {
    if ((TEW - REW) > 0.0) {
      Kr = (TEW - De) / (TEW - REW);
    } else {
      Kr = 0.0;
    }
  }
  Kr = std::max(0.0, std::min(Kr, 1.0));

  // B. Rigorous Climatic Kc_max (FAO-56 Equation 72)
  // Uses native simulated crop height via get_CropHeight() — no hardcoded per-stage values.
  double u2 = (windSpeed > 0.0) ? windSpeed : 2.0;
  double eo_Tmax = 0.6108 * std::exp((17.27 * tmax) / (tmax + 237.3));
  double eo_Tmin = 0.6108 * std::exp((17.27 * tmin) / (tmin + 237.3));
  double RHmin = std::max(5.0, std::min(100.0, (eo_Tmin / eo_Tmax) * 100.0));
  const double baseline = 1.2; // FAO-56 §6 default for most crops
  double h = std::max(0.01, cropModule->get_CropHeight()); // native simulated height [m]
  double Kc_max = baseline + (0.04 * (u2 - 2.0) - 0.004 * (RHmin - 45.0)) * std::pow(h / 3.0, 0.3);
  Kc_max = std::max(Kc_max, Kcb + 0.05);

  // C. few: fraction of exposed and wetted soil (FAO-56 Eq. 74)
  double fc = std::max(0.0, std::min(vc_PercentageSoilCoverage, 0.99));
  double few = std::min(1.0 - fc, fw_adj);
  few = std::max(0.001, few); // guard against zero denominator

  // --- Ke: soil evaporation coefficient (FAO-56 eq. 71) ---
  double Ke = Kr * (Kc_max - Kcb);
  Ke = std::min(Ke, few * Kc_max);
  Ke = std::max(0.0, Ke);

  // Potential soil evaporation [mm d-1]
  E_pot_dualKc = ET0 * Ke;
  // Respect the hard cap from HERMES (6.5 mm/day applies to total, cap E too)
  E_pot_dualKc = std::min(E_pot_dualKc, 6.5);

  vm_Ke = Ke;

  return E_pot_dualKc;
}


/**
 * @brief Calculation of Evapotranspiration
 * Calculation of transpiration and evaporation.
 *
 * @param vc_PercentageSoilCoverage
 * @param vc_KcFactor Needed for calculation of the Evapo-transpiration
 * @param vs_HeightNN
 * @param vw_MaxAirTemperature Maximal air temperature
 * @param vw_MinAirTemperature Minimal air temperature
 * @param vw_RelativeHumidity Relative Humidity
 * @param vw_MeanAirTemperature Mean air temperature
 * @param vw_WindSpeed Speed of wind
 * @param vw_WindSpeedHeight Height for the measurement of the wind speed
 * @param vw_GlobalRadiation Global radiaton
 * @param vc_DevelopmentalStage
 */
void evapotranspiration(SoilMoisture* sm,
                                              double vc_PercentageSoilCoverage,
                                              double vc_KcFactor,
                                              double vs_HeightNN,
                                              double vw_MaxAirTemperature,
                                              double vw_MinAirTemperature,
                                              double vw_RelativeHumidity,
                                              double vw_MeanAirTemperature,
                                              double vw_WindSpeed,
                                              double vw_WindSpeedHeight,
                                              double vw_GlobalRadiation,
                                              int vc_DevelopmentalStage,
                                              int vs_JulianDay,
                                              double vs_Latitude,
                                              double vw_ReferenceEvapotranspiration) {
  auto& vm_EvaporatedFromSurface = sm->vm_EvaporatedFromSurface;
  auto& snowComponent = sm->snowComponent;
  auto& _params = sm->_params;
  auto& vm_XSACriticalSoilMoisture = sm->vm_XSACriticalSoilMoisture;
  auto& monica = sm->monica;
  auto& cropModule = sm->cropModule;
  auto& vm_ReferenceEvapotranspiration = sm->vm_ReferenceEvapotranspiration;
  auto& vm_SurfaceWaterStorage = sm->vm_SurfaceWaterStorage;
  auto& vm_Ke = sm->vm_Ke;
  auto& numberOfSoilLayers = sm->numberOfSoilLayers;
  auto& vm_LayerThickness = sm->vm_LayerThickness;
  auto& vm_SoilMoisture = sm->vm_SoilMoisture;
  auto& vm_Evaporation = sm->vm_Evaporation;
  auto& vm_Transpiration = sm->vm_Transpiration;
  auto& vm_Evapotranspiration = sm->vm_Evapotranspiration;
  auto& vm_ActualTranspiration = sm->vm_ActualTranspiration;
  auto& vm_ActualEvaporation = sm->vm_ActualEvaporation;
  auto& vm_ActualEvapotranspiration = sm->vm_ActualEvapotranspiration;

  double vm_EReducer_1 = 0.0;
  double vm_EReducer_2 = 0.0;
  double vm_EReducer_3 = 0.0;
  double pm_EvaporationZeta = 0.0;
  double pm_MaximumEvaporationImpactDepth = 0.0;
  // Das ist die Tiefe, bis zu der maximal die Evaporation vordringen kann
  double vm_EReducer = 0.0;
  double vm_PotentialEvapotranspiration = 0.0;
  double vc_EvaporatedFromIntercept = 0.0;
  vm_EvaporatedFromSurface = 0.0;
  bool vm_EvaporationFromSurface = false;

  double vm_SnowDepth = snowComponent->getVm_SnowDepth();

  // Berechnung der Bodenevaporation bis max. 4dm Tiefe
  pm_EvaporationZeta = _params.pm_EvaporationZeta; // Parameterdatei

  // Das sind die Steuerungsparameter für die Steigung der Entzugsfunktion
  vm_XSACriticalSoilMoisture = _params.pm_XSACriticalSoilMoisture;

  /** @todo <b>Claas:</b> pm_MaximumEvaporationImpactDepth ist aber Abhängig von der Bodenart,
   * da muss was dran gemacht werden */
  pm_MaximumEvaporationImpactDepth = _params.pm_MaximumEvaporationImpactDepth; // Parameterdatei


  // If a crop grows, ETp is taken from crop module
  if (vc_DevelopmentalStage > 0) {
    // Reference evapotranspiration is only grabbed here for consistent
    // output in monica.cpp
    if (vw_ReferenceEvapotranspiration < 0.0) {
      vm_ReferenceEvapotranspiration = monica.cropGrowth()->get_ReferenceEvapotranspiration();
    } else {
      vm_ReferenceEvapotranspiration = vw_ReferenceEvapotranspiration;
    }

    // Remaining ET from crop module already includes Kc factor and evaporation
    // from interception storage
    vm_PotentialEvapotranspiration = monica.cropGrowth()->get_RemainingEvapotranspiration();
    vc_EvaporatedFromIntercept = monica.cropGrowth()->get_EvaporatedFromIntercept();
  } else { // if no crop grows ETp is calculated from ET0 * kc

    // calculate reference evapotranspiration if not provided via climate files
    if (vw_ReferenceEvapotranspiration < 0.0) {
      vm_ReferenceEvapotranspiration = referenceEvapotranspiration(sm,
                                                                                vs_HeightNN,
                                                                                vw_MaxAirTemperature,
                                                                                vw_MinAirTemperature,
                                                                                vw_RelativeHumidity,
                                                                                vw_MeanAirTemperature,
                                                                                vw_WindSpeed,
                                                                                vw_WindSpeedHeight,
                                                                                vw_GlobalRadiation,
                                                                                vs_JulianDay,
                                                                                vs_Latitude);
    } else {
      // use reference evapotranspiration from climate file
      vm_ReferenceEvapotranspiration = vw_ReferenceEvapotranspiration;
    }

    vm_PotentialEvapotranspiration = vm_ReferenceEvapotranspiration * vc_KcFactor; // - vm_InterceptionReference;
  }

  vm_ActualEvaporation = 0.0;
  vm_ActualTranspiration = 0.0;

  // from HERMES:
  if (vm_PotentialEvapotranspiration > 6.5) vm_PotentialEvapotranspiration = 6.5;

  if (vm_PotentialEvapotranspiration > 0.0) {
    // If surface is water-logged, subsequent evaporation from surface water sources
    if (vm_SurfaceWaterStorage > 0.0) {
      vm_EvaporationFromSurface = true;
      // Water surface evaporates with Kc = 1.1.
      vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (1.1 / vc_KcFactor);

      // If a snow layer is present no water evaporates from surface water sources
      if (vm_SnowDepth > 0.0) {
        vm_EvaporatedFromSurface = 0.0;
      } else {
        if (vm_SurfaceWaterStorage < vm_PotentialEvapotranspiration) {
          vm_PotentialEvapotranspiration -= vm_SurfaceWaterStorage;
          vm_EvaporatedFromSurface = vm_SurfaceWaterStorage;
          vm_SurfaceWaterStorage = 0.0;
        } else {
          vm_SurfaceWaterStorage -= vm_PotentialEvapotranspiration;
          vm_EvaporatedFromSurface = vm_PotentialEvapotranspiration;
          vm_PotentialEvapotranspiration = 0.0;
        }
      }
      vm_PotentialEvapotranspiration = vm_PotentialEvapotranspiration * (vc_KcFactor / 1.1);
    }

    if (vm_PotentialEvapotranspiration > 0) { // Evaporation from soil

      // -----------------------------------------------------------------------
      // FAO-56 Dual Kc pathway — precompute potential soil evaporation (E_pot)
      // using the Ke coefficient.  This block runs ONLY when:
      //   • dualKcMethod is enabled in sim.json
      //   • A crop is actually present (vc_DevelopmentalStage > 0)
      // If either condition is false the existing Single Kc loop runs unchanged.
      // -----------------------------------------------------------------------
      double E_pot_dualKc = 0.0; // [mm d-1] replaces (1-beta)*PET per layer
      bool useDualKc = (monica.simulationParameters().dualKcMethod
                        && vc_DevelopmentalStage > 0
                        && cropModule != nullptr);
      if (useDualKc) {
        E_pot_dualKc = dualKcPrecomputation(sm, vw_WindSpeed, vw_MinAirTemperature, vw_MaxAirTemperature);
      } else {
        vm_Ke = 0.0;
      }
      // -----------------------------------------------------------------------
      // End of Dual Kc precomputation
      // -----------------------------------------------------------------------

      for (int i_Layer = 0; i_Layer < numberOfSoilLayers; i_Layer++) {
        vm_EReducer_1 = getEReducer1(sm,
                                                 i_Layer,
                                                 vc_PercentageSoilCoverage,
                                                 vm_PotentialEvapotranspiration);

        if (i_Layer >= pm_MaximumEvaporationImpactDepth) {
          // layer is too deep for evaporation
          vm_EReducer_2 = 0.0;
        } else {
          // 2nd factor to reduce actual evapotranspiration by
          // MaximumEvaporationImpactDepth and EvaporationZeta
          vm_EReducer_2 = getDeprivationFactor(i_Layer + 1,
                                                           pm_MaximumEvaporationImpactDepth,
                                                           pm_EvaporationZeta,
                                                           vm_LayerThickness[i_Layer]);
        }

        if (i_Layer > 0) {
          if (vm_SoilMoisture[i_Layer] < vm_SoilMoisture[i_Layer - 1]) {
            // 3rd factor to consider if above layer contains more water than
            // the adjacent layer below, evaporation will be significantly reduced
            vm_EReducer_3 = 0.1;
          } else {
            vm_EReducer_3 = 1.0;
          }
        } else {
          vm_EReducer_3 = 1.0;
        }
        // EReducer-> factor to reduce evaporation
        vm_EReducer = vm_EReducer_1 * vm_EReducer_2 * vm_EReducer_3;

        if (vc_DevelopmentalStage > 0) {
          // vegetation is present

          if (useDualKc) {
            // ---------------------------------------------------------------
            // FAO-56 Dual Kc: soil evaporation = Kr * (Kc_max - Kcb) * ET0
            // The EReducer chain still applies (soil moisture, depth limits).
            // Bypasses the Single Kc (1 - beta) * PET partitioning.
            // ---------------------------------------------------------------
            vm_Evaporation[i_Layer] = vm_EReducer * E_pot_dualKc;
          } else {
            // Single Kc: original (1 - beta) * EReducer * PET partitioning
            //Interpolation between [0,1]
            if (vc_PercentageSoilCoverage >= 0.0 && vc_PercentageSoilCoverage < 1.0) {
              vm_Evaporation[i_Layer] = ((1.0 - vc_PercentageSoilCoverage) * vm_EReducer)
                                        * vm_PotentialEvapotranspiration;
            } else {
              if (vc_PercentageSoilCoverage >= 1.0) {
                vm_Evaporation[i_Layer] = 0.0;
              }
            }
          }

          if (vm_SnowDepth > 0.0) vm_Evaporation[i_Layer] = 0.0;

          // Transpiration is derived from ET0; Soil coverage and Kc factors
          // already considered in crop part!
          vm_Transpiration[i_Layer] = monica.cropGrowth()->get_Transpiration(i_Layer);

          //std::cout << setprecision(11) << "vm_Transpiration[i_Layer]: " << i_Layer << ", " << vm_Transpiration[i_Layer] << std::endl;

          // Transpiration is capped in case potential ET after surface
          // and interception evaporation has occurred on same day
          if (vm_EvaporationFromSurface) {
            vm_Transpiration[i_Layer] = vc_PercentageSoilCoverage * vm_EReducer * vm_PotentialEvapotranspiration;
          }
        } else {
          // no vegetation present — Single Kc / bare soil, always unchanged
          if (vm_SnowDepth > 0.0) {
            vm_Evaporation[i_Layer] = 0.0;
          } else {
            vm_Evaporation[i_Layer] = vm_PotentialEvapotranspiration * vm_EReducer;
          }
          vm_Transpiration[i_Layer] = 0.0;
        } // if(vc_DevelopmentalStage > 0)

        vm_Evapotranspiration[i_Layer] = vm_Evaporation[i_Layer] + vm_Transpiration[i_Layer];
        vm_SoilMoisture[i_Layer] -= (vm_Evapotranspiration[i_Layer] / 1000.0 / vm_LayerThickness[i_Layer]);

        //  Generelle Begrenzung des Evaporationsentzuges
        if (vm_SoilMoisture[i_Layer] < 0.01) {
          vm_SoilMoisture[i_Layer] = 0.01;
        }

        vm_ActualTranspiration += vm_Transpiration[i_Layer];
        vm_ActualEvaporation += vm_Evaporation[i_Layer];
      } // for
    } // vm_PotentialEvapotranspiration > 0
  } // vm_PotentialEvapotranspiration > 0.0
  vm_ActualEvapotranspiration = vm_ActualTranspiration + vm_ActualEvaporation
                                + vc_EvaporatedFromIntercept + vm_EvaporatedFromSurface;
}

/**
 * @brief Reference evapotranspiration
 *
 * A method following Penman-Monteith as described by the FAO in Allen
 * RG, Pereira LS, Raes D, Smith M. (1998) Crop evapotranspiration.
 * Guidelines for computing crop water requirements. FAO Irrigation and
 * Drainage Paper 56, FAO, Roma
 *
 * @param vs_HeightNN
 * @param vw_MaxAirTemperature
 * @param vw_MinAirTemperature
 * @param vw_RelativeHumidity
 * @param vw_MeanAirTemperature
 * @param vw_WindSpeed
 * @param vw_WindSpeedHeight
 * @param vw_GlobalRadiation
 * @return
 */
double referenceEvapotranspiration(SoilMoisture* sm,
                                                       double vs_HeightNN,
                                                       double vw_MaxAirTemperature,
                                                       double vw_MinAirTemperature,
                                                       double vw_RelativeHumidity,
                                                       double vw_MeanAirTemperature,
                                                       double vw_WindSpeed,
                                                       double vw_WindSpeedHeight,
                                                       double vw_GlobalRadiation,
                                                       int vs_JulianDay,
                                                       double vs_Latitude) {
  auto& cropPs = sm->cropPs;
  auto& vc_StomataResistance = sm->vc_StomataResistance;
  auto& vw_NetRadiation = sm->vw_NetRadiation;

  double vc_Declination;
  double vc_DeclinationSinus; // old SINLD
  double vc_DeclinationCosinus; // old COSLD
  double vc_AstronomicDayLenght;
  double vc_EffectiveDayLenght;
  double vc_PhotoperiodicDaylength;
  double vc_PhotActRadiationMean;
  double vc_ClearDayRadiation;
  double vc_OvercastDayRadiation;

  double vm_AtmosphericPressure; //[kPA]
  double vm_PsycrometerConstant; //[kPA °C-1]
  double vm_SaturatedVapourPressureMax; //[kPA]
  double vm_SaturatedVapourPressureMin; //[kPA]
  double vm_SaturatedVapourPressure; //[kPA]
  double vm_VapourPressure; //[kPA]
  double vm_SaturationDeficit; //[kPA]
  double vm_SaturatedVapourPressureSlope; //[kPA °C-1]
  double vm_WindSpeed_2m; //[m s-1]
  double vm_AerodynamicResistance; //[s m-1]
  double vm_SurfaceResistance; //[s m-1]
  double vc_ExtraterrestrialRadiation;
  double vm_ReferenceEvapotranspiration; //[mm]
  double pc_ReferenceAlbedo = cropPs.pc_ReferenceAlbedo; // FAO Green gras reference albedo from Allen et al. (1998)
  double PI = 3.14159265358979323;

  vc_Declination = -23.4 * cos(2.0 * PI * ((vs_JulianDay + 10.0) / 365.0));
  vc_DeclinationSinus = sin(vc_Declination * PI / 180.0) * sin(vs_Latitude * PI / 180.0);
  vc_DeclinationCosinus = cos(vc_Declination * PI / 180.0) * cos(vs_Latitude * PI / 180.0);

  double arg_AstroDayLength = vc_DeclinationSinus / vc_DeclinationCosinus;
  arg_AstroDayLength = bound(-1.0, arg_AstroDayLength, 1.0); //The argument of asin must be in the range of -1 to 1
  vc_AstronomicDayLenght = 12.0 * (PI + 2.0 * asin(arg_AstroDayLength)) / PI;

  double arg_EffectiveDayLength = (-sin(8.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus;
  arg_EffectiveDayLength = bound(-1.0, arg_EffectiveDayLength, 1.0);
  //The argument of asin must be in the range of -1 to 1
  vc_EffectiveDayLenght = 12.0 * (PI + 2.0 * asin(arg_EffectiveDayLength)) / PI;

  double arg_PhotoDayLength = (-sin(-6.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus;
  arg_PhotoDayLength = bound(-1.0, arg_PhotoDayLength, 1.0); //The argument of asin must be in the range of -1 to 1
  vc_PhotoperiodicDaylength = 12.0 * (PI + 2.0 * asin(arg_PhotoDayLength)) / PI;

  double arg_PhotAct =
    min(1.0, ((vc_DeclinationSinus / vc_DeclinationCosinus) * (vc_DeclinationSinus / vc_DeclinationCosinus)));
  //The argument of sqrt must be >= 0
  vc_PhotActRadiationMean = 3600.0 * (vc_DeclinationSinus * vc_AstronomicDayLenght + 24.0 / PI * vc_DeclinationCosinus
                                      * sqrt(1.0 - arg_PhotAct));


  vc_ClearDayRadiation = 0;
  if (vc_PhotActRadiationMean > 0 && vc_AstronomicDayLenght > 0) {
    vc_ClearDayRadiation = 0.5 * 1300.0 * vc_PhotActRadiationMean * exp(-0.14 / (vc_PhotActRadiationMean
                                                                          / (vc_AstronomicDayLenght * 3600.0)));
  }

  vc_OvercastDayRadiation = 0.2 * vc_ClearDayRadiation;
  double SC = 24.0 * 60.0 / PI * 8.20 * (1.0 + 0.033 * cos(2.0 * PI * vs_JulianDay / 365.0));
  double arg_SHA = bound(-1.0, -tan(vs_Latitude * PI / 180.0) * tan(vc_Declination * PI / 180.0), 1.0);
  //The argument of acos must be in the range of -1 to 1
  double SHA = acos(arg_SHA);

  vc_ExtraterrestrialRadiation = SC * (SHA * vc_DeclinationSinus + vc_DeclinationCosinus * sin(SHA)) / 100.0;
  // [J cm-2] --> [MJ m-2]

  // Calculation of atmospheric pressure
  vm_AtmosphericPressure = 101.3 * pow(((293.0 - (0.0065 * vs_HeightNN)) / 293.0), 5.26);

  // Calculation of psychrometer constant - Luchtfeuchtigkeit
  vm_PsycrometerConstant = 0.000665 * vm_AtmosphericPressure;

  // Calc. of saturated water vapour pressure at daily max temperature
  vm_SaturatedVapourPressureMax = 0.6108 * exp((17.27 * vw_MaxAirTemperature) / (237.3 + vw_MaxAirTemperature));

  // Calc. of saturated water vapour pressure at daily min temperature
  vm_SaturatedVapourPressureMin = 0.6108 * exp((17.27 * vw_MinAirTemperature) / (237.3 + vw_MinAirTemperature));

  // Calculation of the saturated water vapour pressure
  vm_SaturatedVapourPressure = (vm_SaturatedVapourPressureMax + vm_SaturatedVapourPressureMin) / 2.0;

  // Calculation of the water vapour pressure
  if (vw_RelativeHumidity <= 0.0) {
    // Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
    vm_VapourPressure = vm_SaturatedVapourPressureMin;
  } else {
    vm_VapourPressure = vw_RelativeHumidity * vm_SaturatedVapourPressure;
  }

  // Calculation of the air saturation deficit
  vm_SaturationDeficit = vm_SaturatedVapourPressure - vm_VapourPressure;

  // Slope of saturation water vapour pressure-to-temperature relation
  vm_SaturatedVapourPressureSlope = (4098.0 * (0.6108 * exp((17.27 * vw_MeanAirTemperature) / (vw_MeanAirTemperature
                                                              + 237.3)))) / (
                                      (vw_MeanAirTemperature + 237.3) * (vw_MeanAirTemperature + 237.3));

  // Calculation of wind speed in 2m height
  vm_WindSpeed_2m = max(0.5, vw_WindSpeed * (4.87 / (log(67.8 * vw_WindSpeedHeight - 5.42))));
  // 0.5 minimum allowed windspeed for Penman-Monteith-Method FAO

  // Calculation of the aerodynamic resistance
  vm_AerodynamicResistance = 208.0 / vm_WindSpeed_2m;

  vc_StomataResistance = 100; // FAO default value [s m-1]

  vm_SurfaceResistance = vc_StomataResistance / 1.44;

  double vc_ClearSkySolarRadiation = (0.75 + 0.00002 * vs_HeightNN) * vc_ExtraterrestrialRadiation;
  double vc_RelativeShortwaveRadiation = vc_ClearSkySolarRadiation > 0
                                           ? min(vw_GlobalRadiation / vc_ClearSkySolarRadiation, 1.0)
                                           : 1.0;

  double pc_BolzmannConstant = 0.0000000049;
  double vc_ShortwaveRadiation = (1.0 - pc_ReferenceAlbedo) * vw_GlobalRadiation;
  double vc_LongwaveRadiation = pc_BolzmannConstant
                                * ((pow((vw_MinAirTemperature + 273.16), 4.0)
                                    + pow((vw_MaxAirTemperature + 273.16), 4.0)) / 2.0)
                                * (1.35 * vc_RelativeShortwaveRadiation - 0.35)
                                * (0.34 - 0.14 * sqrt(vm_VapourPressure));
  vw_NetRadiation = vc_ShortwaveRadiation - vc_LongwaveRadiation;

  // Calculation of the reference evapotranspiration
  // Penman-Monteith-Methode FAO
  vm_ReferenceEvapotranspiration = ((0.408 * vm_SaturatedVapourPressureSlope * vw_NetRadiation)
                                    + (vm_PsycrometerConstant * (900.0 / (vw_MeanAirTemperature + 273.0))
                                       * vm_WindSpeed_2m * vm_SaturationDeficit))
                                   / (vm_SaturatedVapourPressureSlope + vm_PsycrometerConstant
                                      * (1.0 + (vm_SurfaceResistance / 208.0) * vm_WindSpeed_2m));

  if (vm_ReferenceEvapotranspiration < 0.0) {
    vm_ReferenceEvapotranspiration = 0.0;
  }

  return vm_ReferenceEvapotranspiration;
}

double getFrostDepth(const SoilMoisture* sm) { return sm->frostComponent->getFrostDepth(); }

//! Returns thaw depth [m]
double getThawDepth(const SoilMoisture* sm) { return sm->frostComponent->getThawDepth(); }

double getMaxSnowDepth(const SoilMoisture* sm) { return sm->snowComponent->getMaxSnowDepth(); }

double getAccumulatedSnowDepth(const SoilMoisture* sm) {
  return sm->snowComponent->getAccumulatedSnowDepth();
}

double getAccumulatedFrostDepth(const SoilMoisture* sm) {
  return sm->frostComponent->getAccumulatedFrostDepth();
}

/*!
 * Calculation of evaporation reduction by soil moisture content
 *
 * @param i_Layer
 * @param vm_PercentageSoilCoverage
 * @param vm_ReferenceEvapotranspiration
 *
 * @return Value for evaporation reduction by soil moisture content
 */
double getEReducer1(const SoilMoisture* sm,
                                        int i_Layer,
                                        double vm_PercentageSoilCoverage,
                                        double vm_ReferenceEvapotranspiration) {
  double vm_EReductionFactor;
  int vm_EvaporationReductionMethod = 1;
  double vm_SoilMoisture_m3 = sm->soilColumn[i_Layer].vs_SoilMoisture_m3;
  double vm_PWP = sm->soilColumn[i_Layer]._sps.vs_PermanentWiltingPoint;
  double vm_FK = sm->soilColumn[i_Layer]._sps.vs_FieldCapacity;
  double vm_RelativeEvaporableWater;
  double vm_CriticalSoilMoisture;
  double vm_XSA;
  double vm_Reducer;

  if (vm_SoilMoisture_m3 < (0.33 * vm_PWP)) vm_SoilMoisture_m3 = 0.33 * vm_PWP;

  vm_RelativeEvaporableWater = (vm_SoilMoisture_m3 - (0.33 * vm_PWP))
                               / (vm_FK - (0.33 * vm_PWP));

  if (vm_RelativeEvaporableWater > 1.0) vm_RelativeEvaporableWater = 1.0;

  if (vm_EvaporationReductionMethod == 0) {
    // THESEUS
    vm_CriticalSoilMoisture = 0.65 * vm_FK;
    if (vm_PercentageSoilCoverage > 0) {
      if (vm_ReferenceEvapotranspiration > 2.5) {
        vm_XSA = (0.65 * vm_FK - vm_PWP) * (vm_FK - vm_PWP);
        vm_Reducer = vm_XSA + (((1 - vm_XSA) / 17.5)
                               * (vm_ReferenceEvapotranspiration - 2.5));
      } else {
        vm_Reducer = sm->vm_XSACriticalSoilMoisture / 2.5 * vm_ReferenceEvapotranspiration;
      }
      vm_CriticalSoilMoisture = sm->soilColumn[i_Layer]._sps.vs_FieldCapacity * vm_Reducer;
    }

    // Calculation of an evaporation-reducing factor in relation to soil water content
    if (vm_SoilMoisture_m3 > vm_CriticalSoilMoisture) {
      // Moisture is higher than critical value so there is a
      // normal evaporation and nothing must be reduced
      vm_EReductionFactor = 1.0;
    } else {
      // critical value is reached, actual evaporation is below potential

      if (vm_SoilMoisture_m3 > (0.33 * vm_PWP)) {
        // moisture is higher than 30% of permanent wilting point
        vm_EReductionFactor = vm_RelativeEvaporableWater;
      } else {
        // if moisture is below 30% of wilting point nothing can be evaporated
        vm_EReductionFactor = 0.0;
      }
    }
  } else if (vm_EvaporationReductionMethod == 1) {
    // HERMES
    vm_EReductionFactor = 0.0;
    if (vm_RelativeEvaporableWater > 0.33) {
      vm_EReductionFactor = 1.0 - (0.1 * (1.0 - vm_RelativeEvaporableWater) / (1.0 - 0.33));
    } else if (vm_RelativeEvaporableWater > 0.22) {
      vm_EReductionFactor = 0.9 - (0.625 * (0.33 - vm_RelativeEvaporableWater) / (0.33 - 0.22));
    } else if (vm_RelativeEvaporableWater > 0.2) {
      vm_EReductionFactor = 0.275 - (0.225 * (0.22 - vm_RelativeEvaporableWater) / (0.22 - 0.2));
    } else {
      vm_EReductionFactor = 0.05 - (0.05 * (0.2 - vm_RelativeEvaporableWater) / 0.2);
    } // end if
  }
  return vm_EReductionFactor;
}

/*!
 * @brief Calculation of deprivation factor
 * @return deprivationFactor
 *
 * PET deprivation distribution (factor as function of depth).
 * The PET is spread over the deprivation depth. This function computes
 * the factor/weight for a given layer/depth[dm] (layerNo).
 *
 * @param layerNo [1..NLAYER]
 * @param deprivationDepth [dm] maximum deprivation depth
 * @param zeta [0..40] shape factor
 * @param vs_LayerThickness
 */
double getDeprivationFactor(int layerNo,
                                                double deprivationDepth,
                                                double zeta,
                                                double vs_LayerThickness) {
  // factor (f(depth)) to distribute the PET along the soil profil/rooting zone

  double deprivationFactor;

  // factor to introduce layer thickness in this algorithm,
  // to allow layer thickness scaling (Claas Nendel)
  double layerThicknessFactor = deprivationDepth / (vs_LayerThickness * 10.0);

  if ((fabs(zeta)) < 0.0003) {
    deprivationFactor = (2.0 / layerThicknessFactor) - (1.0 / (layerThicknessFactor * layerThicknessFactor)) * (2
                          * layerNo - 1);
    return deprivationFactor;
  } else {
    double c2 = 0.0;
    double c3 = 0.0;
    c2 = log((layerThicknessFactor + zeta * layerNo) / (layerThicknessFactor + zeta * (layerNo - 1)));
    c3 = zeta / (layerThicknessFactor * (zeta + 1.0));
    deprivationFactor = (c2 - c3) / (log(zeta + 1.0) - zeta / (zeta + 1.0));
    return deprivationFactor;
  }
}

/*!
 * @brief Calculates mean of water content until the given depth.
 * @param depth_m depth
 *
 * Accumulates moisture values of soil layers until the given depth is reached.
 * The mean moisture value is returned.
 */
double meanWaterContent(const SoilMoisture* sm, double depth_m) {
  double lsum = 0.0, sum = 0.0;
  int count = 0;

  for (int i = 0; i < sm->numberOfSoilLayers; i++) {
    count++;
    double smm3 = sm->soilColumn[i].vs_SoilMoisture_m3;
    double fc = sm->soilColumn[i]._sps.vs_FieldCapacity;
    double pwp = sm->soilColumn[i]._sps.vs_PermanentWiltingPoint;
    sum += smm3 / (fc - pwp); //[%nFK]
    lsum += sm->soilColumn[i].vs_LayerThickness;
    if (lsum >= depth_m) break;
  }

  return sum / double(count);
}


double meanWaterContent(const SoilMoisture* sm, int layer, int number_of_layers) {
  double sum = 0.0;
  int count = 0;

  if (layer + number_of_layers > sm->numberOfSoilLayers) {
    return -1;
  }

  for (int i = layer; i < layer + number_of_layers; i++) {
    count++;
    double smm3 = sm->soilColumn[i].vs_SoilMoisture_m3;
    double fc = sm->soilColumn[i]._sps.vs_FieldCapacity;
    double pwp = sm->soilColumn[i]._sps.vs_PermanentWiltingPoint;
    sum += smm3 / (fc - pwp); //[%nFK]
  }

  return sum / double(count);
}


/**
 * @brief Returns Kc factor
 * @return Kc factor
 */
double getTemperatureUnderSnow(const SoilMoisture* sm) {
  return sm->frostComponent->getTemperatureUnderSnow();
}

kj::Own<SoilMoisture> makeSoilMoisture(MonicaModel& monica, const SoilMoistureModuleParameters& params) {
  auto sm = kj::heap<SoilMoisture>(SoilMoisture{
      0.0,
      monica.soilColumnNC(),
      monica.siteParameters(),
      monica,
      params,
      monica.environmentParameters(),
      monica.cropParameters()});
  initializeFromParams(sm.get());
  return sm;
}

kj::Own<SoilMoisture> makeSoilMoisture(
  MonicaModel& monica,
  mas::schema::model::monica::SoilMoistureModuleState::Reader reader,
  CropModule* cropModule) {
  auto sm = kj::heap<SoilMoisture>(SoilMoisture{
      0.0,
      monica.soilColumnNC(),
      monica.siteParameters(),
      monica,
      {},
      monica.environmentParameters(),
      monica.cropParameters()});
  sm->cropModule = cropModule;
  deserialize(sm.get(), reader);
  return sm;
}

void deserialize(SoilMoisture* sm, mas::schema::model::monica::SoilMoistureModuleState::Reader reader) {
  sm->_params.deserialize(reader.getModuleParams());
  sm->numberOfMoistureLayers = reader.getNumberOfLayers();
  sm->numberOfSoilLayers = reader.getVsNumberOfLayers();
  sm->vm_ActualEvaporation = reader.getActualEvaporation();
  sm->vm_ActualEvapotranspiration = reader.getActualEvapotranspiration();
  sm->vm_ActualTranspiration = reader.getActualTranspiration();
  setFromCapnpList(sm->vm_AvailableWater, reader.getAvailableWater());
  sm->vm_CapillaryRise = reader.getCapillaryRise();
  setFromCapnpList(sm->pm_CapillaryRiseRate, reader.getCapillaryRiseRate());
  setFromCapnpList(sm->vm_CapillaryWater, reader.getCapillaryWater());
  setFromCapnpList(sm->vm_CapillaryWater70, reader.getCapillaryWater70());
  setFromCapnpList(sm->vm_Evaporation, reader.getEvaporation());
  setFromCapnpList(sm->vm_Evapotranspiration, reader.getEvapotranspiration());
  setFromCapnpList(sm->vm_FieldCapacity, reader.getFieldCapacity());
  sm->vm_FluxAtLowerBoundary = reader.getFluxAtLowerBoundary();
  setFromCapnpList(sm->vm_GravitationalWater, reader.getGravitationalWater());
  sm->vm_GrossPrecipitation = reader.getGrossPrecipitation();
  sm->vm_GroundwaterAdded = reader.getGroundwaterAdded();
  sm->vm_GroundwaterDischarge = reader.getGroundwaterDischarge();
  sm->vm_GroundwaterTableLayer = reader.getGroundwaterTable();
  setFromCapnpList(sm->vm_HeatConductivity, reader.getHeatConductivity());
  sm->vm_HydraulicConductivityRedux = reader.getHydraulicConductivityRedux();
  sm->vm_Infiltration = reader.getInfiltration();
  sm->vm_Interception = reader.getInterception();
  sm->vc_KcFactor = reader.getVcKcFactor();
  setFromCapnpList(sm->vm_Lambda, reader.getLambda());
  sm->vs_Latitude = reader.getVsLatitude();
  setFromCapnpList(sm->vm_LayerThickness, reader.getLayerThickness());
  sm->pm_LayerThickness = reader.getPmLayerThickness();
  sm->pm_LeachingDepth = reader.getPmLeachingDepth();
  sm->pm_LeachingDepthLayer = reader.getPmLeachingDepthLayer();
  sm->pm_MaxPercolationRate = reader.getPmMaxPercolationRate();
  sm->vc_NetPrecipitation = reader.getVcNetPrecipitation();
  sm->vw_NetRadiation = reader.getVwNetRadiation();
  setFromCapnpList(sm->vm_PermanentWiltingPoint, reader.getPermanentWiltingPoint());
  sm->vc_PercentageSoilCoverage = reader.getVcPercentageSoilCoverage();
  setFromCapnpList(sm->vm_PercolationRate, reader.getPercolationRate());
  sm->vm_ReferenceEvapotranspiration = reader.getReferenceEvapotranspiration();
  setFromCapnpList(sm->vm_ResidualEvapotranspiration, reader.getResidualEvapotranspiration());
  setFromCapnpList(sm->vm_SaturatedHydraulicConductivity, reader.getSaturatedHydraulicConductivity());
  setFromCapnpList(sm->vm_SoilMoisture, reader.getSoilMoisture());
  sm->vm_SoilMoisture_crit = reader.getSoilMoisturecrit();
  sm->vm_SoilMoistureDeficit = reader.getSoilMoistureDeficit();
  setFromCapnpList(sm->vm_SoilPoreVolume, reader.getSoilPoreVolume());
  sm->vc_StomataResistance = reader.getVcStomataResistance();
  sm->vm_SurfaceRoughness = reader.getSurfaceRoughness();
  sm->vm_SurfaceRunOff = reader.getSurfaceRunOff();
  sm->vm_SumSurfaceRunOff = reader.getSumSurfaceRunOff();
  sm->vm_SurfaceWaterStorage = reader.getSurfaceWaterStorage();
  sm->pt_TimeStep = reader.getPtTimeStep();
  sm->vm_TotalWaterRemoval = reader.getTotalWaterRemoval();
  setFromCapnpList(sm->vm_Transpiration, reader.getTranspiration());
  setFromCapnpList(sm->vm_WaterFlux, reader.getWaterFlux());
  sm->vm_XSACriticalSoilMoisture = reader.getXSACriticalSoilMoisture();
  if (reader.hasSnowComponent()) {
    sm->snowComponent = nullptr;
    sm->snowComponent = kj::heap<SnowComponent>(sm->soilColumn, reader.getSnowComponent());
  }
  if (reader.hasFrostComponent()) {
    sm->frostComponent = nullptr;
    sm->frostComponent = kj::heap<FrostComponent>(sm->soilColumn, reader.getFrostComponent());
  }
}

void serialize(const SoilMoisture* sm, mas::schema::model::monica::SoilMoistureModuleState::Builder builder) {
  sm->_params.serialize(builder.initModuleParams());
  builder.setNumberOfLayers((uint16_t)sm->numberOfMoistureLayers);
  builder.setVsNumberOfLayers((uint16_t)sm->numberOfSoilLayers);
  builder.setActualEvaporation(sm->vm_ActualEvaporation);
  builder.setActualEvapotranspiration(sm->vm_ActualEvapotranspiration);
  builder.setActualTranspiration(sm->vm_ActualTranspiration);
  setCapnpList(sm->vm_AvailableWater, builder.initAvailableWater((capnp::uint)sm->vm_AvailableWater.size()));
  builder.setCapillaryRise(sm->vm_CapillaryRise);
  setCapnpList(sm->pm_CapillaryRiseRate, builder.initCapillaryRiseRate((capnp::uint)sm->pm_CapillaryRiseRate.size()));
  setCapnpList(sm->vm_CapillaryWater, builder.initCapillaryWater((capnp::uint)sm->vm_CapillaryWater.size()));
  setCapnpList(sm->vm_CapillaryWater70, builder.initCapillaryWater70((capnp::uint)sm->vm_CapillaryWater70.size()));
  setCapnpList(sm->vm_Evaporation, builder.initEvaporation((capnp::uint)sm->vm_Evaporation.size()));
  setCapnpList(sm->vm_Evapotranspiration, builder.initEvapotranspiration((capnp::uint)sm->vm_Evapotranspiration.size()));
  setCapnpList(sm->vm_FieldCapacity, builder.initFieldCapacity((capnp::uint)sm->vm_FieldCapacity.size()));
  builder.setFluxAtLowerBoundary(sm->vm_FluxAtLowerBoundary);
  setCapnpList(sm->vm_GravitationalWater, builder.initGravitationalWater((capnp::uint)sm->vm_GravitationalWater.size()));
  builder.setGrossPrecipitation(sm->vm_GrossPrecipitation);
  builder.setGroundwaterAdded(sm->vm_GroundwaterAdded);
  builder.setGroundwaterDischarge(sm->vm_GroundwaterDischarge);
  builder.setGroundwaterTable((uint16_t)sm->vm_GroundwaterTableLayer);
  setCapnpList(sm->vm_HeatConductivity, builder.initHeatConductivity((capnp::uint)sm->vm_HeatConductivity.size()));
  builder.setHydraulicConductivityRedux(sm->vm_HydraulicConductivityRedux);
  builder.setInfiltration(sm->vm_Infiltration);
  builder.setInterception(sm->vm_Interception);
  builder.setVcKcFactor(sm->vc_KcFactor);
  setCapnpList(sm->vm_Lambda, builder.initLambda((capnp::uint)sm->vm_Lambda.size()));
  builder.setVsLatitude(sm->vs_Latitude);
  setCapnpList(sm->vm_LayerThickness, builder.initLayerThickness((capnp::uint)sm->vm_LayerThickness.size()));
  builder.setPmLayerThickness(sm->pm_LayerThickness);
  builder.setPmLeachingDepth(sm->pm_LeachingDepth);
  builder.setPmLeachingDepthLayer(sm->pm_LeachingDepthLayer);
  builder.setPmMaxPercolationRate(sm->pm_MaxPercolationRate);
  builder.setVcNetPrecipitation(sm->vc_NetPrecipitation);
  builder.setVwNetRadiation(sm->vw_NetRadiation);
  setCapnpList(sm->vm_PermanentWiltingPoint,
               builder.initPermanentWiltingPoint((capnp::uint)sm->vm_PermanentWiltingPoint.size()));
  builder.setVcPercentageSoilCoverage(sm->vc_PercentageSoilCoverage);
  setCapnpList(sm->vm_PercolationRate, builder.initPercolationRate((capnp::uint)sm->vm_PercolationRate.size()));
  builder.setReferenceEvapotranspiration(sm->vm_ReferenceEvapotranspiration);
  setCapnpList(sm->vm_ResidualEvapotranspiration,
               builder.initResidualEvapotranspiration((capnp::uint)sm->vm_ResidualEvapotranspiration.size()));
  setCapnpList(sm->vm_SaturatedHydraulicConductivity,
               builder.initSaturatedHydraulicConductivity((capnp::uint)sm->vm_SaturatedHydraulicConductivity.size()));
  setCapnpList(sm->vm_SoilMoisture, builder.initSoilMoisture((capnp::uint)sm->vm_SoilMoisture.size()));
  builder.setSoilMoisturecrit(sm->vm_SoilMoisture_crit);
  builder.setSoilMoistureDeficit(sm->vm_SoilMoistureDeficit);
  setCapnpList(sm->vm_SoilPoreVolume, builder.initSoilPoreVolume((capnp::uint)sm->vm_SoilPoreVolume.size()));
  builder.setVcStomataResistance(sm->vc_StomataResistance);
  builder.setSurfaceRoughness(sm->vm_SurfaceRoughness);
  builder.setSurfaceRunOff(sm->vm_SurfaceRunOff);
  builder.setSumSurfaceRunOff(sm->vm_SumSurfaceRunOff);
  builder.setSurfaceWaterStorage(sm->vm_SurfaceWaterStorage);
  builder.setPtTimeStep(sm->pt_TimeStep);
  builder.setTotalWaterRemoval(sm->vm_TotalWaterRemoval);
  setCapnpList(sm->vm_Transpiration, builder.initTranspiration((capnp::uint)sm->vm_Transpiration.size()));
  setCapnpList(sm->vm_WaterFlux, builder.initWaterFlux((capnp::uint)sm->vm_WaterFlux.size()));
  builder.setXSACriticalSoilMoisture(sm->vm_XSACriticalSoilMoisture);
  if (sm->snowComponent) sm->snowComponent->serialize(builder.initSnowComponent());
  if (sm->frostComponent) sm->frostComponent->serialize(builder.initFrostComponent());
}

std::pair<double, double> getSnowDepthAndCalcTemperatureUnderSnow(const SoilMoisture* sm, double avgAirTemp) {
  double snowDepth = sm->snowComponent->getVm_SnowDepth();
  return make_pair(snowDepth, sm->frostComponent->calcTemperatureUnderSnow(avgAirTemp, snowDepth));
}

} // namespace soilmoisture
} // namespace monica
