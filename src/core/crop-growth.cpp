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

/** @todo Christian: Strahlungskonzept. Welche Information wird wo verwendet? */

#include <cmath>
#include <string>

#include "crop-growth.h"
#include "tools/debug.h"
#include "soilmoisture.h"
#include "monica-parameters.h"
#include "tools/helper.h"
#include "voc-guenther.h"
#include "voc-jjv.h"

const double PI = 3.14159265358979323;

using namespace std;
using namespace Monica;
using namespace Tools;

/**
 * @brief Constructor
 * @param sc Soil column
 * @param gps General parameters
 * @param cps crop parameters
 * @param stps site parameters
 *
 * @author Claas Nendel
 */
CropGrowth::CropGrowth(SoilColumn& sc,
											 const CropParameters& cps,
											 const SiteParameters& stps,
											 const UserCropParameters& cropPs,
											 const SimulationParameters& simPs,
											 int usage)
	: soilColumn(sc)
	, cropPs(cropPs)
	, vs_Latitude(stps.vs_Latitude)
	, pc_AbovegroundOrgan(cps.speciesParams.pc_AbovegroundOrgan)
	, pc_AssimilatePartitioningCoeff(cps.cultivarParams.pc_AssimilatePartitioningCoeff)
	, pc_AssimilateReallocation(cps.speciesParams.pc_AssimilateReallocation)
	, pc_BaseDaylength(cps.cultivarParams.pc_BaseDaylength)
	, pc_BaseTemperature(cps.speciesParams.pc_BaseTemperature)
	, pc_BeginSensitivePhaseHeatStress(cps.cultivarParams.pc_BeginSensitivePhaseHeatStress)
	, pc_CarboxylationPathway(cps.speciesParams.pc_CarboxylationPathway)
	//  , pc_CO2Method(cps.pc_CO2Method)
	, pc_CriticalOxygenContent(cps.speciesParams.pc_CriticalOxygenContent)
	, pc_CriticalTemperatureHeatStress(cps.cultivarParams.pc_CriticalTemperatureHeatStress)
	, pc_CropHeightP1(cps.cultivarParams.pc_CropHeightP1)
	, pc_CropHeightP2(cps.cultivarParams.pc_CropHeightP2)
	, pc_CropName(cps.pc_CropName())
	, pc_CropSpecificMaxRootingDepth(cps.cultivarParams.pc_CropSpecificMaxRootingDepth)
	, vc_CurrentTemperatureSum(cps.speciesParams.pc_NumberOfDevelopmentalStages(), 0.0)
	, pc_CuttingDelayDays(cps.speciesParams.pc_CuttingDelayDays)
	, pc_DaylengthRequirement(cps.cultivarParams.pc_DaylengthRequirement)
	, pc_DefaultRadiationUseEfficiency(cps.speciesParams.pc_DefaultRadiationUseEfficiency)
	, pc_DevelopmentAccelerationByNitrogenStress(cps.speciesParams.pc_DevelopmentAccelerationByNitrogenStress)
	, pc_DroughtStressThreshold(cps.cultivarParams.pc_DroughtStressThreshold)
	, pc_DroughtImpactOnFertilityFactor(cps.speciesParams.pc_DroughtImpactOnFertilityFactor)
	, pc_EmergenceFloodingControlOn(simPs.pc_EmergenceFloodingControlOn)
	, pc_EmergenceMoistureControlOn(simPs.pc_EmergenceMoistureControlOn)
	, pc_EndSensitivePhaseHeatStress(cps.cultivarParams.pc_EndSensitivePhaseHeatStress)
	, pc_FieldConditionModifier(cps.speciesParams.pc_FieldConditionModifier)
	, vo_FreshSoilOrganicMatter(soilColumn.vs_NumberOfLayers(), 0.0)
	, pc_FrostDehardening(cps.cultivarParams.pc_FrostDehardening)
	, pc_FrostHardening(cps.cultivarParams.pc_FrostHardening)
	, pc_HeatSumIrrigationStart(cps.cultivarParams.pc_HeatSumIrrigationStart)
	, pc_HeatSumIrrigationEnd(cps.cultivarParams.pc_HeatSumIrrigationEnd)
	, vs_HeightNN(stps.vs_HeightNN)
	, pc_InitialKcFactor(cps.speciesParams.pc_InitialKcFactor)
	, pc_InitialOrganBiomass(cps.speciesParams.pc_InitialOrganBiomass)
	, pc_InitialRootingDepth(cps.speciesParams.pc_InitialRootingDepth)
	, pc_LowTemperatureExposure(cps.cultivarParams.pc_LowTemperatureExposure)
	, pc_LimitingTemperatureHeatStress(cps.speciesParams.pc_LimitingTemperatureHeatStress)
	, pc_LT50cultivar(cps.cultivarParams.pc_LT50cultivar)
	, pc_LuxuryNCoeff(cps.speciesParams.pc_LuxuryNCoeff)
	, pc_MaxAssimilationRate(cps.cultivarParams.pc_MaxAssimilationRate)
	, pc_MaxCropDiameter(cps.speciesParams.pc_MaxCropDiameter)
	, pc_MaxCropHeight(cps.cultivarParams.pc_MaxCropHeight)
	, pc_MaxNUptakeParam(cps.speciesParams.pc_MaxNUptakeParam)
	, pc_MinimumNConcentration(cps.speciesParams.pc_MinimumNConcentration)
	, pc_MinimumTemperatureForAssimilation(cps.speciesParams.pc_MinimumTemperatureForAssimilation)
	, pc_MinimumTemperatureRootGrowth(cps.speciesParams.pc_MinimumTemperatureRootGrowth)
	, pc_NConcentrationAbovegroundBiomass(cps.speciesParams.pc_NConcentrationAbovegroundBiomass)
	, pc_NConcentrationB0(cps.speciesParams.pc_NConcentrationB0)
	, pc_NConcentrationPN(cps.speciesParams.pc_NConcentrationPN)
	, pc_NConcentrationRoot(cps.speciesParams.pc_NConcentrationRoot)
	, pc_NitrogenResponseOn(simPs.pc_NitrogenResponseOn)
	, pc_NumberOfDevelopmentalStages(cps.speciesParams.pc_NumberOfDevelopmentalStages())
	, pc_NumberOfOrgans(cps.speciesParams.pc_NumberOfOrgans())
	, vc_NUptakeFromLayer(soilColumn.vs_NumberOfLayers(), 0.0)
	, pc_OptimumTemperature(cps.cultivarParams.pc_OptimumTemperature)
	, vc_OrganBiomass(pc_NumberOfOrgans, 0.0)
	, vc_OrganDeadBiomass(cps.speciesParams.pc_NumberOfOrgans(), 0.0)
	, vc_OrganGreenBiomass(cps.speciesParams.pc_NumberOfOrgans(), 0.0)
	, vc_OrganGrowthIncrement(pc_NumberOfOrgans, 0.0)
	, pc_OrganGrowthRespiration(cps.speciesParams.pc_OrganGrowthRespiration)
	, pc_OrganIdsForPrimaryYield(cps.cultivarParams.pc_OrganIdsForPrimaryYield)
	, pc_OrganIdsForSecondaryYield(cps.cultivarParams.pc_OrganIdsForSecondaryYield)
	, pc_OrganIdsForCutting(cps.cultivarParams.pc_OrganIdsForCutting)
	, pc_OrganMaintenanceRespiration(cps.speciesParams.pc_OrganMaintenanceRespiration)
	, vc_OrganSenescenceIncrement(pc_NumberOfOrgans, 0.0)
	, pc_OrganSenescenceRate(cps.cultivarParams.pc_OrganSenescenceRate)
	, pc_PartBiologicalNFixation(cps.speciesParams.pc_PartBiologicalNFixation)
	, pc_Perennial(cps.cultivarParams.pc_Perennial)
	, pc_PlantDensity(cps.speciesParams.pc_PlantDensity)
	, pc_ResidueNRatio(cps.cultivarParams.pc_ResidueNRatio)
	, pc_RespiratoryStress(cps.cultivarParams.pc_RespiratoryStress)
	, vc_RootDensity(soilColumn.vs_NumberOfLayers(), 0.0)
	, vc_RootDiameter(soilColumn.vs_NumberOfLayers(), 0.0)
	, pc_RootDistributionParam(cps.speciesParams.pc_RootDistributionParam)
	, vc_RootEffectivity(soilColumn.vs_NumberOfLayers(), 0.0)
	, pc_RootFormFactor(cps.speciesParams.pc_RootFormFactor)
	, pc_RootGrowthLag(cps.speciesParams.pc_RootGrowthLag)
	, pc_RootPenetrationRate(cps.speciesParams.pc_RootPenetrationRate)
	, vs_SoilMineralNContent(soilColumn.vs_NumberOfLayers(), 0.0)
	, pc_SpecificLeafArea(cps.cultivarParams.pc_SpecificLeafArea)
	, pc_SpecificRootLength(cps.speciesParams.pc_SpecificRootLength)
	, pc_StageAfterCut(cps.speciesParams.pc_StageAfterCut)
	, pc_StageAtMaxDiameter(cps.speciesParams.pc_StageAtMaxDiameter)
	, pc_StageAtMaxHeight(cps.speciesParams.pc_StageAtMaxHeight)
	, pc_StageMaxRootNConcentration(cps.speciesParams.pc_StageMaxRootNConcentration)
	, pc_StageKcFactor(cps.cultivarParams.pc_StageKcFactor)
	, pc_StageTemperatureSum(cps.cultivarParams.pc_StageTemperatureSum)
	, pc_StorageOrgan(cps.speciesParams.pc_StorageOrgan)
	, vs_Tortuosity(cropPs.pc_Tortuosity)
	, vc_Transpiration(soilColumn.vs_NumberOfLayers(), 0.0)
	, vc_TranspirationRedux(soilColumn.vs_NumberOfLayers(), 1.0)
	, pc_VernalisationRequirement(cps.cultivarParams.pc_VernalisationRequirement)
	, pc_WaterDeficitResponseOn(simPs.pc_WaterDeficitResponseOn)
	, eva2_usage(usage)
	, vs_MaxEffectiveRootingDepth(stps.vs_MaxEffectiveRootingDepth)
{
	// Determining the total temperature sum of all developmental stages after
	// emergence (that's why i_Stage starts with 1) until before senescence
	for(int i_Stage = 1; i_Stage < pc_NumberOfDevelopmentalStages - 1; i_Stage++)
		vc_TotalTemperatureSum += pc_StageTemperatureSum[i_Stage];

	vc_FinalDevelopmentalStage = pc_NumberOfDevelopmentalStages - 1;

	// Determining the initial crop organ's biomass
	for(int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++)
	{
		vc_OrganBiomass[i_Organ] = pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

		if(pc_AbovegroundOrgan[i_Organ])
			vc_AbovegroundBiomass += pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

		vc_TotalBiomass += pc_InitialOrganBiomass[i_Organ]; // [kg ha-1]

		// Define storage organ
		if(pc_StorageOrgan[i_Organ])
			vc_StorageOrgan = i_Organ;
	}

	vc_RootBiomass = pc_InitialOrganBiomass[0]; // [kg ha-1]

	// Initialisisng the leaf area index
	vc_LeafAreaIndex = vc_OrganBiomass[1] * pc_SpecificLeafArea[vc_DevelopmentalStage]; // [ha ha-1]

	if(vc_LeafAreaIndex <= 0.0)
		vc_LeafAreaIndex = 0.001;

	// Initialising the root
	vc_RootBiomass = vc_OrganBiomass[0];

	/** @todo Christian: Umrechnung korrekt wenn Biomasse in [kg m-2]? */
	vc_TotalRootLength = (vc_RootBiomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * PI);

	vc_TotalBiomassNContent = (vc_AbovegroundBiomass * pc_NConcentrationAbovegroundBiomass)
		+ (vc_RootBiomass * pc_NConcentrationRoot);
	vc_NConcentrationAbovegroundBiomass = pc_NConcentrationAbovegroundBiomass;
	vc_NConcentrationRoot = pc_NConcentrationRoot;

	// Initialising the initial maximum rooting depth
	double vc_SandContent = soilColumn[0].vs_SoilSandContent(); // [kg kg-1]
	double vc_BulkDensity = soilColumn[0].vs_SoilBulkDensity(); // [kg m-3]
	if(vc_SandContent < 0.55)
		vc_SandContent = 0.55;

	vc_SoilSpecificMaxRootingDepth = vs_SoilSpecificMaxRootingDepth > 0.0
		? vs_SoilSpecificMaxRootingDepth
		: vc_SandContent
		* ((1.1 - vc_SandContent) / 0.275)
		* (1.4 / (vc_BulkDensity / 1000.0)
			 + (vc_BulkDensity * vc_BulkDensity / 40000000.0)); // [m]

	vc_MaxRootingDepth = (vc_SoilSpecificMaxRootingDepth + (pc_CropSpecificMaxRootingDepth * 2.0)) / 3.0; //[m]

	// change organs for yield components in case of eva2 simulation
	// if type of usage is defined
	debug() << "EVA2 Nutzungsart " << eva2_usage << "\t" << pc_CropName.c_str() << endl;
	if(eva2_usage == NUTZUNG_GANZPFLANZE)
	{
		debug() << "Ganzpflanze" << endl;
		for(YieldComponent yc : pc_OrganIdsForPrimaryYield)
			eva2_primaryYieldComponents.push_back(yc);
		for(YieldComponent yc : pc_OrganIdsForSecondaryYield)
			eva2_primaryYieldComponents.push_back(yc);
		eva2_secondaryYieldComponents.clear();
	}

	if(eva2_usage == NUTZUNG_GRUENDUENGUNG)
	{
		// if gruenduengung, put all organs that are in primary yield components
			// into secondary yield component, because the secondary yield stays on
			// the farm
		debug() << "Gründüngung" << endl;
		for(YieldComponent yc : pc_OrganIdsForPrimaryYield)
			eva2_secondaryYieldComponents.push_back(yc);
	}

}

/**
 * @brief Calculates a single time step.
 *
 * @param vw_MeanAirTemperature Mean aire temperature according to weather data
 * @param vw_GlobalRadiation Global radiation
 * @param vw_SunshineHours Number of hours the sun has been shining the day (needed for photosynthesis)
 * @param vs_JulianDay Date in julian notation
 * @param vw_MaxAirTemperature Maximal air temperature for the calculated day
 * @param vw_MinAirTemperature, Minimal air temperature for the calculated day
 * @param vw_RelativeHumidity Relative humidity
 * @param vw_WindSpeed Spped of wind
 * @param vw_WindSpeedHeight Height in which the wind speed has been measured
 * @param vw_AtmosphericCO2Concentration CO2 concentration in the athmosphere (needed for photosynthesis)
 * @param vw_GrossPrecipitation Precipitation
 *
 * @author Claas Nendel
 */
void CropGrowth::step(double vw_MeanAirTemperature,
											double vw_MaxAirTemperature,
											double vw_MinAirTemperature,
											double vw_GlobalRadiation,
											double vw_SunshineHours,
											int vs_JulianDay,
											double vw_RelativeHumidity,
											double vw_WindSpeed,
											double vw_WindSpeedHeight,
											double vw_AtmosphericCO2Concentration,
											double vw_GrossPrecipitation)
{
	if(vc_CuttingDelayDays > 0)
	{
		vc_CuttingDelayDays--;
	}
	//  cout << "Cropstep: " << vw_MinAirTemperature << "\t" << vw_MaxAirTemperature << "\t" << vw_MeanAirTemperature << endl;
	fc_Radiation(vs_JulianDay, vs_Latitude, vw_GlobalRadiation, vw_SunshineHours);

	vc_OxygenDeficit = fc_OxygenDeficiency(pc_CriticalOxygenContent[vc_DevelopmentalStage]);

	int old_DevelopmentalStage = vc_DevelopmentalStage;

	fc_CropDevelopmentalStage(vw_MeanAirTemperature,
														pc_BaseTemperature,
														pc_OptimumTemperature,
														pc_StageTemperatureSum,
														pc_Perennial,
														vc_GrowthCycleEnded,
														vc_TimeStep,
														soilColumn[0].get_Vs_SoilMoisture_m3(),
														soilColumn[0].vs_FieldCapacity(),
														soilColumn[0].vs_PermanentWiltingPoint(),
														pc_NumberOfDevelopmentalStages,
														vc_VernalisationFactor,
														vc_DaylengthFactor,
														vc_CropNRedux);

	if(isAnthesisDay(old_DevelopmentalStage, vc_DevelopmentalStage))
		vc_AnthesisDay = int(vs_JulianDay);

	if(isMaturityDay(old_DevelopmentalStage, vc_DevelopmentalStage))
	{
		vc_MaturityDay = int(vs_JulianDay);
		vc_MaturityReached = true;
	}

	vc_DaylengthFactor =
		fc_DaylengthFactor(pc_DaylengthRequirement[vc_DevelopmentalStage],
											 vc_EffectiveDayLength,
											 vc_PhotoperiodicDaylength,
											 pc_BaseDaylength[vc_DevelopmentalStage]);

	pair<double, double> fc_VernalisationResult =
		fc_VernalisationFactor(vw_MeanAirTemperature,
													 vc_TimeStep,
													 pc_VernalisationRequirement[vc_DevelopmentalStage],
													 vc_VernalisationDays);

	vc_VernalisationFactor = fc_VernalisationResult.first;
	vc_VernalisationDays = fc_VernalisationResult.second;

	if(vc_TotalTemperatureSum == 0.0)
	{
		vc_RelativeTotalDevelopment = 0.0;
	}
	else
	{
		vc_RelativeTotalDevelopment = vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum;
	}

	if(vc_DevelopmentalStage == 0)
	{
		vc_KcFactor = 0.4; /** @todo Claas: muss hier etwas Genaueres hin, siehe FAO? */
	}
	else
	{
		vc_KcFactor = fc_KcFactor(vc_DevelopmentalStage,
															pc_StageTemperatureSum[vc_DevelopmentalStage],
															vc_CurrentTemperatureSum[vc_DevelopmentalStage],
															pc_InitialKcFactor,
															pc_StageKcFactor[vc_DevelopmentalStage],
															pc_StageKcFactor[vc_DevelopmentalStage - 1]);
	}

	if(vc_DevelopmentalStage > 0)
	{

		fc_CropSize(pc_MaxCropHeight,
								pc_MaxCropDiameter,
								pc_StageAtMaxHeight,
								pc_StageAtMaxDiameter,
								pc_StageTemperatureSum,
								vc_CurrentTotalTemperatureSum,
								pc_CropHeightP1,
								pc_CropHeightP2);

		fc_CropGreenArea(vc_OrganGrowthIncrement[1],
										 vc_OrganSenescenceIncrement[1],
										 vc_CropHeight,
										 vc_CropDiameter,
										 pc_SpecificLeafArea[vc_DevelopmentalStage - 1],
										 pc_SpecificLeafArea[vc_DevelopmentalStage],
										 pc_SpecificLeafArea[1],
										 pc_StageTemperatureSum[vc_DevelopmentalStage],
										 vc_CurrentTemperatureSum[vc_DevelopmentalStage],
										 pc_PlantDensity,
										 vc_TimeStep);

		vc_SoilCoverage = fc_SoilCoverage(vc_LeafAreaIndex);


		fc_CropPhotosynthesis(vw_MeanAirTemperature,
													vw_MaxAirTemperature,
													vw_MinAirTemperature,
													vc_GlobalRadiation,
													vw_AtmosphericCO2Concentration,
													vs_Latitude,
													vc_LeafAreaIndex,
													pc_DefaultRadiationUseEfficiency,
													pc_MaxAssimilationRate,
													pc_MinimumTemperatureForAssimilation,
													vc_AstronomicDayLenght,
													vc_Declination,
													vc_ClearDayRadiation,
													vc_EffectiveDayLength,
													vc_OvercastDayRadiation);

		fc_HeatStressImpact(vw_MaxAirTemperature,
												vw_MinAirTemperature,
												vc_CurrentTotalTemperatureSum);

		fc_FrostKill(vw_MaxAirTemperature,
								 vw_MinAirTemperature);

		fc_DroughtImpactOnFertility(vc_TranspirationDeficit);

		fc_CropNitrogen();

		fc_CropDryMatter(vc_DevelopmentalStage,
										 vc_Assimilates,
										 vc_NetMaintenanceRespiration,
										 pc_CropSpecificMaxRootingDepth,
										 vs_SoilSpecificMaxRootingDepth,
										 vw_MeanAirTemperature);

		vc_ReferenceEvapotranspiration = fc_ReferenceEvapotranspiration(vs_HeightNN,
																																		vw_MaxAirTemperature,
																																		vw_MinAirTemperature,
																																		vw_RelativeHumidity,
																																		vw_MeanAirTemperature,
																																		vw_WindSpeed,
																																		vw_WindSpeedHeight,
																																		vc_GlobalRadiation,
																																		vw_AtmosphericCO2Concentration,
																																		vc_GrossPhotosynthesisReference_mol);

		fc_CropWaterUptake(vc_SoilCoverage,
											 vc_RootingZone,
											 soilColumn.vm_GroundwaterTable,
											 vc_ReferenceEvapotranspiration,
											 vw_GrossPrecipitation,
											 vc_CurrentTotalTemperatureSum,
											 vc_TotalTemperatureSum);

		fc_CropNUptake(vc_RootingZone,
									 soilColumn.vm_GroundwaterTable,
									 vc_CurrentTotalTemperatureSum,
									 vc_TotalTemperatureSum);

		vc_GrossPrimaryProduction =
			fc_GrossPrimaryProduction(vc_GrossAssimilates);

		vc_NetPrimaryProduction =
			fc_NetPrimaryProduction(vc_GrossPrimaryProduction,
															vc_TotalRespired);
	}
}

/**
 * @brief Calculation of daylength and radiation
 *
 * Taken from the original HERMES model, Kersebaum, K.C. and Richter J.
 * (1991): Modelling nitrogen dynamics in a plant-soil system with a
 * simple model for advisory purposes. Fert. Res. 27 (2-3), 273 - 281.
 *
 * @param vs_JulianDay
 * @param vs_Latitude
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_Radiation(double vs_JulianDay, double vs_Latitude,
															double vw_GlobalRadiation,
															double vw_SunshineHours)
{

	double vc_DeclinationSinus = 0.0; // old SINLD
	double vc_DeclinationCosinus = 0.0; // old COSLD

	// Calculation of declination - old DEC
	vc_Declination = -23.4 * cos(2.0 * PI * ((vs_JulianDay + 10.0) / 365.0));

	vc_DeclinationSinus = sin(vc_Declination * PI / 180.0) * sin(vs_Latitude * PI / 180.0);
	vc_DeclinationCosinus = cos(vc_Declination * PI / 180.0) * cos(vs_Latitude * PI / 180.0);

	// Calculation of the atmospheric day lenght - old DL
	vc_AstronomicDayLenght = 12.0 * (PI + 2.0 * asin(vc_DeclinationSinus / vc_DeclinationCosinus)) / PI;

	// Calculation of the effective day length - old DLE

	double EDLHelper = (-sin(8.0 * PI / 180.0) + vc_DeclinationSinus) / vc_DeclinationCosinus;

	if((EDLHelper < -1.0) || (EDLHelper > 1.0))
	{
		vc_EffectiveDayLength = 0.01;
	}
	else
	{
		vc_EffectiveDayLength = 12.0 * (PI + 2.0 * asin(EDLHelper)) / PI;
	}

	// old DLP
	vc_PhotoperiodicDaylength = 12.0 * (PI + 2.0 * asin((-sin(-6.0 * PI / 180.0) + vc_DeclinationSinus)
																											/ vc_DeclinationCosinus)) / PI;

	// Calculation of the mean photosynthetically active radiation [J m-2] - old RDN
	vc_PhotActRadiationMean = 3600.0 * (vc_DeclinationSinus * vc_AstronomicDayLenght + 24.0 / PI * vc_DeclinationCosinus
																			* sqrt(1.0 - ((vc_DeclinationSinus / vc_DeclinationCosinus) * (vc_DeclinationSinus / vc_DeclinationCosinus))));

	// Calculation of radiation on a clear day [J m-2] - old DRC
	vc_ClearDayRadiation = 0.5 * 1300.0 * vc_PhotActRadiationMean * exp(-0.14 / (vc_PhotActRadiationMean
																																							 / (vc_AstronomicDayLenght * 3600.0)));

	// Calculation of radiation on an overcast day [J m-2] - old DRO
	vc_OvercastDayRadiation = 0.2 * vc_ClearDayRadiation;

	// Calculation of extraterrestrial radiation - old EXT
	double pc_SolarConstant = 0.082; //[MJ m-2 d-1] Note: Here is the difference to HERMES, which calculates in [J cm-2 d-1]!
	double SC = 24.0 * 60.0 / PI * pc_SolarConstant *(1.0 + 0.033 * cos(2.0 * PI * vs_JulianDay / 365.0));
	double vc_SunsetSolarAngle = acos(-tan(vs_Latitude * PI / 180.0) * tan(vc_Declination * PI / 180.0));
	vc_ExtraterrestrialRadiation = SC * (vc_SunsetSolarAngle * vc_DeclinationSinus + vc_DeclinationCosinus * sin(vc_SunsetSolarAngle)); // [MJ m-2]

	if(vw_GlobalRadiation > 0.0)
		vc_GlobalRadiation = vw_GlobalRadiation;
	else
		vc_GlobalRadiation = vc_ExtraterrestrialRadiation *
		(0.19 + 0.55 * vw_SunshineHours / vc_AstronomicDayLenght);
}

/**
 * @brief Calculation of day length factor
 * @param d_DaylengthRequirement
 * @param vc_PhotoperiodicDaylength
 * @param d_BaseDaylength
 *
 * @return Day length factor
 *
 * @author Claas Nendel
 */
double CropGrowth::fc_DaylengthFactor(double d_DaylengthRequirement, double vc_EffectiveDayLength,
																			double vc_PhotoperiodicDayLength, double d_BaseDaylength)
{
	if(d_DaylengthRequirement > 0.0)
	{

		// ************ Long-day plants **************
		// * Development acceleration by day length. *
		// *  (Day lenght requirement is positive.)  *
		// *******************************************

		vc_DaylengthFactor = (vc_PhotoperiodicDaylength - d_BaseDaylength)
			/ (d_DaylengthRequirement - d_BaseDaylength);

	}
	else if(d_DaylengthRequirement < 0.0)
	{

		// ************* Short-day plants **************
		// * Development acceleration by night lenght. *
		// *  (Day lenght requirement is negative and  *
		// *      represents critical day length.)     *
		// *********************************************

		double vc_CriticalDayLenght = -d_DaylengthRequirement;
		double vc_MaximumDayLength = -d_BaseDaylength;
		if(vc_EffectiveDayLength <= vc_CriticalDayLenght)
		{
			vc_DaylengthFactor = 1.0;
		}
		else
		{
			vc_DaylengthFactor = (vc_EffectiveDayLength - vc_MaximumDayLength)
				/ (vc_CriticalDayLenght - vc_MaximumDayLength);
		}

	}
	else
	{
		vc_DaylengthFactor = 1.0;
	}

	if(vc_DaylengthFactor > 1.0)
	{
		vc_DaylengthFactor = 1.0;
	}

	if(vc_DaylengthFactor < 0.0)
	{
		vc_DaylengthFactor = 0.0;
	}

	return vc_DaylengthFactor;
}

/**
 * @brief Calculation of vernalisation factor
 *
 * @param vw_MeanAirTemperature
 * @param vc_TimeStep
 * @param d_VernalisationRequirement
 * @param d_VernalisationDays
 *
 * @return vernalisation factor
 *
 * @author Claas Nendel
 */
pair<double, double> CropGrowth::fc_VernalisationFactor(double vw_MeanAirTemperature,
																												double vc_TimeStep,
																												double d_VernalisationRequirement,
																												double d_VernalisationDays)
{
	double vc_EffectiveVernalisation;

	if(d_VernalisationRequirement == 0.0)
	{
		vc_VernalisationFactor = 1.0;

	}
	else
	{

		if((vw_MeanAirTemperature > -4.0) && (vw_MeanAirTemperature <= 0.0))
		{
			vc_EffectiveVernalisation = (vw_MeanAirTemperature + 4.0) / 4.0;
		}
		else if((vw_MeanAirTemperature > 0.0) && (vw_MeanAirTemperature <= 3.0))
		{

			vc_EffectiveVernalisation = 1.0;
		}
		else if((vw_MeanAirTemperature > 3.0) && (vw_MeanAirTemperature <= 7.0))
		{

			vc_EffectiveVernalisation = 1.0 - (0.2 * (vw_MeanAirTemperature - 3.0) / 4.0);
		}
		else if((vw_MeanAirTemperature > 7.0) && (vw_MeanAirTemperature <= 9.0))
		{

			vc_EffectiveVernalisation = 0.8 - (0.4 * (vw_MeanAirTemperature - 7.0) / 2.0);
		}
		else if((vw_MeanAirTemperature > 9.0) && (vw_MeanAirTemperature <= 18.0))
		{

			vc_EffectiveVernalisation = 0.4 - (0.4 * (vw_MeanAirTemperature - 9.0) / 9.0);
		}
		else if((vw_MeanAirTemperature <= -4.0) || (vw_MeanAirTemperature > 18.0))
		{

			vc_EffectiveVernalisation = 0.0;
		}
		else
		{
			vc_EffectiveVernalisation = 1.0;
		}

		// old VERNTAGE
		d_VernalisationDays += vc_EffectiveVernalisation * vc_TimeStep;

		// old VERSCHWELL
		double vc_VernalisationThreshold = min(d_VernalisationRequirement, 9.0) - 1.0;

		if(vc_VernalisationThreshold >= 1)
		{

			vc_VernalisationFactor = (d_VernalisationDays - vc_VernalisationThreshold) / (d_VernalisationRequirement
																																										- vc_VernalisationThreshold);

			if(vc_VernalisationFactor < 0)
			{
				vc_VernalisationFactor = 0.0;
			}
		}
		else
		{
			vc_VernalisationFactor = 1.0;
		}
	}

	return make_pair(vc_VernalisationFactor, d_VernalisationDays);
}

/*!
 * @brief Calculation of oxygen deficiency
 *
 * @returns Oxygen deficiency factor.
 *
 * @author Claas Nendel
 */
double CropGrowth::fc_OxygenDeficiency(double d_CriticalOxygenContent)
{
	double vc_AirFilledPoreVolume = 0.0;
	double vc_MaxOxygenDeficit = 0.0;

	// Reduktion bei Luftmangel Stauwasser berücksichtigen!!!!
	vc_AirFilledPoreVolume = ((soilColumn[0].vs_Saturation() + soilColumn[1].vs_Saturation()
														 + soilColumn[2].vs_Saturation()) - (soilColumn[0].get_Vs_SoilMoisture_m3() + soilColumn[1].get_Vs_SoilMoisture_m3()
																																 + soilColumn[2].get_Vs_SoilMoisture_m3())) / 3.0;
	if(vc_AirFilledPoreVolume < d_CriticalOxygenContent)
	{
		vc_TimeUnderAnoxia += int(vc_TimeStep);
		if(vc_TimeUnderAnoxia > 4)
			vc_TimeUnderAnoxia = 4;
		if(vc_AirFilledPoreVolume < 0.0)
			vc_AirFilledPoreVolume = 0.0;
		vc_MaxOxygenDeficit = vc_AirFilledPoreVolume / d_CriticalOxygenContent;
		vc_OxygenDeficit = 1.0 - double(vc_TimeUnderAnoxia / 4) * (1.0 - vc_MaxOxygenDeficit);
	}
	else
	{
		vc_TimeUnderAnoxia = 0;
		vc_OxygenDeficit = 1.0;
	}
	if(vc_OxygenDeficit > 1.0)
		vc_OxygenDeficit = 1.0;

	return vc_OxygenDeficit;
}

/**
 * @brief Determining the crop's developmental stage
 *
 * In this module the developmental stage of the crop is calculated from
 * accumulated heat units (degree-days). It includes the germination of the
 * seed in dependence of the moisture status of the top soil and development
 * acceleration if stress occurs. Stress factors can be limited water or N
 * supply. The temperature sum is cumulated specifically for every
 * developmental stage. As soon as the current temperature sum exceeds the
 * defined crop-specific temperature sum for the respective developmental
 * stage the crop will enter the following stage, until the final stage's
 * temperature sum is reached. In order to follow the overal development
 * of the crop its total temperature sum is monitored.
 *
 * @param vw_MeanAirTemperature
 * @param vc_DevelopmentalStage
 * @param d_BaseTemperature
 * @param d_StageTemperatureSum
 * @param d_CurrentTemperatureSum
 * @param vc_TimeStep
 * @param d_SoilMoisture_m3
 * @param d_FieldCapacity
 * @param d_PermanentWiltingPoint
 * @param pc_NumberOfDevelopmentalStages
 * @param vc_VernalisationFactor
 * @param vc_DaylengthFactor
 * @param vc_CropNRedux
 *
 * @return developmental stage, currently accumulated heat units
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_CropDevelopmentalStage(double vw_MeanAirTemperature, std::vector<double> pc_BaseTemperature,
																					 std::vector<double> pc_OptimumTemperature, std::vector<double> pc_StageTemperatureSum,
																					 bool pc_Perennial, bool vc_GrowthCycleEnded, double vc_TimeStep, double d_SoilMoisture_m3, double d_FieldCapacity,
																					 double d_PermanentWiltingPoint, int pc_NumberOfDevelopmentalStages, double vc_VernalisationFactor,
																					 double vc_DaylengthFactor, double vc_CropNRedux)
{
	double vc_CapillaryWater;
	double vc_DevelopmentAccelerationByNitrogenStress = 0.0; // old NPROG
	double vc_DevelopmentAccelerationByWaterStress = 0.0; // old WPROG
	double vc_DevelopmentAccelerationByStress = 0.0; // old DEVPROG
	double vc_SoilTemperature = soilColumn[0].get_Vs_SoilTemperature();
	double vc_StageExcessTemperatureSum = 0.0;

	double old_DevelopmentalStage = vc_DevelopmentalStage;
	if(vc_DevelopmentalStage == 0)
	{
		if(pc_Perennial)
		{ //pc_Perennial == true
			if(vw_MeanAirTemperature > pc_BaseTemperature[vc_DevelopmentalStage])
			{
				if(vw_MeanAirTemperature > pc_OptimumTemperature[vc_DevelopmentalStage])
				{
					vw_MeanAirTemperature = pc_OptimumTemperature[vc_DevelopmentalStage];
				}

				vc_CurrentTemperatureSum[vc_DevelopmentalStage] += (vw_MeanAirTemperature
																														- pc_BaseTemperature[vc_DevelopmentalStage]) * vc_VernalisationFactor * vc_DaylengthFactor
					* vc_TimeStep;

				vc_CurrentTotalTemperatureSum += (vw_MeanAirTemperature - pc_BaseTemperature[vc_DevelopmentalStage])
					* vc_VernalisationFactor * vc_DaylengthFactor * vc_TimeStep;

			}

			if(vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage])
			{
				if(vc_DevelopmentalStage < (pc_NumberOfDevelopmentalStages - 1))
				{
					vc_DevelopmentalStage++;
				}
			}
		}
		else // pc_Perennial == false
		{
			if(vc_SoilTemperature > pc_BaseTemperature[vc_DevelopmentalStage])
			{

				vc_CapillaryWater = d_FieldCapacity - d_PermanentWiltingPoint;

				/** @todo Claas: Schränkt trockener Boden das Aufsummieren der Wärmeeinheiten ein, oder
				 sollte nicht eher nur der Wechsel in das Stadium 1 davon abhängen? --> Christian */

				if(pc_EmergenceMoistureControlOn == true && pc_EmergenceFloodingControlOn == true)
				{

					if(d_SoilMoisture_m3 > ((0.2 * vc_CapillaryWater) + d_PermanentWiltingPoint)
						 && (soilColumn.vs_SurfaceWaterStorage < 0.001))
					{
						// Germination only if soil water content in top layer exceeds
						// 20% of capillary water, but is not beyond field capacity and
						// if no water is stored on the soil surface.

						vc_CurrentTemperatureSum[vc_DevelopmentalStage] += (vc_SoilTemperature
																																- pc_BaseTemperature[vc_DevelopmentalStage]) * vc_TimeStep;

						if(vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage])
						{
							vc_StageExcessTemperatureSum = vc_CurrentTemperatureSum[vc_DevelopmentalStage] -
								pc_StageTemperatureSum[vc_DevelopmentalStage];
							vc_DevelopmentalStage++;
							vc_CurrentTemperatureSum[vc_DevelopmentalStage] += vc_StageExcessTemperatureSum;
							vc_StageExcessTemperatureSum = 0.0;
						}
					}
				}
				else if(pc_EmergenceMoistureControlOn == true && pc_EmergenceFloodingControlOn == false)
				{

					if(d_SoilMoisture_m3 > ((0.2 * vc_CapillaryWater) + d_PermanentWiltingPoint))
					{
						// Germination only if soil water content in top layer exceeds
						// 20% of capillary water, but is not beyond field capacity.

						vc_CurrentTemperatureSum[vc_DevelopmentalStage] += (vc_SoilTemperature
																																- pc_BaseTemperature[vc_DevelopmentalStage]) * vc_TimeStep;

						if(vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage])
						{
							vc_DevelopmentalStage++;

						}
					}
				}
				else if(pc_EmergenceMoistureControlOn == false && pc_EmergenceFloodingControlOn == true)
				{

					if(soilColumn.vs_SurfaceWaterStorage < 0.001)
					{
						// Germination only if no water is stored on the soil surface.

						vc_CurrentTemperatureSum[vc_DevelopmentalStage] += (vc_SoilTemperature
																																- pc_BaseTemperature[vc_DevelopmentalStage]) * vc_TimeStep;

						if(vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage])
						{
							vc_DevelopmentalStage++;

						}
					}
				}
				else
				{
					vc_CurrentTemperatureSum[vc_DevelopmentalStage] += (vc_SoilTemperature
																															- pc_BaseTemperature[vc_DevelopmentalStage]) * vc_TimeStep;

					if(vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage])
					{
						vc_DevelopmentalStage++;
					}
				}
			}
		}
	}
	else if(vc_DevelopmentalStage > 0)
	{

		// Development acceleration by N deficit in crop tissue
		if((pc_DevelopmentAccelerationByNitrogenStress == 1) &&
			(pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][vc_StorageOrgan] > 0.9))
		{

			vc_DevelopmentAccelerationByNitrogenStress = 1.0 + ((1.0 - vc_CropNRedux) * (1.0 - vc_CropNRedux));

		}
		else
		{

			vc_DevelopmentAccelerationByNitrogenStress = 1.0;
		}

		// Development acceleration by water deficit
		if((vc_TranspirationDeficit < pc_DroughtStressThreshold[vc_DevelopmentalStage]) &&
			(pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][vc_StorageOrgan] > 0.9))
		{

			if(vc_OxygenDeficit < 1.0)
			{
				vc_DevelopmentAccelerationByWaterStress = 1.0;
			}
			else
			{
				vc_DevelopmentAccelerationByWaterStress = 1.0 + ((1.0 - vc_TranspirationDeficit)
																												 * (1.0 - vc_TranspirationDeficit));
			}

		}
		else
		{
			vc_DevelopmentAccelerationByWaterStress = 1.0;
		}

		vc_DevelopmentAccelerationByStress = max(vc_DevelopmentAccelerationByNitrogenStress,
																						 vc_DevelopmentAccelerationByWaterStress);

		if(vc_CuttingDelayDays > 0)
		{
			vc_CurrentTemperatureSum[vc_DevelopmentalStage] = 0.0;
		}
		else if(vw_MeanAirTemperature > pc_BaseTemperature[vc_DevelopmentalStage])
		{

			if(vw_MeanAirTemperature > pc_OptimumTemperature[vc_DevelopmentalStage])
			{
				vw_MeanAirTemperature = pc_OptimumTemperature[vc_DevelopmentalStage];
			}

			vc_CurrentTemperatureSum[vc_DevelopmentalStage] += (vw_MeanAirTemperature
																													- pc_BaseTemperature[vc_DevelopmentalStage]) * vc_VernalisationFactor * vc_DaylengthFactor
				* vc_DevelopmentAccelerationByStress * vc_TimeStep;

			vc_CurrentTotalTemperatureSum += (vw_MeanAirTemperature - pc_BaseTemperature[vc_DevelopmentalStage])
				* vc_VernalisationFactor * vc_DaylengthFactor * vc_DevelopmentAccelerationByStress * vc_TimeStep;

		}

		if(vc_CurrentTemperatureSum[vc_DevelopmentalStage] >= pc_StageTemperatureSum[vc_DevelopmentalStage])
		{
			vc_StageExcessTemperatureSum = vc_CurrentTemperatureSum[vc_DevelopmentalStage] -
				pc_StageTemperatureSum[vc_DevelopmentalStage];

			if(vc_DevelopmentalStage < (pc_NumberOfDevelopmentalStages - 1))
			{
				vc_DevelopmentalStage++;
				vc_CurrentTemperatureSum[vc_DevelopmentalStage] += vc_StageExcessTemperatureSum;
				vc_StageExcessTemperatureSum = 0.0;
			}
			else if(vc_DevelopmentalStage == (pc_NumberOfDevelopmentalStages - 1))
			{
				vc_StageExcessTemperatureSum = 0.0;
				if((pc_Perennial) && (vc_GrowthCycleEnded))
				{
					vc_DevelopmentalStage = 0;
					fc_UpdateCropParametersForPerennial();
					for(int i_Stage = 0; i_Stage < pc_NumberOfDevelopmentalStages; i_Stage++)
					{
						vc_CurrentTemperatureSum[i_Stage] = 0.0;
					}
					vc_CurrentTotalTemperatureSum = 0.0;
					vc_GrowthCycleEnded = false;
				}
			}
		}
	}
	else
	{

		vc_ErrorStatus = true;
		vc_ErrorMessage = "irregular developmental stage";
	}



	debug() << "devstage: " << vc_DevelopmentalStage << endl;
}

/**
 * @brief Determining the crop's Kc factor
 *
 * @param vc_DevelopmentalStage
 * @param d_StageTemperatureSum
 * @param d_CurrentTemperatureSum
 * @param pc_InitialKcFactor
 * @param d_StageKcFactor
 * @param d_EarlierStageKcFactor
 *
 * @return
 *
 * @author Claas Nendel
 */
double CropGrowth::fc_KcFactor(int vc_DevelopmentalStage, double d_StageTemperatureSum, double d_CurrentTemperatureSum,
															 double pc_InitialKcFactor, // DB
															 double d_StageKcFactor, // DB
															 double d_EarlierStageKcFactor) // DB
{
	double vc_RelativeDevelopment = 0.0;

	if(d_StageTemperatureSum == 0.0)
	{
		vc_RelativeDevelopment = 0.0;
	}
	else
	{
		vc_RelativeDevelopment = d_CurrentTemperatureSum / d_StageTemperatureSum; // old relint
	}
	if(vc_RelativeDevelopment > 1.0)
	{
		vc_RelativeDevelopment = 1.0;
	}

	if(vc_DevelopmentalStage == 0)
	{
		vc_KcFactor = pc_InitialKcFactor + (d_StageKcFactor - pc_InitialKcFactor) * vc_RelativeDevelopment;
	}
	else
	{

		// Interpolating the Kc Factors
		vc_KcFactor = d_EarlierStageKcFactor + ((d_StageKcFactor - d_EarlierStageKcFactor) * vc_RelativeDevelopment);
	}

	return vc_KcFactor;
}

/**
 * @brief Calculation of the crop's size
 *
 * This method uses the total relative development of the crop in terms of heat
 * units to determine the sigmoidal development of the crop's height.
 *
 * @Todo In a later stage this must be coupled to the stem's biomass in order to reflect water
 * and N stress to crop height.
 * Diameter is assumed to develop lineraly.
 *
 * @param pc_MaxCropHeight
 * @param pc_MaxCropDiameter
 * @param pc_StageAtMaxHeight
 * @param pc_StageAtMaxDiameter
 * @param vc_DevelopmentalStage
 * @param d_CurrentTemperatureSum
 * @param pc_StageTemperatureSum
 * @param vc_CurrentTotalTemperatureSum
 * @param pc_CropHeightP1
 * @param pc_CropHeightP2
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_CropSize(double pc_MaxCropHeight,
														 double pc_MaxCropDiameter,
														 double pc_StageAtMaxHeight,
														 double pc_StageAtMaxDiameter,
														 std::vector <double> pc_StageTemperatureSum,
														 double vc_CurrentTotalTemperatureSum,
														 double pc_CropHeightP1,
														 double pc_CropHeightP2)
{
	double vc_TotalTemperatureSumForHeight = 0.0;
	for(int i_Stage = 1; i_Stage < pc_StageAtMaxHeight + 1; i_Stage++)
	{
		vc_TotalTemperatureSumForHeight += pc_StageTemperatureSum[i_Stage];
	}

	double vc_TotalTemperatureSumForDiameter = 0.0;
	for(int i_Stage = 1; i_Stage < pc_StageAtMaxDiameter + 1; i_Stage++)
	{
		vc_TotalTemperatureSumForDiameter += pc_StageTemperatureSum[i_Stage];
	}

	double vc_RelativeTotalDevelopmentForHeight = vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSumForHeight;
	if(vc_RelativeTotalDevelopmentForHeight > 1.0)
		vc_RelativeTotalDevelopmentForHeight = 1.0;

	double vc_RelativeTotalDevelopmentForDiameter = vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSumForDiameter;
	if(vc_RelativeTotalDevelopmentForDiameter > 1.0)
		vc_RelativeTotalDevelopmentForDiameter = 1.0;

	if(vc_RelativeTotalDevelopmentForHeight > 0.0)
	{
		vc_CropHeight = pc_MaxCropHeight / (1.0 + exp(-pc_CropHeightP1 * (vc_RelativeTotalDevelopmentForHeight
																																			- pc_CropHeightP2)));
	}
	else
	{
		vc_CropHeight = 0.0;
	}

	if(vc_RelativeTotalDevelopmentForDiameter > 0.0)
	{
		vc_CropDiameter = pc_MaxCropDiameter * vc_RelativeTotalDevelopmentForDiameter;
	}
	else
	{
		vc_CropDiameter = 0.0;
	}
}

/**
 * @brief Calculation of the crop's green area
 *
 * @param d_LeafBiomass
 * @param d_ShootBiomass
 * @param vc_CropHeight
 * @param vc_CropDiameter
 * @param d_SpecificLeafAreaStart
 * @param d_SpecificLeafAreaEnd
 * @param d_StageTemperatureSum
 * @param d_CurrentTemperatureSum
 * @param vc_TimeStep
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_CropGreenArea(double d_LeafBiomassIncrement,
																	double d_LeafBiomassDecrement,
																	double vc_CropHeight,
																	double vc_CropDiameter,
																	double d_SpecificLeafAreaStart,
																	double d_SpecificLeafAreaEnd,
																	double d_SpecificLeafAreaEarly,
																	double d_StageTemperatureSum,
																	double d_CurrentTemperatureSum,
																	double pc_PlantDensity,
																	double vc_TimeStep)
{

	vc_LeafAreaIndex += (d_LeafBiomassIncrement * (d_SpecificLeafAreaStart + (d_CurrentTemperatureSum
																																						/ d_StageTemperatureSum * (d_SpecificLeafAreaEnd - d_SpecificLeafAreaStart))) * vc_TimeStep)
		- (d_LeafBiomassDecrement * d_SpecificLeafAreaEarly * vc_TimeStep); // [ha ha-1]

	if(vc_LeafAreaIndex <= 0.0)
	{
		vc_LeafAreaIndex = 0.001;
	}

	vc_GreenAreaIndex = vc_LeafAreaIndex + (vc_CropHeight * PI * vc_CropDiameter * pc_PlantDensity); // [m2 m-2]
}

/**
 * @brief Calculation of soil area covered by the crop
 *
 * This approach to calculate soil coverage from crop leaf area index is a
 * work-around to at least have a feed-back relation of crop biomass to
 * transpiration. It emanates from The HERMES model. At a later stage LAI
 * should be used directly to calculate Transpiration.
 * Note: This appraoch does not consider spaces between row crops. If any
 * row crop is to be calculated, we need to think this over.
 *
 * @param vc_LeafAreaIndex
 *
 * @return part of soil coved by vegetation
 *
 * @author Claas Nendel
 */
double CropGrowth::fc_SoilCoverage(double vc_LeafAreaIndex)
{
	vc_SoilCoverage = 1.0 - (exp(-0.5 * vc_LeafAreaIndex));

	return vc_SoilCoverage;
}

/**
 * @brief Calculation of photosynthesis
 *
 * In this function crop photosynthesis is calculated.
 *
 * @param vw_MeanAirTemperature
 * @param vw_MaxAirTemperature
 * @param vw_MinAirTemperature
 * @param vc_GlobalRadiation
 * @param vw_SunshineHours
 * @param vw_AtmosphericCO2Concentration
 * @param vs_JulianDay
 * @param vs_Latitude
 * @param vc_LeafAreaIndex
 * @param pc_DefaultRadiationUseEfficiency
 * @param pc_MaxAssimilationRate
 * @param pc_MinimumTemperatureForAssimilation
 * @param vc_PhotActRadiationMean
 * @param vc_AstronomicDayLenght
 * @param vc_Declination
 * @param vc_ClearDayRadiation
 * @param vc_EffectiveDayLength
 * @param vc_OvercastDayRadiation
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_CropPhotosynthesis(double vw_MeanAirTemperature,
																			 double vw_MaxAirTemperature,
																			 double vw_MinAirTemperature,
																			 double vc_GlobalRadiation,
																			 double vw_AtmosphericCO2Concentration,
																			 double vs_Latitude,
																			 double vc_LeafAreaIndex,
																			 double pc_DefaultRadiationUseEfficiency,
																			 double pc_MaxAssimilationRate,
																			 double pc_MinimumTemperatureForAssimilation,
																			 double vc_AstronomicDayLenght,
																			 double vc_Declination,
																			 double vc_ClearDayRadiation,
																			 double vc_EffectiveDayLength,
																			 double vc_OvercastDayRadiation)
{
	double vc_CO2CompensationPoint = 0.0; // old COcomp
	double vc_CO2CompensationPointReference = 0.0;
	double vc_RadiationUseEfficiency = 0.0; // old EFF
	double vc_RadiationUseEfficiencyReference = 0.0;
	double KTvmax = 0.0; // old KTvmax
	double KTkc = 0.0; // old KTkc
	double KTko = 0.0; // old KTko
	double vc_AmaxFactor = 0.0; // old fakamax
	double vc_AmaxFactorReference = 0.0;
	double vc_Vcmax = 0.0; // old vcmax
	double vc_VcmaxReference = 0.0;
	double Mkc = 0.0; // old Mkc
	double Mko = 0.0; // old Mko
	double Oi = 0.0; // old Oi
	double Ci = 0.0; // old Ci
	double vc_AssimilationRateReference = 0.0;
	double vc_HoffmannK1 = 0.0; // old KCo1
	double vc_HoffmannC0 = 0.0; // old coco
	double vc_HoffmannKCO2 = 0.0; // old KCO2
	double vc_NetRadiationUseEfficiency = 0.0; // old EFFE;
	double vc_NetRadiationUseEfficiencyReference = 0.0;
	double SSLAE = 0.0; // old SSLAE;
	double X = 0.0; // old X;
	double XReference = 0.0;
	double PHCH1 = 0.0; // old PHCH1;
	double PHCH1Reference = 0.0;
	double Y = 0.0; // old Y;
	double YReference = 0.0;
	double PHCH2 = 0.0; // old PHCH2;
	double PHCH2Reference = 0.0;
	double PHCH = 0.0; // old PHCH;
	double PHCHReference = 0.0;
	double PHC3 = 0.0; // old PHC3;
	double PHC3Reference = 0.0;
	double PHC4 = 0.0; // old PHC4;
	double PHC4Reference = 0.0;
	double PHCL = 0.0; // old PHCL;
	double PHCLReference = 0.0;
	double Z = 0.0; // old Z;
	double PHOH1 = 0.0; // old PHOH1;
	double PHOH = 0.0; // old PHOH;
	double PHO3 = 0.0; // old PHO3;
	double PHO3Reference = 0.0;
	double PHOL = 0.0; // old PHOL;
	double PHOLReference = 0.0;
	double vc_ClearDayCO2AssimilationReference = 0.0;
	double vc_OvercastDayCO2AssimilationReference = 0.0;
	double vc_ClearDayCO2Assimilation = 0.0; // old DGAC;
	double vc_OvercastDayCO2Assimilation = 0.0; // old DGAO;
	//double vc_GrossAssimilates = 0.0;
	double vc_GrossCO2Assimilation = 0.0; // old DTGA;
	double vc_GrossCO2AssimilationReference = 0.0; // used for ET0 calculation
	double vc_OvercastSkyTimeFraction = 0.0; // old FOV;
	double vc_MaintenanceTemperatureDependency = 0.0; // old TEFF
	double vc_MaintenanceRespiration = 0.0; // old MAINTS
	double vc_DroughtStressThreshold = 0.0; // old VSWELL;
	double vc_PhotoTemperature = 0.0;
	double vc_NightTemperature = 0.0;
	double vc_PhotoMaintenanceRespiration = 0.0;
	double vc_DarkMaintenanceRespiration = 0.0;
	double vc_PhotoGrowthRespiration = 0.0;
	double vc_DarkGrowthRespiration = 0.0;

	const UserCropParameters& user_crops = cropPs;
	double pc_ReferenceLeafAreaIndex = user_crops.pc_ReferenceLeafAreaIndex;
	double pc_ReferenceMaxAssimilationRate = user_crops.pc_ReferenceMaxAssimilationRate;
	double pc_MaintenanceRespirationParameter_1 = user_crops.pc_MaintenanceRespirationParameter1;
	double pc_MaintenanceRespirationParameter_2 = user_crops.pc_MaintenanceRespirationParameter2;

	double pc_GrowthRespirationParameter_1 = user_crops.pc_GrowthRespirationParameter1;
	double pc_GrowthRespirationParameter_2 = user_crops.pc_GrowthRespirationParameter2;
	double pc_CanopyReflectionCoeff = user_crops.pc_CanopyReflectionCoefficient; // old REFLC;

//  std::cout << setprecision(15) << "pc_ReferenceLeafAreaIndex: " << pc_ReferenceLeafAreaIndex << std::endl;
//  std::cout << setprecision(15) << "pc_ReferenceMaxAssimilationRate: " << pc_ReferenceMaxAssimilationRate << std::endl;
//  std::cout << setprecision(15) << "pc_MaintenanceRespirationParameter_1: " << pc_MaintenanceRespirationParameter_1 << std::endl;
//  std::cout << setprecision(15) << "pc_MaintenanceRespirationParameter_2: " << pc_MaintenanceRespirationParameter_2 << std::endl;
//  std::cout << setprecision(15) << "pc_GrowthRespirationParameter_1: " << pc_GrowthRespirationParameter_1 << std::endl;
//  std::cout << setprecision(15) << "pc_GrowthRespirationParameter_2: " << pc_GrowthRespirationParameter_2 << std::endl;
//  std::cout << setprecision(15) << "pc_CanopyReflectionCoeff: " << pc_CanopyReflectionCoeff << std::endl;

	vc_RadiationUseEfficiency = pc_DefaultRadiationUseEfficiency;
	vc_RadiationUseEfficiencyReference = pc_DefaultRadiationUseEfficiency;

	if(pc_CarboxylationPathway == 1)
	{

		// Calculation of CO2 impact on crop growth
		if(pc_CO2Method == 3)
		{

			//////////////////////////////////////////////////////////////////////////
			// Method 3:
			// Long, S.P. 1991. Modification of the response of photosynthetic
			// productivity to rising temperature by atmospheric CO2
			// concentrations - Has its importance been underestimated. Plant
			// Cell Environ. 14(8): 729-739.
			// and
			// Mitchell, R.A.C., D.W. Lawlor, V.J. Mitchell, C.L. Gibbard, E.M.
			// White, and J.R. Porter. 1995. Effects of elevated CO2
			// concentration and increased temperature on winter-wheat - Test
			// of ARCWHEAT1 simulation model. Plant Cell Environ. 18(7):736-748.
			//////////////////////////////////////////////////////////////////////////

			KTvmax = exp(68800.0 * ((vw_MeanAirTemperature + 273.0) - 298.0)
									 / (298.0 * (vw_MeanAirTemperature + 273.0) * 8.314)) * pow(((vw_MeanAirTemperature + 273.0) / 298.0), 0.5);

			KTkc = exp(65800.0 * ((vw_MeanAirTemperature + 273.0) - 298.0) / (298.0 * (vw_MeanAirTemperature + 273.0) * 8.314))
				* pow(((vw_MeanAirTemperature + 273.0) / 298.0), 0.5);

			KTko = exp(1400.0 * ((vw_MeanAirTemperature + 273.0) - 298.0) / (298.0 * (vw_MeanAirTemperature + 273.0) * 8.314))
				* pow(((vw_MeanAirTemperature + 273.0) / 298.0), 0.5);

			// Berechnung des Transformationsfaktors fr pflanzenspez. AMAX bei 25 grad
			vc_AmaxFactor = pc_MaxAssimilationRate / 34.668;
			vc_AmaxFactorReference = pc_ReferenceMaxAssimilationRate / 34.668;
			vc_Vcmax = 98.0 * vc_AmaxFactor * KTvmax;
			vc_VcmaxReference = 98.0 * vc_AmaxFactorReference * KTvmax;

			Mkc = 460.0 * KTkc; //[µmol mol-1]
			Mko = 330.0 * KTko; //[mmol mol-1]

			Oi = 210.0 + (0.047 - 0.0013087 * vw_MeanAirTemperature + 0.000025603 * (vw_MeanAirTemperature
																																							 * vw_MeanAirTemperature) - 0.00000021441 * (vw_MeanAirTemperature * vw_MeanAirTemperature
																																																													 * vw_MeanAirTemperature)) / 0.026934;// [mmol mol-1]

			Ci = vw_AtmosphericCO2Concentration * 0.7 * (1.674 - 0.061294 * vw_MeanAirTemperature + 0.0011688
																									 * (vw_MeanAirTemperature * vw_MeanAirTemperature) - 0.0000088741 * (vw_MeanAirTemperature
																																																											 * vw_MeanAirTemperature * vw_MeanAirTemperature)) / 0.73547;// [µmol mol-1]

			vc_CO2CompensationPoint = 0.5 * 0.21 * vc_Vcmax * Mkc * Oi / (vc_Vcmax * Mko); // [µmol mol-1] 
			vc_CO2CompensationPointReference = 0.5 * 0.21 * vc_VcmaxReference * Mkc * Oi / (vc_VcmaxReference * Mko); // [µmol mol-1]

			// Mitchell et al. 1995:
			vc_RadiationUseEfficiency = min((0.77 / 2.1 * (Ci - vc_CO2CompensationPoint) / (4.5 * Ci + 10.5
																																											* vc_CO2CompensationPoint) * 8.3769), 0.5);
			vc_RadiationUseEfficiencyReference = min((0.77 / 2.1 * (Ci - vc_CO2CompensationPointReference) / (4.5 * Ci + 10.5
																																																				* vc_CO2CompensationPointReference) * 8.3769), 0.5);

			vc_AssimilationRate = (Ci - vc_CO2CompensationPoint) * vc_Vcmax / (Ci + Mkc * (1.0 + Oi / Mko)) * 1.656;
			vc_AssimilationRateReference = (Ci - vc_CO2CompensationPointReference) * vc_VcmaxReference / (Ci + Mkc * (1.0
																																																								+ Oi / Mko)) * 1.656;

			if(vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation)
			{
				vc_AssimilationRate = 0.0;
				vc_AssimilationRateReference = 0.0;
			}

		}
		else if(pc_CO2Method == 2)
		{

			//////////////////////////////////////////////////////////////////////////
			// Method 2:
			// Hoffmann, F. 1995. Fagus, a model for growth and development of
			// beech. Ecol. Mod. 83 (3):327-348.
			//////////////////////////////////////////////////////////////////////////

			if(vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation)
			{
				vc_AssimilationRate = 0.0;
				vc_AssimilationRateReference = 0.0;
			}
			else if(vw_MeanAirTemperature < 10.0)
			{
				vc_AssimilationRate = pc_MaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.4;
				vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.4;
			}
			else if(vw_MeanAirTemperature < 15.0)
			{
				vc_AssimilationRate = pc_MaxAssimilationRate * (0.4 + (vw_MeanAirTemperature - 10.0) / 5.0 * 0.5);
				vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.4 + (vw_MeanAirTemperature - 10.0) / 5.0
																																					* 0.5);
			}
			else if(vw_MeanAirTemperature < 25.0)
			{
				vc_AssimilationRate = pc_MaxAssimilationRate * (0.9 + (vw_MeanAirTemperature - 15.0) / 10.0 * 0.1);
				vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.9 + (vw_MeanAirTemperature - 15.0) / 10.0
																																					* 0.1);
			}
			else if(vw_MeanAirTemperature < 35.0)
			{
				vc_AssimilationRate = pc_MaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 25.0) / 10.0);
				vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 25.0) / 10.0);
			}
			else
			{
				vc_AssimilationRate = 0.0;
				vc_AssimilationRateReference = 0.0;
			}


			/** @FOR_PARAM */
			vc_HoffmannK1 = 220.0 + 0.158 * (vc_GlobalRadiation * 86400.0 / 1000000.0);

			// PAR [MJ m-2], Hoffmann's model requires [W m-2] ->
			// conversion of [MJ m-2] to [W m-2]

			vc_HoffmannC0 = 80.0 - 0.036 * (vc_GlobalRadiation * 86400.0 / 1000000.0);


			vc_HoffmannKCO2 = ((vw_AtmosphericCO2Concentration - vc_HoffmannC0) / (vc_HoffmannK1
																																						 + vw_AtmosphericCO2Concentration - vc_HoffmannC0)) / ((350.0 - vc_HoffmannC0) / (vc_HoffmannK1 + 350.0
																																																																															- vc_HoffmannC0));

			vc_AssimilationRate = vc_AssimilationRate * vc_HoffmannKCO2;
			vc_AssimilationRateReference = vc_AssimilationRateReference * vc_HoffmannKCO2;
		}

	}
	else
	{ //if pc_CarboxylationPathway = 2
		if(vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation)
		{
			vc_AssimilationRate = 0;
			vc_AssimilationRateReference = 0.0;

			// Sage & Kubien (2007): The temperature response of C3 and C4 phtotsynthesis.
			// Plant, Cell and Environment 30, 1086 - 1106.

		}
		else if(vw_MeanAirTemperature < 9.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.08;
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.08;
		}
		else if(vw_MeanAirTemperature < 14.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * (0.071 + (vw_MeanAirTemperature - 9.0) * 0.03);
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.071 + (vw_MeanAirTemperature - 9.0) * 0.03);
		}
		else if(vw_MeanAirTemperature < 20.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * (0.221 + (vw_MeanAirTemperature - 14.0) * 0.09);
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.221 + (vw_MeanAirTemperature - 14.0) * 0.09);
		}
		else if(vw_MeanAirTemperature < 24.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * (0.761 + (vw_MeanAirTemperature - 20.0) * 0.04);
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.761 + (vw_MeanAirTemperature - 20.0) * 0.04);
		}
		else if(vw_MeanAirTemperature < 32.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * (0.921 + (vw_MeanAirTemperature - 24.0) * 0.01);
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.921 + (vw_MeanAirTemperature - 24.0) * 0.01);
		}
		else if(vw_MeanAirTemperature < 38.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate;
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate;
		}
		else if(vw_MeanAirTemperature < 42.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 38.0) * 0.01);
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 38.0) * 0.01);
		}
		else if(vw_MeanAirTemperature < 45.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * (0.96 - (vw_MeanAirTemperature - 42.0) * 0.04);
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.96 - (vw_MeanAirTemperature - 42.0) * 0.04);
		}
		else if(vw_MeanAirTemperature < 54.0)
		{
			vc_AssimilationRate = pc_MaxAssimilationRate * (0.84 - (vw_MeanAirTemperature - 45.0) * 0.09);
			vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.84 - (vw_MeanAirTemperature - 45.0) * 0.09);
		}
		else
		{
			vc_AssimilationRate = 0;
			vc_AssimilationRateReference = 0;

			//      // HERMES
			//    } else if (vw_MeanAirTemperature < 9.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.0555;
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * vw_MeanAirTemperature / 10.0 * 0.0555;
			//    } else if (vw_MeanAirTemperature < 16.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate * (0.05 + (vw_MeanAirTemperature - 9.0) /7.0 * 0.75);
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.05 + (vw_MeanAirTemperature - 9.0) /7 * 0.075);
			//    } else if (vw_MeanAirTemperature < 18.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate * (0.8 + (vw_MeanAirTemperature - 16.0) * 0.07);
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.8 + (vw_MeanAirTemperature - 16.0) * 0.07);
			//    } else if (vw_MeanAirTemperature < 20.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate * (0.94 + (vw_MeanAirTemperature - 18.0) * 0.03);
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.94 + (vw_MeanAirTemperature - 18.0) * 0.03);
			//    } else if (vw_MeanAirTemperature < 30.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate;
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate;
			//    } else if (vw_MeanAirTemperature < 36.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 30.0) * 0.0083);
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (1.0 - (vw_MeanAirTemperature - 30.0) * 0.0083);
			//    } else if (vw_MeanAirTemperature < 42.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate * (0.95 - (vw_MeanAirTemperature - 36.0) * 0.0065);
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.95 - (vw_MeanAirTemperature - 36.0) * 0.0065);
			//    } else if (vw_MeanAirTemperature < 55.0) {
			//      vc_AssimilationRate = pc_MaxAssimilationRate * (0.91 - (vw_MeanAirTemperature - 42.0) * 0.07);
			//      vc_AssimilationRateReference = pc_ReferenceMaxAssimilationRate * (0.91 - (vw_MeanAirTemperature - 42.0) * 0.07);
			//    } else {
			//      vc_AssimilationRate = 0;
			//      vc_AssimilationRateReference = 0;
		}
	}

	if(vc_CuttingDelayDays > 0)
	{
		vc_AssimilationRate = 0.0;
		vc_AssimilationRateReference = 0.0;
	}


	if(vc_AssimilationRate < 0.1)
	{
		vc_AssimilationRate = 0.1;
	}
	if(vc_AssimilationRateReference < 0.1)
	{
		vc_AssimilationRateReference = 0.1;
	}

	///////////////////////////////////////////////////////////////////////////
	// Calculation of light interception in the crop
	//
	// Penning De Vries, F.W.T and H.H. van Laar (1982): Simulation of
	// plant growth and crop production. Pudoc, Wageningen, The
	// Netherlands, p. 87-97.
	///////////////////////////////////////////////////////////////////////////

	vc_NetRadiationUseEfficiency = (1.0 - pc_CanopyReflectionCoeff) * vc_RadiationUseEfficiency;
	vc_NetRadiationUseEfficiencyReference = (1.0 - pc_CanopyReflectionCoeff) * vc_RadiationUseEfficiencyReference;

	SSLAE = sin((90.0 + vc_Declination - vs_Latitude) * PI / 180.0); // = HERMES

	X = log(1.0 + 0.45 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0) * vc_NetRadiationUseEfficiency / (SSLAE
																																																								 * vc_AssimilationRate)); // = HERMES
	XReference = log(1.0 + 0.45 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0)
									 * vc_NetRadiationUseEfficiencyReference / (SSLAE * vc_AssimilationRateReference));

	PHCH1 = SSLAE * vc_AssimilationRate * vc_EffectiveDayLength * X / (1.0 + X); // = HERMES
	PHCH1Reference = SSLAE * vc_AssimilationRateReference * vc_EffectiveDayLength * XReference / (1.0 + XReference);

	Y = log(1.0 + 0.55 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0) * vc_NetRadiationUseEfficiency / ((5.0
																																																									- SSLAE) * vc_AssimilationRate)); // = HERMES
	YReference = log(1.0 + 0.55 * vc_ClearDayRadiation / (vc_EffectiveDayLength * 3600.0) * vc_NetRadiationUseEfficiency
									 / ((5.0 - SSLAE) * vc_AssimilationRateReference));

	PHCH2 = (5.0 - SSLAE) * vc_AssimilationRate * vc_EffectiveDayLength * Y / (1.0 + Y); // = HERMES
	PHCH2Reference = (5.0 - SSLAE) * vc_AssimilationRateReference * vc_EffectiveDayLength * YReference / (1.0
																																																				+ YReference);

	PHCH = 0.95 * (PHCH1 + PHCH2) + 20.5; // = HERMES
	PHCHReference = 0.95 * (PHCH1Reference + PHCH2Reference) + 20.5;

	PHC3 = PHCH * (1.0 - exp(-0.8 * vc_LeafAreaIndex));
	PHC3Reference = PHCHReference * (1.0 - exp(-0.8 * pc_ReferenceLeafAreaIndex));

	PHC4 = vc_AstronomicDayLenght * vc_LeafAreaIndex * vc_AssimilationRate;
	PHC4Reference = vc_AstronomicDayLenght * pc_ReferenceLeafAreaIndex * vc_AssimilationRateReference;

	if(PHC3 < PHC4)
	{
		PHCL = PHC3 * (1.0 - exp(-PHC4 / PHC3));
	}
	else
	{
		PHCL = PHC4 * (1.0 - exp(-PHC3 / PHC4));
	}

	if(PHC3Reference < PHC4Reference)
	{
		PHCLReference = PHC3Reference * (1.0 - exp(-PHC4Reference / PHC3Reference));
	}
	else
	{
		PHCLReference = PHC4Reference * (1.0 - exp(-PHC3Reference / PHC4Reference));
	}

	Z = vc_OvercastDayRadiation / (vc_EffectiveDayLength * 3600.0) * vc_NetRadiationUseEfficiency / (5.0
																																																	 * vc_AssimilationRate);

	PHOH1 = 5.0 * vc_AssimilationRate * vc_EffectiveDayLength * Z / (1.0 + Z);
	PHOH = 0.9935 * PHOH1 + 1.1;
	PHO3 = PHOH * (1.0 - exp(-0.8 * vc_LeafAreaIndex));
	PHO3Reference = PHOH * (1.0 - exp(-0.8 * pc_ReferenceLeafAreaIndex));

	if(PHO3 < PHC4)
	{
		PHOL = PHO3 * (1.0 - exp(-PHC4 / PHO3));
	}
	else
	{
		PHOL = PHC4 * (1.0 - exp(-PHO3 / PHC4));
	}

	if(PHO3Reference < PHC4Reference)
	{
		PHOLReference = PHO3Reference * (1.0 - exp(-PHC4Reference / PHO3Reference));
	}
	else
	{
		PHOLReference = PHC4Reference * (1.0 - exp(-PHO3Reference / PHC4Reference));
	}

	if(vc_LeafAreaIndex < 5.0)
	{
		vc_ClearDayCO2Assimilation = PHCL; // [J m-2]
		vc_OvercastDayCO2Assimilation = PHOL; // [J m-2]
	}
	else
	{
		vc_ClearDayCO2Assimilation = PHCH; // [J m-2]
		vc_OvercastDayCO2Assimilation = PHOH; // [J m-2]
	}

	vc_ClearDayCO2AssimilationReference = PHCLReference;
	vc_OvercastDayCO2AssimilationReference = PHOLReference;

	// Calculation of time fraction for overcast sky situations by
	// comparing clear day radiation and measured PAR in [J m-2].
	// HERMES uses PAR as 50% of global radiation

	vc_OvercastSkyTimeFraction = (vc_ClearDayRadiation - (1000000.0 * vc_GlobalRadiation * 0.50)) / (0.8
																																																	 * vc_ClearDayRadiation); // [J m-2]

	if(vc_OvercastSkyTimeFraction > 1.0)
	{
		vc_OvercastSkyTimeFraction = 1.0;
	}

	if(vc_OvercastSkyTimeFraction < 0.0)
	{
		vc_OvercastSkyTimeFraction = 0.0;
	}

	// Calculation of gross CO2 assimilation in dependence of cloudiness
	vc_GrossCO2Assimilation = vc_OvercastSkyTimeFraction * vc_OvercastDayCO2Assimilation + (1.0
																																													- vc_OvercastSkyTimeFraction) * vc_ClearDayCO2Assimilation;

	vc_GrossCO2AssimilationReference = vc_OvercastSkyTimeFraction * vc_OvercastDayCO2AssimilationReference + (1.0
																																																						- vc_OvercastSkyTimeFraction) * vc_ClearDayCO2AssimilationReference;

	if(vc_OxygenDeficit < 1.0)
	{

		// vc_OxygenDeficit separates drought stress (ETa/Etp) from saturation stress.
		vc_DroughtStressThreshold = 0.0;
	}
	else
	{
		vc_DroughtStressThreshold = pc_DroughtStressThreshold[vc_DevelopmentalStage];
	}

	// Gross CO2 assimilation is used for reference evapotranspiration calculation.
	// For this purpose it must not be affected by drought stress, as the grass
	// reference is defined as being always well supplied with water. Water stress
	// is acting at a later stage.

	if(vc_TranspirationDeficit < vc_DroughtStressThreshold)
	{
		vc_GrossCO2Assimilation = vc_GrossCO2Assimilation; // *  vc_TranspirationDeficit;
	}

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [kg CH2O ha-1 d-1]
	vc_GrossPhotosynthesis = vc_GrossCO2Assimilation * 30.0 / 44.0;

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1]  to [mol m-2 s-1] or [cm3 cm-2 s-1]
	vc_GrossPhotosynthesis_mol = vc_GrossCO2Assimilation * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0);
	vc_GrossPhotosynthesisReference_mol = vc_GrossCO2AssimilationReference * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0);

	// Converting photosynthesis rate from [kg CO2 ha leaf-1 d-1] to [kg CH2O ha-1  d-1]
	vc_Assimilates = vc_GrossCO2Assimilation * 30.0 / 44.0;

	// reduction value for assimilate amount to simulate field conditions;
	vc_Assimilates *= pc_FieldConditionModifier;

	// reduction value for assimilate amount to simulate frost damage;
	vc_Assimilates *= vc_CropFrostRedux;

	if(vc_TranspirationDeficit < vc_DroughtStressThreshold)
	{
		vc_Assimilates = vc_Assimilates * vc_TranspirationDeficit;

	}

	vc_GrossAssimilates = vc_Assimilates;

	// ########################################################################
	// #                              AGROSIM                                 #
	// ########################################################################

	// AGROSIM night and day temperatures
	vc_PhotoTemperature = vw_MaxAirTemperature - ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);
	vc_NightTemperature = vw_MinAirTemperature + ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);

	double vc_MaintenanceRespirationSum = 0.0;
	// AGOSIM night and day maintenance and growth respiration
	for(int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++)
	{
		vc_MaintenanceRespirationSum += (vc_OrganBiomass[i_Organ] - vc_OrganDeadBiomass[i_Organ])
			* pc_OrganMaintenanceRespiration[i_Organ]; // [kg CH2O ha-1]
	// * vc_ActiveFraction[i_Organ]; wenn nicht schon durch acc dead matter abgedeckt
	}

	double vc_NormalisedDayLength = 2.0 - (vc_PhotoperiodicDaylength / 12.0);

	vc_PhotoMaintenanceRespiration = vc_MaintenanceRespirationSum * pow(2.0, (pc_MaintenanceRespirationParameter_1
																																						* (vc_PhotoTemperature - pc_MaintenanceRespirationParameter_2))) * (2.0 - vc_NormalisedDayLength);// @todo: [g m-2] --> [kg ha-1]

	vc_DarkMaintenanceRespiration = vc_MaintenanceRespirationSum * pow(2.0, (pc_MaintenanceRespirationParameter_1
																																					 * (vc_NightTemperature - pc_MaintenanceRespirationParameter_2))) * vc_NormalisedDayLength; // @todo: [g m-2] --> [kg ha-1]

	vc_MaintenanceRespirationAS = vc_PhotoMaintenanceRespiration + vc_DarkMaintenanceRespiration; // [kg CH2O ha-1]

	vc_Assimilates -= vc_PhotoMaintenanceRespiration + vc_DarkMaintenanceRespiration; // [kg CH2O ha-1]



	double vc_GrowthRespirationSum = 0.0;

	for(int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++)
	{
		vc_GrowthRespirationSum += (vc_OrganBiomass[i_Organ] - vc_OrganDeadBiomass[i_Organ])
			* pc_OrganGrowthRespiration[i_Organ];
	}


	if(vc_Assimilates > 0.0)
	{
		vc_PhotoGrowthRespiration = vc_GrowthRespirationSum * pow(2.0, (pc_GrowthRespirationParameter_1
																																		* (vc_PhotoTemperature - pc_GrowthRespirationParameter_2))) * (2.0 - vc_NormalisedDayLength); // [kg CH2O ha-1]

		if(vc_Assimilates > vc_PhotoGrowthRespiration)
		{
			vc_Assimilates -= vc_PhotoGrowthRespiration;

		}
		else
		{
			vc_PhotoGrowthRespiration = vc_Assimilates; // in this case the plant will be restricted in growth!
			vc_Assimilates = 0.0;
		}
	}




	if(vc_Assimilates > 0.0)
	{
		vc_DarkGrowthRespiration = vc_GrowthRespirationSum * pow(2.0, (pc_GrowthRespirationParameter_1
																																	 * (vc_PhotoTemperature - pc_GrowthRespirationParameter_2))) * vc_NormalisedDayLength; // [kg CH2O ha-1]

		if(vc_Assimilates > vc_DarkGrowthRespiration)
		{

			vc_Assimilates -= vc_DarkGrowthRespiration;
		}
		else
		{
			vc_DarkGrowthRespiration = vc_Assimilates; // in this case the plant will be restricted in growth!
			vc_Assimilates = 0.0;
		}

	}
	vc_GrowthRespirationAS = vc_PhotoGrowthRespiration + vc_DarkGrowthRespiration; // [kg CH2O ha-1]
	vc_TotalRespired = vc_GrossAssimilates - vc_Assimilates; // [kg CH2O ha-1]

	// JV! debug
	//std::cout << setprecision(15) << "vc_GrossAssimilates: " << vc_GrossAssimilates << std::endl;
	//std::cout << setprecision(15) << "vc_Assimilates: " << vc_Assimilates << std::endl;
	//std::cout << setprecision(15) << "vc_TotalRespired: " << vc_TotalRespired << std::endl;

	/** to reactivate HERMES algorithms, needs to be vc_NetPhotosynthesis
	 * used instead of  vc_Assimilates in the subsequent methods */

	 // #########################################################################
	 // HERMES calculation of maintenance respiration in dependence of temperature

	vc_MaintenanceTemperatureDependency = pow(2.0, (0.1 * vw_MeanAirTemperature - 2.5));

	vc_MaintenanceRespiration = 0.0;

	for(int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++)
	{
		vc_MaintenanceRespiration += (vc_OrganBiomass[i_Organ] - vc_OrganDeadBiomass[i_Organ])
			* pc_OrganMaintenanceRespiration[i_Organ];
	}

	if(vc_GrossPhotosynthesis < (vc_MaintenanceRespiration * vc_MaintenanceTemperatureDependency))
	{
		vc_NetMaintenanceRespiration = vc_GrossPhotosynthesis;
	}
	else
	{
		vc_NetMaintenanceRespiration = vc_MaintenanceRespiration * vc_MaintenanceTemperatureDependency;
	}

	if(vw_MeanAirTemperature < pc_MinimumTemperatureForAssimilation)
	{
		vc_GrossPhotosynthesis = vc_NetMaintenanceRespiration;
	}
	// This section is now inactive
	// #########################################################################
}


/**
 * @brief Heat stress impact
 *
 * @param vw_MaxAirTemperature
 * @param vw_MinAirTemperature
 * @param vc_CurrentTotalTemperatureSum
 */
void CropGrowth::fc_HeatStressImpact(double vw_MaxAirTemperature,
																		 double vw_MinAirTemperature,
																		 double vc_CurrentTotalTemperatureSum)
{
	// AGROSIM night and day temperatures
	double vc_PhotoTemperature = vw_MaxAirTemperature - ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);
	double vc_FractionOpenFlowers = 0.0;
	double vc_YesterdaysFractionOpenFlowers = 0.0;

	if((pc_BeginSensitivePhaseHeatStress == 0.0) && (pc_EndSensitivePhaseHeatStress == 0.0))
	{
		vc_TotalCropHeatImpact = 1.0;
	}

	if((vc_CurrentTotalTemperatureSum >= pc_BeginSensitivePhaseHeatStress) &&
		 vc_CurrentTotalTemperatureSum < pc_EndSensitivePhaseHeatStress)
	{

		// Crop heat redux: Challinor et al. (2005): Simulation of the impact of high
		// temperature stress on annual crop yields. Agricultural and Forest
		// Meteorology 135, 180 - 189.

		double vc_CropHeatImpact = 1.0 - ((vc_PhotoTemperature - pc_CriticalTemperatureHeatStress)
																			/ (pc_LimitingTemperatureHeatStress - pc_CriticalTemperatureHeatStress));

		if(vc_CropHeatImpact > 1.0)
			vc_CropHeatImpact = 1.0;

		if(vc_CropHeatImpact < 0.0)
			vc_CropHeatImpact = 0.0;

		// Fraction open flowers from Moriondo et al. (2011): Climate change impact
		// assessment: the role of climate extremes in crop yield simulation. Climatic
		// Change 104 (3-4), 679-701.

		vc_FractionOpenFlowers = 1.0 / (1.0 + ((1.0 / 0.015) - 1.0) * exp(-1.4 * vc_DaysAfterBeginFlowering));
		if(vc_DaysAfterBeginFlowering > 0)
		{
			vc_YesterdaysFractionOpenFlowers = 1.0 / (1.0 + ((1.0 / 0.015) - 1.0) * exp(-1.4 * (vc_DaysAfterBeginFlowering - 1)));
		}
		else
		{
			vc_YesterdaysFractionOpenFlowers = 0.0;
		}
		double vc_DailyFloweringRate = vc_FractionOpenFlowers - vc_YesterdaysFractionOpenFlowers;

		// Total effect: Challinor et al. (2005): Simulation of the impact of high
		// temperature stress on annual crop yields. Agricultural and Forest
		// Meteorology 135, 180 - 189.
		vc_TotalCropHeatImpact += vc_CropHeatImpact * vc_DailyFloweringRate;

		vc_DaysAfterBeginFlowering += 1;

	}

	if(vc_CurrentTotalTemperatureSum >= pc_EndSensitivePhaseHeatStress)
	{
		if(vc_TotalCropHeatImpact < vc_CropHeatRedux)
		{
			vc_CropHeatRedux = vc_TotalCropHeatImpact;
		}
	}

}


/**
* @brief Frost kill
*
* @param vw_MaxAirTemperature
* @param vw_MinAirTemperature
*/

void CropGrowth::fc_FrostKill(double vw_MaxAirTemperature, double
															vw_MinAirTemperature)
{

	// ************************************************************
	// ** Fowler, D.B., B.M. Byrns, K.J. Greer. 2014. Overwinter **
	// **	Low-Temperature Responses of Cereals: Analyses and     **
	// **	Simulation. Crop Sci. 54:2395–2405.                    **
	// ************************************************************


	double vc_LT50old = vc_LT50;
	double vc_NightTemperature = 0.0;
	vc_NightTemperature = vw_MinAirTemperature + ((vw_MaxAirTemperature - vw_MinAirTemperature) / 4.0);

	double vc_CrownTemperature = 0.0;
	if(vc_DevelopmentalStage <= 1)
	{
		vc_CrownTemperature = (3.0 * soilColumn.vt_SoilSurfaceTemperature
													 + 2.0 * soilColumn[0].get_Vs_SoilTemperature()) / 5.0;
	}
	else
	{
		vc_CrownTemperature = vc_NightTemperature * 0.8;
	}

	double vc_FrostHardening = 0.0;
	double vc_ThresholdInductionTemperature = 3.72135 - 0.401124 * pc_LT50cultivar;

	if((vc_VernalisationFactor < 1.0) && (vc_CrownTemperature < vc_ThresholdInductionTemperature))
	{
		vc_FrostHardening = pc_FrostHardening * (vc_ThresholdInductionTemperature - vc_CrownTemperature)
			* (vc_LT50old - pc_LT50cultivar);
	}
	else
	{
		vc_FrostHardening = 0.0;
	}

	double vc_FrostDehardening = 0.0;
	if(((vc_VernalisationFactor < 1.0) && (vc_CrownTemperature >= vc_ThresholdInductionTemperature)) ||
		((vc_VernalisationFactor >= 1.0) && (vc_CrownTemperature >= -4.0)))
	{

		vc_FrostDehardening = pc_FrostDehardening / (1.0 + exp(4.35 - 0.28 * vc_CrownTemperature));

	}
	double vc_LowTemperatureExposure = 0.0;

	if((vc_CrownTemperature < -3.0) && ((vc_LT50old - vc_CrownTemperature)) > -12.0)
	{
		vc_LowTemperatureExposure = (vc_LT50old - vc_CrownTemperature) /
			exp(pc_LowTemperatureExposure * (vc_LT50old - vc_CrownTemperature) - 3.74);
	}
	else
	{

		vc_LowTemperatureExposure = 0.0;

	}

	double vc_RespirationFactor = (exp(0.84 + 0.051 * vc_CrownTemperature) - 2.0) / 1.85;
	double vc_SnowDepthFactor = 0.0;

	if(soilColumn.vm_SnowDepth <= 125.0)
	{
		vc_SnowDepthFactor = soilColumn.vm_SnowDepth / 125.0;
	}
	else
	{
		vc_SnowDepthFactor = 1.0;
	}

	double vc_RespiratoryStress = pc_RespiratoryStress * vc_RespirationFactor * vc_SnowDepthFactor;

	vc_LT50 = vc_LT50old - vc_FrostHardening + vc_FrostDehardening + vc_LowTemperatureExposure + vc_RespiratoryStress;

	if(vc_LT50 > -3.0)
	{

		vc_LT50 = -3.0;

	}

	if(vc_CrownTemperature < vc_LT50)
	{

		vc_CropFrostRedux *= 0.5;

	}

	return;
}

/**
 * @brief Drought impact on crop fertility
 *
 * @param vc_TranspirationDeficit
 */
void CropGrowth::fc_DroughtImpactOnFertility(double vc_TranspirationDeficit)
{
	if(vc_TranspirationDeficit < 0.0) vc_TranspirationDeficit = 0.0;

	// Fertility of the crop is reduced in cases of severe drought during bloom
	if((vc_TranspirationDeficit < (pc_DroughtImpactOnFertilityFactor *
																 pc_DroughtStressThreshold[vc_DevelopmentalStage])) &&
																 (pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][vc_StorageOrgan] > 0.0))
	{

		double vc_TranspirationDeficitHelper = vc_TranspirationDeficit /
			(pc_DroughtImpactOnFertilityFactor * pc_DroughtStressThreshold[vc_DevelopmentalStage]);

		if(vc_OxygenDeficit < 1.0)
		{
			vc_DroughtImpactOnFertility = 1.0;
		}
		else
		{
			vc_DroughtImpactOnFertility = 1.0 - ((1.0 - vc_TranspirationDeficitHelper)
																					 * (1.0 - vc_TranspirationDeficitHelper));
		}

	}
	else
	{
		vc_DroughtImpactOnFertility = 1.0;
	}

}

/**
 * @brief Crop Nitrogen
 *
 * @param vc_CurrentTotalTemperatureSum
 */
void CropGrowth::fc_CropNitrogen()
{
	double vc_RootNRedux = 0.0; // old REDWU
	double vc_RootNReduxHelper = 0.0; // old WUX
	//double vc_MinimumNConcentration   = 0.0; // old MININ
	double vc_CropNReduxHelper = 0.0; // old AUX

	vc_CriticalNConcentration = pc_NConcentrationPN *
		(1.0 + (pc_NConcentrationB0 *
						exp(-0.26 * (vc_AbovegroundBiomass + vc_BelowgroundBiomass) / 1000.0))) / 100.0;
	// [kg ha-1 -> t ha-1]

	vc_TargetNConcentration = vc_CriticalNConcentration * pc_LuxuryNCoeff;

	vc_NConcentrationAbovegroundBiomassOld = vc_NConcentrationAbovegroundBiomass;
	vc_NConcentrationRootOld = vc_NConcentrationRoot;

	if(vc_NConcentrationRoot < 0.01)
	{

		if(vc_NConcentrationRoot <= 0.005)
		{
			vc_RootNRedux = 0.0;
		}
		else
		{

			vc_RootNReduxHelper = (vc_NConcentrationRoot - 0.005) / 0.005;
			vc_RootNRedux = 1.0 - sqrt(1.0 - vc_RootNReduxHelper * vc_RootNReduxHelper);
		}
	}
	else
	{
		vc_RootNRedux = 1.0;
	}

	if(pc_PartBiologicalNFixation <= 0.01)
	{
		if(vc_NConcentrationAbovegroundBiomass < vc_CriticalNConcentration)
		{

			if(vc_NConcentrationAbovegroundBiomass <= pc_MinimumNConcentration)
			{
				vc_CropNRedux = 0.0;
			}
			else
			{

				vc_CropNReduxHelper = (vc_NConcentrationAbovegroundBiomass - pc_MinimumNConcentration)
					/ (vc_CriticalNConcentration - pc_MinimumNConcentration);

				//       // New Monica appraoch
				vc_CropNRedux = 1.0 - exp(pc_MinimumNConcentration - (5.0 * vc_CropNReduxHelper));

				//        // Original HERMES approach
				//        vc_CropNRedux = (1.0 - exp(1.0 + 1.0 / (vc_CropNReduxHelper - 1.0))) *
				//                    (1.0 - exp(1.0 + 1.0 / (vc_CropNReduxHelper - 1.0)));
			}
		}
		else
		{
			vc_CropNRedux = 1.0;
		}
	}
	else
	{
		if(vc_NConcentrationAbovegroundBiomass < vc_CriticalNConcentration)
		{
			vc_FixedN = vc_CriticalNConcentration - vc_NConcentrationAbovegroundBiomass;
			vc_NConcentrationAbovegroundBiomass = vc_CriticalNConcentration;
			vc_CropNRedux = 1.0;
		}
	}

	if(pc_NitrogenResponseOn == false)
	{
		vc_CropNRedux = 1.0;
	}
}

/**
 * @brief  Dry matter allocation within the crop
 *
 *  In this function the result from crop photosynthesis is allocated to the
 *  different crop organs under consideration of any stress factors
 *  (Water, Nitrogen, Temperature)
 *
 * @param vs_NumberOfLayers
 * @param vs_LayerThickness
 * @param pc_CropName
 * @param vc_DevelopmentalStage
 * @param vc_Assimilates
 * @param vc_NetMaintenanceRespiration
 * @param pc_CropSpecificMaxRootingDepth
 * @param vc_CropNRedux
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_CropDryMatter(int vc_DevelopmentalStage,
																	double vc_Assimilates,
																	double /*vc_NetMaintenanceRespiration*/,
																	double /*pc_CropSpecificMaxRootingDepth*/,
																	double /*vs_SoilSpecificMaxRootingDepth*/,
																	double vw_MeanAirTemperature)
{
	size_t nols = soilColumn.vs_NumberOfLayers();
	double layerThickness = soilColumn.vs_LayerThickness();

	double vc_MaxRootNConcentration = 0.0; // old WGM
	double vc_NConcentrationOptimum = 0.0; // old DTOPTN
	double vc_RootNIncrement = 0.0; // old WUMM
	double vc_AssimilatePartitioningCoeffOld = 0.0;
	double vc_AssimilatePartitioningCoeff = 0.0;
	//double vc_RootDistributionFactor                  = 0.0; // old QREZ
	//double vc_SoilDepth                               = 0.0; // old TIEFE
	//std::vector<double> vc_RootLengthToLayer(nols, 0.0);	// old WULAE
	//std::vector<double> vc_RootLengthInLayer(nols, 0.0);	// old WULAE2
	//  std::vector<double> vc_CapillaryWater(nols, 0.0);
	//std::vector<double> vc_RootSurface(nols, 0.0); // old FL


	const UserCropParameters& user_crops = cropPs;
	double pc_MaxCropNDemand = user_crops.pc_MaxCropNDemand;

	//double pc_GrowthRespirationRedux = user_crops->getPc_GrowthRespirationRedux();

	// Assuming that growth respiration takes 30% of total assimilation --> 0.7 [kg ha-1]
	//vc_NetPhotosynthesis = (vc_GrossPhotosynthesis - vc_NetMaintenanceRespiration + vc_ReserveAssimilatePool) * pc_GrowthRespirationRedux; // from HERMES algorithms
	vc_NetPhotosynthesis = vc_Assimilates; // from AGROSIM algorithms
	vc_ReserveAssimilatePool = 0.0;

	vc_AbovegroundBiomassOld = vc_AbovegroundBiomass;
	vc_AbovegroundBiomass = 0.0;
	vc_BelowgroundBiomassOld = vc_BelowgroundBiomass;
	vc_BelowgroundBiomass = 0.0;
	vc_TotalBiomass = 0.0;

	//old PESUM [kg m-2 --> kg ha-1]
	vc_TotalBiomassNContent += (soilColumn.vq_CropNUptake * 10000.0) + vc_FixedN;



	// Dry matter production
	// old NRKOM
	// double assimilate_partition_shoot = 0.7;
	double assimilate_partition_leaf = 0.3;

	for(int i_Organ = 0; i_Organ < pc_NumberOfOrgans; i_Organ++)
	{

		vc_AssimilatePartitioningCoeffOld = pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage - 1][i_Organ];
		vc_AssimilatePartitioningCoeff = pc_AssimilatePartitioningCoeff[vc_DevelopmentalStage][i_Organ];

		//Identify storage organ and reduce assimilate flux in case of heat stress
		if(pc_StorageOrgan[i_Organ])
		{
			vc_AssimilatePartitioningCoeffOld = vc_AssimilatePartitioningCoeffOld * vc_CropHeatRedux * vc_DroughtImpactOnFertility;
			vc_AssimilatePartitioningCoeff = vc_AssimilatePartitioningCoeff * vc_CropHeatRedux * vc_DroughtImpactOnFertility;
		}


		if((vc_CurrentTemperatureSum[vc_DevelopmentalStage] / pc_StageTemperatureSum[vc_DevelopmentalStage]) > 1)
		{

			// Pflanze ist ausgewachsen
			vc_OrganGrowthIncrement[i_Organ] = 0.0;
			vc_OrganSenescenceIncrement[i_Organ] = 0.0;
			if(pc_Perennial)
			{
				vc_GrowthCycleEnded = true;
			}

		}
		else
		{

			// test if there is a positive bilance of produced assimilates
			// if vc_NetPhotosynthesis is negativ, the crop needs more for
			// maintenance than for building new biomass
			if(vc_NetPhotosynthesis < 0.0)
			{

				// reduce biomass from leaf and shoot because of negative assimilate
				//! TODO: hard coded organ ids; must be more generalized because in database organ_ids can be mixed
				vc_OrganBiomass[i_Organ];

				if(i_Organ == LEAF)
				{ // leaf

					double incr = assimilate_partition_leaf * vc_NetPhotosynthesis;
					if(fabs(incr) <= vc_OrganBiomass[i_Organ])
					{
						debug() << "LEAF - Reducing organ biomass - default case (" << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")" << endl;
						vc_OrganGrowthIncrement[i_Organ] = incr;
					}
					else
					{
						// temporary hack because complex algorithm produces questionable results
						debug() << "LEAF - Not enough biomass for reduction - Reducing only what is available " << endl;
						vc_OrganGrowthIncrement[i_Organ] = (-1) * vc_OrganBiomass[i_Organ];


						//                      debug() << "LEAF - Not enough biomass for reduction; Need to calculate new partition coefficient" << endl;
						//                      // calculate new partition coefficient to detect, how much of organ biomass
						//                      // can be reduced
						//                      assimilate_partition_leaf = fabs(vc_OrganBiomass[i_Organ] / vc_NetPhotosynthesis);
						//                      assimilate_partition_shoot = 1.0 - assimilate_partition_leaf;
						//                      debug() << "LEAF - New Partition: " << assimilate_partition_leaf << endl;
						//
						//                      // reduce biomass for leaf
						//                      incr = assimilate_partition_leaf * vc_NetPhotosynthesis; // should be negative, therefor the addition
						//                      vc_OrganGrowthIncrement[i_Organ] = incr;
						//                      debug() << "LEAF - Reducing organ by " << incr << " (" << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")"<< endl;
					}

				}
				else if(i_Organ == SHOOT)
				{ // shoot

					double incr = assimilate_partition_leaf * vc_NetPhotosynthesis; // should be negative

					if(fabs(incr) <= vc_OrganBiomass[i_Organ])
					{
						vc_OrganGrowthIncrement[i_Organ] = incr;
						debug() << "SHOOT - Reducing organ biomass - default case (" << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")" << endl;
					}
					else
					{
						// temporary hack because complex algorithm produces questionable results
						debug() << "SHOOT - Not enough biomass for reduction - Reducing only what is available " << endl;
						vc_OrganGrowthIncrement[i_Organ] = (-1) * vc_OrganBiomass[i_Organ];


						//                      debug() << "SHOOT - Not enough biomass for reduction; Need to calculate new partition coefficient" << endl;
						//
						//                      assimilate_partition_shoot = fabs(vc_OrganBiomass[i_Organ] / vc_NetPhotosynthesis);
						//                      assimilate_partition_leaf = 1.0 - assimilate_partition_shoot;
						//                      debug() << "SHOOT - New Partition: " << assimilate_partition_shoot << endl;
						//
						//                      incr = assimilate_partition_shoot * vc_NetPhotosynthesis;
						//                      vc_OrganGrowthIncrement[i_Organ] = incr;
						//                      debug() << "SHOOT - Reducing organ (" << vc_OrganBiomass[i_Organ] + vc_OrganGrowthIncrement[i_Organ] << ")"<< endl;
						//
						//                      // test if there is the possibility to reduce biomass of leaf
						//                      // for remaining assimilates
						//                      incr = assimilate_partition_leaf * vc_NetPhotosynthesis;
						//                      double available_leaf_biomass = vc_OrganBiomass[LEAF] + vc_OrganGrowthIncrement[LEAF];
						//                      if (incr<available_leaf_biomass) {
						//                          // leaf biomass is big enough, so reduce biomass furthermore
						//                          vc_OrganGrowthIncrement[LEAF] += incr; // should be negative, therefor the addition
						//                          debug() << "LEAF - Reducing leaf biomasse further (" << vc_OrganBiomass[LEAF] + vc_OrganGrowthIncrement[LEAF] << ")"<< endl;
						//                      } else {
						//                          // worst case - there is not enough biomass available to reduce
						//                          // maintenaince respiration requires more assimilates that can be
						//                          // provided by plant itselft
						//                          // now the plant is dying - sorry
						//                          dyingOut = true;
						//                      }

					}

				}
				else
				{
					// root or storage organ - do nothing in case of negative photosynthesis
					vc_OrganGrowthIncrement[i_Organ] = 0.0;
				}

			}
			else
			{ // if (vc_NetPhotosynthesis < 0.0) {
				vc_OrganGrowthIncrement[i_Organ] = vc_NetPhotosynthesis *
					(vc_AssimilatePartitioningCoeffOld
					 + ((vc_AssimilatePartitioningCoeff - vc_AssimilatePartitioningCoeffOld)
							* (vc_CurrentTemperatureSum[vc_DevelopmentalStage]
								 / pc_StageTemperatureSum[vc_DevelopmentalStage]))) * vc_CropNRedux; // [kg CH2O ha-1]
			}
			vc_OrganSenescenceIncrement[i_Organ] = (vc_OrganBiomass[i_Organ] - vc_OrganDeadBiomass[i_Organ]) *
				(pc_OrganSenescenceRate[vc_DevelopmentalStage - 1][i_Organ]
				 + ((pc_OrganSenescenceRate[vc_DevelopmentalStage][i_Organ]
						 - pc_OrganSenescenceRate[vc_DevelopmentalStage - 1][i_Organ])
						* (vc_CurrentTemperatureSum[vc_DevelopmentalStage] / pc_StageTemperatureSum[vc_DevelopmentalStage]))); // [kg CH2O ha-1]

		}

		if(i_Organ != vc_StorageOrgan)
		{
			// Wurzel, Sprossachse, Blatt
			vc_OrganBiomass[i_Organ] += (vc_OrganGrowthIncrement[i_Organ] * vc_TimeStep)
				- (vc_OrganSenescenceIncrement[i_Organ] * vc_TimeStep); // [kg CH2O ha-1]
			vc_OrganBiomass[vc_StorageOrgan] += pc_AssimilateReallocation * vc_OrganSenescenceIncrement[i_Organ]; // [kg CH2O ha-1]
		}
		else
		{
			vc_OrganBiomass[i_Organ] += (vc_OrganGrowthIncrement[i_Organ] * vc_TimeStep)
				- (vc_OrganSenescenceIncrement[i_Organ] * vc_TimeStep); // [kg CH2O ha-1]
		}

		vc_OrganDeadBiomass[i_Organ] += vc_OrganSenescenceIncrement[i_Organ] * vc_TimeStep; // [kg CH2O ha-1]
		vc_OrganGreenBiomass[i_Organ] = vc_OrganBiomass[i_Organ] - vc_OrganDeadBiomass[i_Organ]; // [kg CH2O ha-1]

		if((vc_OrganGreenBiomass[i_Organ]) < 0.0)
		{
			vc_OrganDeadBiomass[i_Organ] = vc_OrganBiomass[i_Organ];
			vc_OrganGreenBiomass[i_Organ] = 0.0;
		}

		if(pc_AbovegroundOrgan[i_Organ])
			vc_AbovegroundBiomass += vc_OrganBiomass[i_Organ]; // [kg CH2O ha-1]
		else if(!pc_AbovegroundOrgan[i_Organ] && i_Organ > 0)
			vc_BelowgroundBiomass += vc_OrganBiomass[i_Organ]; // [kg CH2O ha-1]

		vc_TotalBiomass += vc_OrganBiomass[i_Organ]; // [kg CH2O ha-1]

	}

	/** @todo N redux noch ausgeschaltet */
	vc_ReserveAssimilatePool = 0.0; //+= vc_NetPhotosynthesis * (1.0 - vc_CropNRedux);
	vc_RootBiomassOld = vc_RootBiomass;
	vc_RootBiomass = vc_OrganBiomass[0];

	if(vc_DevelopmentalStage > 0)
	{

		vc_MaxRootNConcentration = pc_StageMaxRootNConcentration[vc_DevelopmentalStage - 1]
			- (pc_StageMaxRootNConcentration[vc_DevelopmentalStage - 1] - pc_StageMaxRootNConcentration[vc_DevelopmentalStage])
			* vc_CurrentTemperatureSum[vc_DevelopmentalStage] / pc_StageTemperatureSum[vc_DevelopmentalStage]; //[kg kg-1]
	}
	else
	{
		vc_MaxRootNConcentration = pc_StageMaxRootNConcentration[vc_DevelopmentalStage];
	}

	vc_CropNDemand = ((vc_TargetNConcentration * vc_AbovegroundBiomass)
										+ (vc_RootBiomass * vc_MaxRootNConcentration)
										+ (vc_TargetNConcentration * vc_BelowgroundBiomass / pc_ResidueNRatio)
										- vc_TotalBiomassNContent) * vc_TimeStep; // [kg ha-1]

	vc_NConcentrationOptimum = ((vc_TargetNConcentration
															 - (vc_TargetNConcentration - vc_CriticalNConcentration) * 0.15) * vc_AbovegroundBiomass
															+ (vc_TargetNConcentration
																 - (vc_TargetNConcentration - vc_CriticalNConcentration) * 0.15) * vc_BelowgroundBiomass / pc_ResidueNRatio
															+ (vc_RootBiomass * vc_MaxRootNConcentration) - vc_TotalBiomassNContent) * vc_TimeStep; // [kg ha-1]



	if(vc_CropNDemand > (pc_MaxCropNDemand * vc_TimeStep))
	{
		// Not more than 6kg N per day to be taken up.
		vc_CropNDemand = pc_MaxCropNDemand * vc_TimeStep;
	}

	if(vc_CropNDemand < 0)
	{
		vc_CropNDemand = 0.0;
	}

	if(vc_RootBiomass < vc_RootBiomassOld)
	{
		/** @todo: Claas: Macht die Bedingung hier Sinn? Hat sich die Wurzel wirklich zurückgebildet? */
		vc_RootNIncrement = (vc_RootBiomassOld - vc_RootBiomass) * vc_NConcentrationRoot;
	}
	else
	{
		vc_RootNIncrement = 0;
	}

	// In case of drought stress the root will grow deeper
	if((vc_TranspirationDeficit < (0.95 * pc_DroughtStressThreshold[vc_DevelopmentalStage])) &&
		(vc_RootingDepth_m > 0.95 * vc_MaxRootingDepth) &&
		 (vc_DevelopmentalStage < (pc_NumberOfDevelopmentalStages - 1)))
	{
		vc_MaxRootingDepth += 0.005;
	}

	if(vc_MaxRootingDepth > (double(nols - 1) * layerThickness))
		vc_MaxRootingDepth = double(nols - 1) * layerThickness;


	// ***************************************************************************
	// *** Taken from Pedersen et al. 2010: Modelling diverse root density     ***
	// *** dynamics and deep nitrogen uptake - a simple approach.              ***
	// *** Plant & Soil 326, 493 - 510                                         ***
	// ***************************************************************************

	// Determining temperature sum for root growth
	double pc_MaximumTemperatureRootGrowth = pc_MinimumTemperatureRootGrowth + 20.0;
	double vc_DailyTemperatureRoot = 0.0;
	if(vw_MeanAirTemperature >= pc_MaximumTemperatureRootGrowth)
	{
		vc_DailyTemperatureRoot = pc_MaximumTemperatureRootGrowth - pc_MinimumTemperatureRootGrowth;
	}
	else
	{
		vc_DailyTemperatureRoot = vw_MeanAirTemperature - pc_MinimumTemperatureRootGrowth;
	}
	if(vc_DailyTemperatureRoot < 0.0)
	{
		vc_DailyTemperatureRoot = 0.0;
	}
	vc_CurrentTotalTemperatureSumRoot += vc_DailyTemperatureRoot;

	// Determining root penetration rate according to soil clay content [m °C-1 d-1]
	double vc_RootPenetrationRate = 0.0; // [m °C-1 d-1]
	if(soilColumn[vc_RootingDepth].vs_SoilClayContent() <= 0.02)
	{
		vc_RootPenetrationRate = 0.5 * pc_RootPenetrationRate;
	}
	else if(soilColumn[vc_RootingDepth].vs_SoilClayContent() <= 0.08)
	{
		vc_RootPenetrationRate = ((1.0 / 3.0) + (0.5 / 0.06 * soilColumn[vc_RootingDepth].vs_SoilClayContent()))
			* pc_RootPenetrationRate; // [m °C-1 d-1]
	}
	else
	{
		vc_RootPenetrationRate = pc_RootPenetrationRate; // [m °C-1 d-1]
	}

	// Calculating rooting depth [m]
	if(vc_CurrentTotalTemperatureSumRoot <= pc_RootGrowthLag)
	{
		vc_RootingDepth_m = pc_InitialRootingDepth; // [m]
	}
	else
	{
		// corrected because oscillating rooting depth at layer boundaries with texture change
	 /* vc_RootingDepth_m = pc_InitialRootingDepth
				+ ((vc_CurrentTotalTemperatureSumRoot - pc_RootGrowthLag)
				* vc_RootPenetrationRate); // [m] */

		vc_RootingDepth_m += (vc_DailyTemperatureRoot * vc_RootPenetrationRate); // [m]

	}

	if(vc_RootingDepth_m <= pc_InitialRootingDepth)
	{
		vc_RootingDepth_m = pc_InitialRootingDepth;
	}

	if(vc_RootingDepth_m > vc_MaxRootingDepth)
	{
		vc_RootingDepth_m = vc_MaxRootingDepth; // [m]
	}


	if(vc_RootingDepth_m > vs_MaxEffectiveRootingDepth)
	{
		vc_RootingDepth_m = vs_MaxEffectiveRootingDepth;
	}

	//std::cout << "vs_MaxEffectiveRootingDepth: " << vs_MaxEffectiveRootingDepth << std::endl;
	//std::cout << "vc_RootPenetrationRate: " << vc_RootPenetrationRate << std::endl;
	//std::cout << "pc_RootPenetrationRate: " << pc_RootPenetrationRate << std::endl;
	//std::cout << "vc_CurrentTotalTemperatureSumRoot: " << vc_CurrentTotalTemperatureSumRoot << std::endl;
	//std::cout << "vc_DailyTemperatureRoot: " << vc_DailyTemperatureRoot << std::endl;
	//std::cout << "pc_RootGrowthLag: " << pc_RootGrowthLag << std::endl;
	//std::cout << "pc_InitialRootingDepth: " << pc_InitialRootingDepth << std::endl;

	// Calculating rooting depth layer []
	vc_RootingDepth = int(floor(0.5 + (vc_RootingDepth_m / layerThickness))); // []
	if(vc_RootingDepth > nols)
	{
		vc_RootingDepth = nols;
	}

	vc_RootingZone = int(floor(0.5 + ((1.3 * vc_RootingDepth_m) / layerThickness))); // []
	if(vc_RootingZone > nols)
	{
		vc_RootingZone = nols;
	}

	vc_TotalRootLength = vc_RootBiomass * pc_SpecificRootLength; //[m m-2]

	//std::cout << "vc_MaxRootingDepth: " << vc_MaxRootingDepth << std::endl;
	//std::cout << "vc_RootingDepth: " << vc_RootingDepth << std::endl;
	//std::cout << "vc_RootingZone: " << vc_RootingZone << std::endl;
	//std::cout << "vc_RootingDepth_m: " << vc_RootingDepth_m << std::endl;
	//std::cout << "vc_TotalRootLength: " << vc_TotalRootLength << std::endl;
	//std::cout << "vs_LayerThickness: " << vs_LayerThickness << std::endl;
	//std::cout << "pc_RootFormFactor: " << pc_RootFormFactor << std::endl;
	//std::cout << "pc_SpecificRootLength: " << pc_SpecificRootLength << std::endl;

	// Calculating a root density distribution factor []
	std::vector<double> vc_RootDensityFactor(nols, 0.0);
	for(size_t i_Layer = 0; i_Layer < nols; i_Layer++)
	{
		if(i_Layer < vc_RootingDepth)
			vc_RootDensityFactor[i_Layer] = exp(-pc_RootFormFactor * (i_Layer * layerThickness)); // []
		else if(i_Layer < vc_RootingZone)
			vc_RootDensityFactor[i_Layer] = exp(-pc_RootFormFactor * (i_Layer * layerThickness))
			* (1.0 - ((i_Layer - vc_RootingDepth) / (vc_RootingZone - vc_RootingDepth))); // []
		else
			vc_RootDensityFactor[i_Layer] = 0.0; // []

		//std::cout << setprecision(11) << "vc_RootDensityFactor[i_Layer]: " << i_Layer << ", " << vc_RootDensityFactor[i_Layer] << std::endl;
	}

	// Summing up all factors to scale to a relative factor between [0;1]
	double vc_RootDensityFactorSum = 0.0;
	for(size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++)
		vc_RootDensityFactorSum += vc_RootDensityFactor[i_Layer]; // []

	// Calculating root density per layer from total root length and
	// a relative root density distribution factor
	for(size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++)
		vc_RootDensity[i_Layer] = (vc_RootDensityFactor[i_Layer] / vc_RootDensityFactorSum)
		* vc_TotalRootLength; // [m m-3]

	for(size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++)
	{
		// Root diameter [m]
		if(!pc_AbovegroundOrgan[3])
			vc_RootDiameter[i_Layer] = 0.0001; //[m]
		else
			vc_RootDiameter[i_Layer] = 0.0002 - ((i_Layer + 1) * 0.00001); // [m]

		// Default root decay - 10 %
		vo_FreshSoilOrganicMatter[i_Layer] += vc_RootNIncrement
			* vc_RootDensity[i_Layer]
			* 10.0
			/ vc_TotalRootLength;

	}
	// *****************************************************************************

		// *** Original HERMES approach: ***
	//  // Taken from Gerwitz & Page --> Parameter for e-function indicating the
	//  // depth above which 68% of the roots are present
	//  if (pc_AbovegroundOrgan[3] == 0) {
	//    vc_RootDistributionFactor = pow((0.081476 + exp(-pc_RootDistributionParam * vc_CurrentTotalTemperatureSum)), 1.8);
	//  } else {
	//    vc_RootDistributionFactor = pow((0.081476 + exp(-pc_RootDistributionParam * (vc_CurrentTotalTemperatureSum
	//								 + vc_CurrentTemperatureSum[1]))), 1.8);
	//  }
	//
	//  if (vc_RootDistributionFactor > 1.00) { // HERMES: 0.35
	//    vc_RootDistributionFactor = 1.00;
	//  }
	//
	//  if (vc_RootDistributionFactor < (0.45 / (vc_MaxRootingDepth * (vs_LayerThickness * 100.0)))) {
	//    vc_RootDistributionFactor = 0.45 / (vc_MaxRootingDepth * (vs_LayerThickness * 100.0));
	//  }
	//
	//  // original HERMES:
	//  // vc_RootingDepth = int(floor(0.5 + (4.5 / vc_RootDistributionFactor / (vs_LayerThickness * 100.0)))); //[layer]
	//
	//  // new:
	//  vc_RootingDepth = int(floor((1.1 - vc_RootDistributionFactor) * (1.0 / vs_LayerThickness) * vc_MaxRootingDepth)); //[layer]
	//
	//
	//
	//
	//  // Assuming that root diameter [cm] decreases with depth
	//  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
	//
	//    if (pc_AbovegroundOrgan[3] == 0) {
	//      vc_RootDiameter[i_Layer] = 0.0001; //[m]
	//    } else {
	//      vc_RootDiameter[i_Layer] = 0.0002 - ((i_Layer + 1) * 0.00001); // [m]
	//    }
	//  }
	//
	//
	//
	//  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
	//
	//    vc_SoilDepth = (i_Layer + 1) * vs_LayerThickness; //[m]
	//
	//    // Root length down to layer i [m]
	//    vc_RootLengthToLayer[i_Layer] = (vc_RootBiomass * (1.0 - exp(-vc_RootDistributionFactor * vc_SoilDepth * 100.0))
	//			       / 100000.0 * 100.0 / 7.0) / 100.0;
	//
	//    if (i_Layer > 1) {
	//      vc_RootLengthInLayer[i_Layer] = fabs(vc_RootLengthToLayer[i_Layer] - vc_RootLengthToLayer[i_Layer - 1])
	//			        / (vc_RootDiameter[i_Layer] * vc_RootDiameter[i_Layer] * PI) / vs_LayerThickness;
	//      // [m m-3]
	//    } else {
	//      vc_RootLengthInLayer[i_Layer] = fabs(vc_RootLengthToLayer[i_Layer]) / (vc_RootDiameter[i_Layer]
	//							      * vc_RootDiameter[i_Layer] * PI) / vs_LayerThickness;
	//      // [m m-3]
	//    }
	//
	//    // Root density per volume soil [m m-3]
	//    vc_RootDensity[i_Layer] = vc_RootLengthInLayer[i_Layer];
	//
	//    // Root surface [m m-2]
	//    vc_RootSurface[i_Layer] = vc_RootDensity[i_Layer] * vc_RootDiameter[i_Layer] * 2.0 * PI;
	//  }
	//
	//  vc_TotalRootLength = 0;
	//  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
	//
	//    // Total root length in [m m-2]
	//    vc_TotalRootLength += vc_RootDensity[i_Layer] * vs_LayerThickness;
	//  }
	//
	//  for (int i_Layer = 0; i_Layer < vc_RootingDepth; i_Layer++) {
	//    // default root decay - 10 %
	//    vo_FreshSoilOrganicMatter[i_Layer] += vc_RootNIncrement * vc_RootDensity[i_Layer] * 10.0 / vc_TotalRootLength;
	//  }
	//


		// Limiting the maximum N-uptake to 26-13*10^-14 mol/cm W./sec
	vc_MaxNUptake = pc_MaxNUptakeParam - (vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum); // [kg m Wurzel-1]

	if((vc_CropNDemand / 10000.0) > (vc_TotalRootLength * vc_MaxNUptake * vc_TimeStep))
	{
		vc_CropNDemand = vc_TotalRootLength * vc_MaxNUptake * vc_TimeStep; //[kg m-2]
	}
	else
	{
		vc_CropNDemand = vc_CropNDemand / 10000.0; // [kg ha-1 --> kg m-2]
	}





}

/**
 * @brief Reference evapotranspiration
 *
 * A method following Penman-Monteith as described by the FAO in Allen
 * RG, Pereira LS, Raes D, Smith M. (1998) Crop evapotranspiration.
 * Guidelines for computing crop water requirements. FAO Irrigation and
 * Drainage Paper 56, FAO, Roma
 *
 * @param vs_HeightNN Height above sea level
 * @param vw_MaxAirTemperature Maximal air temperature for the calculated day
 * @param vw_MinAirTemperature Minimal air temperature for the calculated day
 * @param vw_RelativeHumidity Relative humidity
 * @param vw_MeanAirTemperature Mean air temperature
 * @param vw_WindSpeed Spped of wind
 * @param vw_WindSpeedHeight Height in which the wind speed has been measured *
 * @param vc_GlobalRadiation Global radiation
 * @param vw_AtmosphericCO2Concentration CO2 concentration in the athmosphere (needed for photosynthesis)
 * @param vc_GrossPhotosynthesisReference_mol under well watered conditions
 * @return Reference evapotranspiration
 */
double CropGrowth::fc_ReferenceEvapotranspiration(double vs_HeightNN,
																									double vw_MaxAirTemperature,
																									double vw_MinAirTemperature,
																									double vw_RelativeHumidity,
																									double vw_MeanAirTemperature,
																									double vw_WindSpeed,
																									double vw_WindSpeedHeight,
																									double vc_GlobalRadiation,
																									double vw_AtmosphericCO2Concentration,
																									double vc_GrossPhotosynthesisReference_mol)
{
	double vc_AtmosphericPressure; //[kPA]
	double vc_PsycrometerConstant; //[kPA °C-1]
	double vc_SaturatedVapourPressureMax; //[kPA]
	double vc_SaturatedVapourPressureMin; //[kPA]
	double vc_SaturatedVapourPressure; //[kPA]
	double vc_VapourPressure; //[kPA]
	double vc_SaturationDeficit; //[kPA]
	double vc_SaturatedVapourPressureSlope; //[kPA °C-1]
	double vc_WindSpeed_2m; //[m s-1]
	double vc_AerodynamicResistance; //[s m-1]
	double vc_SurfaceResistance; //[s m-1]
	double vc_ReferenceEvapotranspiration; //[mm]
	double vw_NetRadiation; //[MJ m-2]

	const UserCropParameters& user_crops = cropPs;
	double pc_SaturationBeta = user_crops.pc_SaturationBeta; // Original: Yu et al. 2001; beta = 3.5
	double pc_StomataConductanceAlpha = user_crops.pc_StomataConductanceAlpha; // Original: Yu et al. 2001; alpha = 0.06
	double pc_ReferenceAlbedo = user_crops.pc_ReferenceAlbedo; // FAO Green gras reference albedo from Allen et al. (1998)

	// Calculation of atmospheric pressure
	vc_AtmosphericPressure = 101.3 * pow(((293.0 - (0.0065 * vs_HeightNN)) / 293.0), 5.26);

	// Calculation of psychrometer constant - Luchtfeuchtigkeit
	vc_PsycrometerConstant = 0.000665 * vc_AtmosphericPressure;

	// Calc. of saturated water vapour pressure at daily max temperature
	vc_SaturatedVapourPressureMax = 0.6108 * exp((17.27 * vw_MaxAirTemperature) / (237.3 + vw_MaxAirTemperature));

	// Calc. of saturated water vapour pressure at daily min temperature
	vc_SaturatedVapourPressureMin = 0.6108 * exp((17.27 * vw_MinAirTemperature) / (237.3 + vw_MinAirTemperature));

	// Calculation of the saturated water vapour pressure
	vc_SaturatedVapourPressure = (vc_SaturatedVapourPressureMax + vc_SaturatedVapourPressureMin) / 2.0;

	// Calculation of the water vapour pressure
	if(vw_RelativeHumidity <= 0.0)
	{
		// Assuming Tdew = Tmin as suggested in FAO56 Allen et al. 1998
		vc_VapourPressure = vc_SaturatedVapourPressureMin;
	}
	else
	{
		vc_VapourPressure = vw_RelativeHumidity * vc_SaturatedVapourPressure;
	}

	// Calculation of the air saturation deficit
	vc_SaturationDeficit = vc_SaturatedVapourPressure - vc_VapourPressure;

	// Slope of saturation water vapour pressure-to-temperature relation
	vc_SaturatedVapourPressureSlope = (4098.0 * (0.6108 * exp((17.27 * vw_MeanAirTemperature) / (vw_MeanAirTemperature
																																															 + 237.3)))) / ((vw_MeanAirTemperature + 237.3) * (vw_MeanAirTemperature + 237.3));

	// Calculation of wind speed in 2m height
	vc_WindSpeed_2m = vw_WindSpeed * (4.87 / (log(67.8 * vw_WindSpeedHeight - 5.42)));

	// Calculation of the aerodynamic resistance
	vc_AerodynamicResistance = 208.0 / vc_WindSpeed_2m;

	if(vc_GrossPhotosynthesisReference_mol <= 0.0)
	{
		vc_StomataResistance = 999999.9; // [s m-1]
	}
	else
	{

		if(pc_CarboxylationPathway = 1)
		{
			vc_StomataResistance = // [s m-1]
				(vw_AtmosphericCO2Concentration * (1.0 + vc_SaturationDeficit / pc_SaturationBeta))
				/ (pc_StomataConductanceAlpha * vc_GrossPhotosynthesisReference_mol);
		}
		else
		{
			vc_StomataResistance = // [s m-1]
				(vw_AtmosphericCO2Concentration * (1.0 + vc_SaturationDeficit / pc_SaturationBeta))
				/ (pc_StomataConductanceAlpha * vc_GrossPhotosynthesisReference_mol);
		}
	}

	vc_SurfaceResistance = vc_StomataResistance / 1.44;

	// vc_SurfaceResistance = vc_StomataResistance / (vc_CropHeight * vc_LeafAreaIndex);

	// vw_NetRadiation = vc_GlobalRadiation * (1.0 - pc_ReferenceAlbedo); // [MJ m-2]

	double vc_ClearSkyShortwaveRadiation = (0.75 + 0.00002 * vs_HeightNN) * vc_ExtraterrestrialRadiation;
	double vc_RelativeShortwaveRadiation = vc_GlobalRadiation / vc_ClearSkyShortwaveRadiation;
	double vc_NetShortwaveRadiation = (1.0 - pc_ReferenceAlbedo) * vc_GlobalRadiation;

	double pc_BolzmanConstant = 0.0000000049; // Bolzmann constant 4.903 * 10-9 MJ m-2 K-4 d-1
	vw_NetRadiation = vc_NetShortwaveRadiation - (pc_BolzmanConstant
																								* (pow((vw_MinAirTemperature + 273.16), 4.0) + pow((vw_MaxAirTemperature
																																																		+ 273.16), 4.0)) / 2.0 * (1.35 * vc_RelativeShortwaveRadiation - 0.35)
																								* (0.34 - 0.14 * sqrt(vc_VapourPressure)));

	// Calculation of reference evapotranspiration
	// Penman-Monteith-Method FAO
	vc_ReferenceEvapotranspiration = ((0.408 * vc_SaturatedVapourPressureSlope * vw_NetRadiation)
																		+ (vc_PsycrometerConstant * (900.0 / (vw_MeanAirTemperature + 273.0)) * vc_WindSpeed_2m * vc_SaturationDeficit))
		/ (vc_SaturatedVapourPressureSlope + vc_PsycrometerConstant * (1.0 + (vc_SurfaceResistance / vc_AerodynamicResistance)));

	return vc_ReferenceEvapotranspiration;
}

/**
 * @brief  Water uptake by the crop
 *
 *  In this function the potential transpiration calcuated from potential
 *  evapotranspiration by soil cover fraction is reduced by water availability
 *  in the soil accoridng to actaul water contents, root distribution and
 *  root effectivity.
 *
 * @param vs_NumberOfLayers
 * @param vs_LayerThickness
 * @param vc_SoilCoverage
 * @param vc_RootingZone
 * @param vc_GroundwaterTable
 * @param vc_ReferenceEvapotranspiration
 * @param vw_GrossPrecipitation
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_CropWaterUptake(double vc_SoilCoverage,
																		size_t vc_RootingZone,
																		size_t vc_GroundwaterTable,
																		double vc_ReferenceEvapotranspiration,
																		double vw_GrossPrecipitation,
																		double /*vc_CurrentTotalTemperatureSum*/,
																		double /*vc_TotalTemperatureSum*/)
{
	size_t nols = soilColumn.vs_NumberOfLayers();
	double layerThickness = soilColumn.vs_LayerThickness();

	double vc_PotentialTranspirationDeficit = 0.0; // [mm]
	vc_PotentialTranspiration = 0.0; // old TRAMAX [mm]
	double vc_PotentialEvapotranspiration = 0.0; // [mm]
	double vc_TranspirationReduced = 0.0; // old TDRED [mm]
	vc_ActualTranspiration = 0.0; // [mm]
	double vc_RemainingTotalRootEffectivity = 0.0; //old WEFFREST [m]
	double vc_CropWaterUptakeFromGroundwater = 0.0; // old GAUF [mm]
	double vc_TotalRootEffectivity = 0.0; // old WEFF [m]
	double vc_ActualTranspirationDeficit = 0.0; // old TREST [mm]
	double vc_Interception = 0.0;
	vc_RemainingEvapotranspiration = 0.0;

	for(size_t i_Layer = 0; i_Layer < nols; i_Layer++)
	{
		vc_Transpiration[i_Layer] = 0.0; // old TP [mm]
		vc_TranspirationRedux[i_Layer] = 0.0; // old TRRED []
		vc_RootEffectivity[i_Layer] = 0.0; // old WUEFF [?]
	}

	// ################
	// # Interception #
	// ################

	double vc_InterceptionStorageOld = vc_InterceptionStorage;

	// Interception in [mm d-1];
	vc_Interception = (2.5 * vc_CropHeight * vc_SoilCoverage) - vc_InterceptionStorage;

	if(vc_Interception < 0)
	{
		vc_Interception = 0.0;
	}

	// If no precipitation occurs, vm_Interception = 0
	if(vw_GrossPrecipitation <= 0)
	{
		vc_Interception = 0.0;
	}

	// Calculating net precipitation and adding to surface water
	if(vw_GrossPrecipitation <= vc_Interception)
	{
		vc_Interception = vw_GrossPrecipitation;
		vc_NetPrecipitation = 0.0;
	}
	else
	{
		vc_NetPrecipitation = vw_GrossPrecipitation - vc_Interception;
	}

	// add intercepted precipitation to the virtual interception water storage
	vc_InterceptionStorage = vc_InterceptionStorageOld + vc_Interception;


	// #################
	// # Transpiration #
	// #################

	vc_PotentialEvapotranspiration = vc_ReferenceEvapotranspiration * vc_KcFactor; // [mm]

	// from HERMES:
	if(vc_PotentialEvapotranspiration > 6.5) vc_PotentialEvapotranspiration = 6.5;

	vc_RemainingEvapotranspiration = vc_PotentialEvapotranspiration; // [mm]

	// If crop holds intercepted water, first evaporation from crop surface
	if(vc_InterceptionStorage > 0.0)
	{
		if(vc_RemainingEvapotranspiration >= vc_InterceptionStorage)
		{
			vc_RemainingEvapotranspiration -= vc_InterceptionStorage;
			vc_EvaporatedFromIntercept = vc_InterceptionStorage;
			vc_InterceptionStorage = 0.0;
		}
		else
		{
			vc_InterceptionStorage -= vc_RemainingEvapotranspiration;
			vc_EvaporatedFromIntercept = vc_RemainingEvapotranspiration;
			vc_RemainingEvapotranspiration = 0.0;
		}
	}
	else
	{
		vc_EvaporatedFromIntercept = 0.0;
	}

	// if the plant has matured, no transpiration occurs!
	if(vc_DevelopmentalStage < vc_FinalDevelopmentalStage)
	{
		//if ((vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum) < 1.0){

		vc_PotentialTranspiration = vc_RemainingEvapotranspiration * vc_SoilCoverage; // [mm]

		for(size_t i_Layer = 0; i_Layer < vc_RootingZone; i_Layer++)
		{
			double vc_AvailableWater = soilColumn[i_Layer].vs_FieldCapacity() - soilColumn[i_Layer].vs_PermanentWiltingPoint();
			double vc_AvailableWaterPercentage = (soilColumn[i_Layer].get_Vs_SoilMoisture_m3()
																						- soilColumn[i_Layer].vs_PermanentWiltingPoint()) / vc_AvailableWater;
			if(vc_AvailableWaterPercentage < 0.0) vc_AvailableWaterPercentage = 0.0;

			if(vc_AvailableWaterPercentage < 0.15)
			{
				vc_TranspirationRedux[i_Layer] = vc_AvailableWaterPercentage * 3.0; // []
				vc_RootEffectivity[i_Layer] = 0.15 + 0.45 * vc_AvailableWaterPercentage / 0.15; // []
			}
			else if(vc_AvailableWaterPercentage < 0.3)
			{
				vc_TranspirationRedux[i_Layer] = 0.45 + (0.25 * (vc_AvailableWaterPercentage - 0.15) / 0.15);
				vc_RootEffectivity[i_Layer] = 0.6 + (0.2 * (vc_AvailableWaterPercentage - 0.15) / 0.15);
			}
			else if(vc_AvailableWaterPercentage < 0.5)
			{
				vc_TranspirationRedux[i_Layer] = 0.7 + (0.275 * (vc_AvailableWaterPercentage - 0.3) / 0.2);
				vc_RootEffectivity[i_Layer] = 0.8 + (0.2 * (vc_AvailableWaterPercentage - 0.3) / 0.2);
			}
			else if(vc_AvailableWaterPercentage < 0.75)
			{
				vc_TranspirationRedux[i_Layer] = 0.975 + (0.025 * (vc_AvailableWaterPercentage - 0.5) / 0.25);
				vc_RootEffectivity[i_Layer] = 1.0;
			}
			else
			{
				vc_TranspirationRedux[i_Layer] = 1.0;
				vc_RootEffectivity[i_Layer] = 1.0;
			}
			if(vc_TranspirationRedux[i_Layer] < 0)
				vc_TranspirationRedux[i_Layer] = 0.0;
			if(vc_RootEffectivity[i_Layer] < 0)
				vc_RootEffectivity[i_Layer] = 0.0;
			if(i_Layer == vc_GroundwaterTable)
			{ // old GRW
				vc_RootEffectivity[i_Layer] = 0.5;
			}
			if(i_Layer > vc_GroundwaterTable)
			{ // old GRW
				vc_RootEffectivity[i_Layer] = 0.0;
			}
			if(((i_Layer + 1)*layerThickness) >= vs_MaxEffectiveRootingDepth)
			{
				vc_RootEffectivity[i_Layer] = 0.0;
			}

			vc_TotalRootEffectivity += vc_RootEffectivity[i_Layer] * vc_RootDensity[i_Layer]; //[m m-3]
			vc_RemainingTotalRootEffectivity = vc_TotalRootEffectivity;
		}

		//std::cout << setprecision(11) << "vc_TotalRootEffectivity: " << vc_TotalRootEffectivity << std::endl;
		//std::cout << setprecision(11) << "vc_OxygenDeficit: " << vc_OxygenDeficit << std::endl;

		for(size_t i_Layer = 0; i_Layer < nols; i_Layer++)
		{
			if(i_Layer > min(vc_RootingZone, vc_GroundwaterTable + 1))
			{
				vc_Transpiration[i_Layer] = 0.0; //[mm]
			}
			else
			{
				vc_Transpiration[i_Layer] = vc_TotalRootEffectivity != 0.0
					? vc_PotentialTranspiration * ((vc_RootEffectivity[i_Layer] * vc_RootDensity[i_Layer])
																				 / vc_TotalRootEffectivity) * vc_OxygenDeficit
					: 0;

				//std::cout << setprecision(11) << "vc_Transpiration[i_Layer]: " << i_Layer << ", " << vc_Transpiration[i_Layer] << std::endl;
				//std::cout << setprecision(11) << "vc_RootEffectivity[i_Layer]: " << i_Layer << ", " << vc_RootEffectivity[i_Layer] << std::endl;
				//std::cout << setprecision(11) << "vc_RootDensity[i_Layer]: " << i_Layer << ", " << vc_RootDensity[i_Layer] << std::endl;

				// [mm]
			}
		}

		for(size_t i_Layer = 0; i_Layer < min(vc_RootingZone, vc_GroundwaterTable + 1); i_Layer++)
		{


			vc_RemainingTotalRootEffectivity -= vc_RootEffectivity[i_Layer] * vc_RootDensity[i_Layer]; // [m m-3]

			if(vc_RemainingTotalRootEffectivity <= 0.0)
				vc_RemainingTotalRootEffectivity = 0.00001;
			if(((vc_Transpiration[i_Layer] / 1000.0) / layerThickness) > ((soilColumn[i_Layer].get_Vs_SoilMoisture_m3()
																																		 - soilColumn[i_Layer].vs_PermanentWiltingPoint())))
			{
				vc_PotentialTranspirationDeficit = (((vc_Transpiration[i_Layer] / 1000.0) / layerThickness)
																						- (soilColumn[i_Layer].get_Vs_SoilMoisture_m3() - soilColumn[i_Layer].vs_PermanentWiltingPoint()))
					* layerThickness * 1000.0; // [mm]
				if(vc_PotentialTranspirationDeficit < 0.0)
				{
					vc_PotentialTranspirationDeficit = 0.0;
				}
				if(vc_PotentialTranspirationDeficit > vc_Transpiration[i_Layer])
				{
					vc_PotentialTranspirationDeficit = vc_Transpiration[i_Layer]; //[mm]
				}
			}
			else
			{
				vc_PotentialTranspirationDeficit = 0.0;
			}
			vc_TranspirationReduced = vc_Transpiration[i_Layer] * (1.0 - vc_TranspirationRedux[i_Layer]);

			//! @todo Claas: How can we lower the groundwater table if crop water uptake is restricted in that layer?
			vc_ActualTranspirationDeficit = max(vc_TranspirationReduced, vc_PotentialTranspirationDeficit); //[mm]
			if(vc_ActualTranspirationDeficit > 0.0)
			{
				if(i_Layer < min(vc_RootingZone, vc_GroundwaterTable + 1))
				{
					for(size_t i_Layer2 = i_Layer + 1; i_Layer2 < min(vc_RootingZone, vc_GroundwaterTable + 1); i_Layer2++)
					{
						vc_Transpiration[i_Layer2] += vc_ActualTranspirationDeficit * (vc_RootEffectivity[i_Layer2]
																																					 * vc_RootDensity[i_Layer2] / vc_RemainingTotalRootEffectivity);

					}
				}
			}
			vc_Transpiration[i_Layer] = vc_Transpiration[i_Layer] - vc_ActualTranspirationDeficit;
			if(vc_Transpiration[i_Layer] < 0.0)
				vc_Transpiration[i_Layer] = 0.0;
			vc_ActualTranspiration += vc_Transpiration[i_Layer];
			if(i_Layer == vc_GroundwaterTable)
			{
				vc_CropWaterUptakeFromGroundwater = (vc_Transpiration[i_Layer] / 1000.0) / layerThickness; //[m3 m-3]
			}
		}
		if(vc_PotentialTranspiration > 0)
		{
			vc_TranspirationDeficit = vc_ActualTranspiration / vc_PotentialTranspiration;
		}
		else
		{
			vc_TranspirationDeficit = 1.0; //[]
		}

		int vm_GroundwaterDistance = vc_GroundwaterTable - vc_RootingDepth;
		//std::cout << "vm_GroundwaterDistance: " << vm_GroundwaterDistance << std::endl;
		if(vm_GroundwaterDistance <= 1)
		{
			vc_TranspirationDeficit = 1.0;
		}

		if(pc_WaterDeficitResponseOn == false)
		{
			vc_TranspirationDeficit = 1.0;
		}


	} //if
}

/**
 * @brief Nitrogen uptake by the crop
 *
 * @param  vs_NumberOfLayers Number of soil layers
 * @param  vc_RootingZone rooting depth of current crop
 * @param  vc_GroundwaterTable Depth of groundwater table
 *
 * @author Claas Nendel
 */
void CropGrowth::fc_CropNUptake(int vc_RootingZone,
																int vc_GroundwaterTable,
																double /*vc_CurrentTotalTemperatureSum*/,
																double /*vc_TotalTemperatureSum*/)
{
	size_t nols = soilColumn.vs_NumberOfLayers();
	double layerThickness = soilColumn.vs_LayerThickness();

	double vc_ConvectiveNUptake = 0.0; // old TRNSUM
	double vc_DiffusiveNUptake = 0.0; // old SUMDIFF
	std::vector<double> vc_ConvectiveNUptakeFromLayer(nols, 0.0); // old MASS
	std::vector<double> vc_DiffusionCoeff(nols, 0.0); // old D
	std::vector<double> vc_DiffusiveNUptakeFromLayer(nols, 0.0); // old DIFF
	double vc_ConvectiveNUptake_1 = 0.0; // old MASSUM
	double vc_DiffusiveNUptake_1 = 0.0; // old DIFFSUM
	double pc_MinimumAvailableN = cropPs.pc_MinimumAvailableN; // kg m-3
	double pc_MinimumNConcentrationRoot = cropPs.pc_MinimumNConcentrationRoot;  // kg kg-1
	double pc_MaxCropNDemand = cropPs.pc_MaxCropNDemand;

	vc_TotalNUptake = 0.0;
	vc_TotalNInput = 0.0;
	vc_FixedN = 0.0;
	//for (size_t i_Layer = 0; i_Layer < nols; i_Layer++)
	//	vc_NUptakeFromLayer[i_Layer] = 0.0;
	for(auto& v : vc_NUptakeFromLayer)
		v = 0.0;

	// if the plant has matured, no N uptake occurs!
	if(vc_DevelopmentalStage < vc_FinalDevelopmentalStage)
	{
		//if ((vc_CurrentTotalTemperatureSum / vc_TotalTemperatureSum) < 1.0){

		for(int i_Layer = 0; i_Layer < (min(vc_RootingZone, vc_GroundwaterTable)); i_Layer++)
		{

			vs_SoilMineralNContent[i_Layer] = soilColumn[i_Layer].vs_SoilNO3; // [kg m-3]

			// Convective N uptake per layer
			vc_ConvectiveNUptakeFromLayer[i_Layer] = (vc_Transpiration[i_Layer] / 1000.0) * //[mm --> m]
				(vs_SoilMineralNContent[i_Layer] / // [kg m-3]
				(soilColumn[i_Layer].get_Vs_SoilMoisture_m3())) * // old WG [m3 m-3]
				vc_TimeStep; // -->[kg m-2]

			vc_ConvectiveNUptake += vc_ConvectiveNUptakeFromLayer[i_Layer]; // [kg m-2]

			/** @todo Claas: Woher kommt der Wert für vs_Tortuosity? */
			/** @todo Claas: Prüfen ob Umstellung auf [m] die folgenden Gleichungen beeinflusst */
			vc_DiffusionCoeff[i_Layer] = 0.000214 * (vs_Tortuosity * exp(soilColumn[i_Layer].get_Vs_SoilMoisture_m3() * 10))
				/ soilColumn[i_Layer].get_Vs_SoilMoisture_m3(); //[m2 d-1]

			vc_DiffusiveNUptakeFromLayer[i_Layer] = (vc_DiffusionCoeff[i_Layer] * // [m2 d-1]
																							 soilColumn[i_Layer].get_Vs_SoilMoisture_m3() * // [m3 m-3]
																							 2.0 * PI * vc_RootDiameter[i_Layer] * // [m]
																							 (vs_SoilMineralNContent[i_Layer] / 1000.0 / // [kg m-3]
																								soilColumn[i_Layer].get_Vs_SoilMoisture_m3() - 0.000014) * // [m3 m-3]
																							 sqrt(PI * vc_RootDensity[i_Layer])) * // [m m-3]
				vc_RootDensity[i_Layer] * 1000.0 * vc_TimeStep; // -->[kg m-2]

			if(vc_DiffusiveNUptakeFromLayer[i_Layer] < 0.0)
			{
				vc_DiffusiveNUptakeFromLayer[i_Layer] = 0;
			}

			vc_DiffusiveNUptake += vc_DiffusiveNUptakeFromLayer[i_Layer]; // [kg m-2]

		}

		for(int i_Layer = 0; i_Layer < (min(vc_RootingZone, vc_GroundwaterTable)); i_Layer++)
		{

			if(vc_CropNDemand > 0.0)
			{

				if(vc_ConvectiveNUptake >= vc_CropNDemand)
				{
					// convective N uptake is sufficient
					vc_NUptakeFromLayer[i_Layer] = vc_CropNDemand * vc_ConvectiveNUptakeFromLayer[i_Layer] / vc_ConvectiveNUptake;
				}
				else
				{
					// N demand is not covered
					if((vc_CropNDemand - vc_ConvectiveNUptake) < vc_DiffusiveNUptake)
					{
						vc_NUptakeFromLayer[i_Layer] = vc_ConvectiveNUptakeFromLayer[i_Layer] + ((vc_CropNDemand
																																											- vc_ConvectiveNUptake) * vc_DiffusiveNUptakeFromLayer[i_Layer] / vc_DiffusiveNUptake);
					}
					else
					{
						vc_NUptakeFromLayer[i_Layer] = vc_ConvectiveNUptakeFromLayer[i_Layer] + vc_DiffusiveNUptakeFromLayer[i_Layer];
					}
				}

				vc_ConvectiveNUptake_1 += vc_ConvectiveNUptakeFromLayer[i_Layer];
				vc_DiffusiveNUptake_1 += vc_DiffusiveNUptakeFromLayer[i_Layer];

				if(vc_NUptakeFromLayer[i_Layer] > ((vs_SoilMineralNContent[i_Layer] * layerThickness) - pc_MinimumAvailableN))
				{
					vc_NUptakeFromLayer[i_Layer] = (vs_SoilMineralNContent[i_Layer] * layerThickness) - pc_MinimumAvailableN;
				}

				if(vc_NUptakeFromLayer[i_Layer] > (pc_MaxCropNDemand / 10000.0 * 0.75))
				{
					vc_NUptakeFromLayer[i_Layer] = (pc_MaxCropNDemand / 10000.0 * 0.75);
				}

				if(vc_NUptakeFromLayer[i_Layer] < 0.0)
				{
					vc_NUptakeFromLayer[i_Layer] = 0.0;
				}
			}
			else
			{
				vc_NUptakeFromLayer[i_Layer] = 0.0;
			}

			vc_TotalNUptake += vc_NUptakeFromLayer[i_Layer] * 10000.0; //[kg m-2] --> [kg ha-1]

		} // for

		vc_FixedN = pc_PartBiologicalNFixation * vc_CropNDemand * 10000.0; // [kg N ha-1]
		//Part of the deficit which can be covered by biologocal N fixation.

		if(((vc_CropNDemand * 10000.0) - vc_TotalNUptake) < vc_FixedN)
		{
			vc_TotalNInput = vc_CropNDemand * 10000.0;
			vc_FixedN = (vc_CropNDemand * 10000.0) - vc_TotalNUptake;
		}
		else
		{
			vc_TotalNInput = vc_TotalNUptake + vc_FixedN;
		}
	} // if

	vc_SumTotalNUptake += vc_TotalNUptake;


	if(vc_RootBiomass > vc_RootBiomassOld)
	{

		// wurzel ist gewachsen
		vc_NConcentrationRoot = ((vc_RootBiomassOld * vc_NConcentrationRoot)
														 + ((vc_RootBiomass - vc_RootBiomassOld) / (vc_AbovegroundBiomass
																																				- vc_AbovegroundBiomassOld + vc_BelowgroundBiomass - vc_BelowgroundBiomassOld
																																				+ vc_RootBiomass - vc_RootBiomassOld) * vc_TotalNInput)) / vc_RootBiomass;

		vc_NConcentrationRoot = min(vc_NConcentrationRoot, pc_StageMaxRootNConcentration[vc_DevelopmentalStage]);


		if(vc_NConcentrationRoot < pc_MinimumNConcentrationRoot)
		{
			vc_NConcentrationRoot = pc_MinimumNConcentrationRoot;
		}
	}

	vc_NConcentrationAbovegroundBiomass = (vc_TotalBiomassNContent + vc_TotalNInput
																				 - (vc_RootBiomass * vc_NConcentrationRoot))
		/ (vc_AbovegroundBiomass + (vc_BelowgroundBiomass / pc_ResidueNRatio));

	if((vc_NConcentrationAbovegroundBiomass * vc_AbovegroundBiomass) < (vc_AbovegroundBiomassOld
																																			* vc_NConcentrationAbovegroundBiomassOld))
	{

		vc_NConcentrationAbovegroundBiomass = vc_AbovegroundBiomassOld * vc_NConcentrationAbovegroundBiomassOld
			/ vc_AbovegroundBiomass;

		vc_NConcentrationRoot = (vc_TotalBiomassNContent + vc_TotalNInput
														 - (vc_AbovegroundBiomass * vc_NConcentrationAbovegroundBiomass)
														 - (vc_NConcentrationAbovegroundBiomass / pc_ResidueNRatio * vc_BelowgroundBiomass)) / vc_RootBiomass;
	}
}



/**
 * @brief Calculation of gross primary production [kg C ha-1 d-1]
 *
 * @param vc_LeafAreaIndex
 * @param vc_Assimilates
 * @return daily gross primary production per hectare
 *
 * @author Claas Nendel
 */
double CropGrowth::fc_GrossPrimaryProduction(double vc_Assimilates)
{
	double vc_GPP = 0.0;
	// Converting photosynthesis rate from [kg CH2O ha-1 d-1] back to
	// [kg C ha-1 d-1]
	vc_GPP = vc_Assimilates / 30.0 * 12.0;
	return vc_GPP;
}

/**
* @brief Calculation of net primary production [kg C ha-1 d-1]
*
* @param vc_GrossPrimaryProduction
* @param vc_TotalRespired
* @param vc_LeafAreaIndex
* @return daily net primary production per hectare
*
* @author Claas Nendel
*/
double CropGrowth::fc_NetPrimaryProduction(double vc_GrossPrimaryProduction,
																					 double vc_TotalRespired)
{
	double vc_NPP = 0.0;
	// Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
	vc_Respiration = vc_TotalRespired / 30.0 * 12.0;

	vc_NPP = vc_GrossPrimaryProduction - vc_Respiration;
	return vc_NPP;
}

/**
 * @brief Returns crop name [ ]
 * @return crop name
 */
std::string CropGrowth::get_CropName() const
{
	return pc_CropName;
}

/**
 * @brief Returns gross photosynthesis rate [mol m-2 s-1]
 * @return photosynthesis rate
 */
double CropGrowth::get_GrossPhotosynthesisRate() const
{
	return vc_GrossPhotosynthesis_mol;
}

/**
 * @brief Returns gross photosynthesis rate [kg ha-1]
 * @return photosynthesis rate
 */
double CropGrowth::get_GrossPhotosynthesisHaRate() const
{
	return vc_GrossPhotosynthesis;
}

/**
 * @brief Returns assimilation rate [kg CO2 ha leaf-1]
 * @return Assimilation rate
 */
double CropGrowth::get_AssimilationRate() const
{
	return vc_AssimilationRate;
}

/**
 * @brief Returns assimilates [kg CO2 ha-1]
 * @return Assimilates
 */
double CropGrowth::get_Assimilates() const
{
	return vc_Assimilates;
}


/**
 * @brief Returns net maintenance respiration rate [kg CO2 ha-1]
 * @return Net maintenance respiration rate
 */
double CropGrowth::get_NetMaintenanceRespiration() const
{
	return vc_NetMaintenanceRespiration;
}

/**
 * @brief Returns maintenance respiration rate from AGROSIM [kg CO2 ha-1]
 * @return Maintenance respiration rate
 */
double CropGrowth::get_MaintenanceRespirationAS() const
{
	return vc_MaintenanceRespirationAS;
}


/**
 * @brief Returns growth respiration rate from AGROSIM [kg CO2 ha-1]
 * @return GRowth respiration rate
 */
double CropGrowth::get_GrowthRespirationAS() const
{
	return vc_GrowthRespirationAS;
}

/**
 */
double CropGrowth::get_VernalisationFactor() const
{
	return vc_VernalisationFactor;
}

/**
 */
double CropGrowth::get_DaylengthFactor() const
{
	return vc_DaylengthFactor;
}

/**
 * @brief Returns growth increment of organ i [kg CH2O ha-1 d-1]
 * @return Organ growth increment
 */
double CropGrowth::get_OrganGrowthIncrement(int i_Organ) const
{
	return vc_OrganGrowthIncrement[i_Organ];
}

/**
 * @brief Returns net photosynthesis [kg CH2O ha-1]
 * @return net photosynthesis
 */
double CropGrowth::get_NetPhotosynthesis() const
{
	return vc_NetPhotosynthesis;
}


Voc::Emissions CropGrowth::calculateVOCEmissions(const Voc::MicroClimateData& mcd)
{
	Voc::SpeciesData species;
	species.id = 0; // right now we just have one crop at a time, so no need to distinguish multiple crops
	species.lai = get_LeafAreaIndex();
	species.mFol = get_OrganBiomass(LEAF) / (100. * 100.); //kg/ha -> kg/m2
	species.sla = pc_SpecificLeafArea[vc_DevelopmentalStage] * 100. * 100.; //ha/kg -> m2/kg

	auto gems = Voc::calculateGuentherVOCEmissions(species, mcd);
	debug() << "guenther: isoprene: " << gems.isoprene_emission << " monoterpene: " << gems.monoterpene_emission << endl;

	auto jjvems = Voc::calculateJJVVOCEmissions(species, mcd);
	debug() << "jjv: isoprene: " << jjvems.isoprene_emission << " monoterpene: " << jjvems.monoterpene_emission << endl;

	return gems;
}

/**
 * @brief Returns reference evapotranspiration [mm]
 * @return Reference evapotranspiration
 */
double CropGrowth::get_ReferenceEvapotranspiration() const
{
	return vc_ReferenceEvapotranspiration;
}

/**
 * @brief Returns evapotranspiration remaining after evaporation of intercepted water [mm]
 * @return Remaning evapotranspirationn
 */
double CropGrowth::get_RemainingEvapotranspiration() const
{
	return vc_RemainingEvapotranspiration;
}

/**
 * @brief Returns evaporation from intercepted water [mm]
 * @return evaporated from intercept
 */
double CropGrowth::get_EvaporatedFromIntercept() const
{
	return vc_EvaporatedFromIntercept;
}

/**
 * @brief Returns precipitation after interception on crop surface [mm]
 * @return Remaning net precipitation
 */
double CropGrowth::get_NetPrecipitation() const
{
	return vc_NetPrecipitation;
}

/**
 * @brief Returns leaf area index [m2 m-2]
 * @return Leaf area index
 */
double CropGrowth::get_LeafAreaIndex() const
{
	return vc_LeafAreaIndex;
}

/**
 * @brief Returns crop height [m]
 * @return crop height
 */
double CropGrowth::get_CropHeight() const
{
	return vc_CropHeight;
}

/**
 * @brief Returns rooting depth [layer]
 * @return rooting depth
 */
int CropGrowth::get_RootingDepth() const
{
	return vc_RootingDepth;
}

/**
 * @brief Returns soil coverage [0;1]
 * @return soil coverage
 */
double CropGrowth::get_SoilCoverage() const
{
	return vc_SoilCoverage;
}

/**
 * @brief Returns current Kc factor []
 * @return Kc factor
 */
double CropGrowth::get_KcFactor() const
{
	return vc_KcFactor;
}

/**
 * @brief Returns Stomata resistance [s m-1]
 * @return Stomata resistance
 */
double CropGrowth::get_StomataResistance() const
{
	return vc_StomataResistance;
}

/**
 * @brief Returns transpiration per layer[mm]
 * @return transpiration per layer
 */
double CropGrowth::get_PotentialTranspiration() const
{
	return vc_PotentialTranspiration;
}

/**
 * @brief Returns transpiration per layer[mm]
 * @return transpiration per layer
 */
double CropGrowth::get_ActualTranspiration() const
{
	return vc_ActualTranspiration;
}

/**
 * @brief Returns transpiration per layer[mm]
 * @return transpiration per layer
 */
double CropGrowth::get_Transpiration(int i_Layer) const
{
	return vc_Transpiration[i_Layer];
}

/**
 * @brief Returns transpiration deficit [0;1]
 * @return transpiration deficit
 */
double CropGrowth::get_TranspirationDeficit() const
{
	return vc_TranspirationDeficit;
}

/**
 * @brief Returns oxygen deficit [0;1]
 * @return oxygen deficit
 */
double CropGrowth::get_OxygenDeficit() const
{
	return vc_OxygenDeficit;
}

/**
 * @brief Returns Nitrogen deficit [0;1]
 * @return nitrogen deficit
 */
double CropGrowth::get_CropNRedux() const
{
	return vc_CropNRedux;
}

/**
 * @brief Returns Heat stress reductor [0;1]
 * @return heat stress reductor
 */
double CropGrowth::get_HeatStressRedux() const
{
	return vc_CropHeatRedux;
}

double CropGrowth::get_FrostStressRedux() const
{
	return vc_CropFrostRedux;
}

/**
 * @brief Returns current total temperature sum [°Cd]
 * @return Current temperature sum
 */
double CropGrowth::get_CurrentTemperatureSum() const
{
	return vc_CurrentTotalTemperatureSum;
}

/**
 * @brief Returns developmental stage[]
 * @return deveklopmental stage
 */
int CropGrowth::get_DevelopmentalStage() const
{
	return vc_DevelopmentalStage;
}

/**
 * @brief Returns Relative total development []
 * @return Relative total development
 */
double CropGrowth::get_RelativeTotalDevelopment() const
{
	return vc_RelativeTotalDevelopment;
}

/**
* @brief Returns total number of organs[]
* @return total number of organs
*/
int CropGrowth::get_NumberOfOrgans() const
{
	return pc_NumberOfOrgans;
}

/**
 * @brief Returns current biomass of organ i [kg ha-1]
 * @return organ biomass
 */
double CropGrowth::get_OrganBiomass(int i_Organ) const
{
	return vc_OrganBiomass[i_Organ];
}

/**
 * @brief Returns aboveground biomass [kg ha-1]
 * @return organ biomass
 */
double CropGrowth::get_AbovegroundBiomass() const
{
	return vc_AbovegroundBiomass;
}

/**
* @brief Returns crop's lethal temperature LT50 [°C]
* @return LT50
*/
double CropGrowth::get_LT50() const
{
	return vc_LT50;
}

/**
 * @brief Returns crop N uptake from layer i [kg N ha-1]
 * @return Crop N uptake
 */
double CropGrowth::get_NUptakeFromLayer(int i_Layer) const
{
	return vc_NUptakeFromLayer[i_Layer];
}

/**
 * @brief Returns total crop biomass [kg ha-1]
 * @return Total crop biomass
 */
double CropGrowth::get_TotalBiomass() const
{
	return vc_TotalBiomass;
}

/**
* @brief Returns total crop N content [kg N ha-1]
* @return Total crop N uptake
*/
double CropGrowth::get_TotalBiomassNContent() const
{
	return vc_TotalBiomassNContent;
}

/**
 * @brief Returns aboveground biomass N content [kg N ha-1]
 * @return organ biomass
 */
double CropGrowth::get_AbovegroundBiomassNContent() const
{
	return vc_AbovegroundBiomass * vc_NConcentrationAbovegroundBiomass;
}


/**
* @brief Returns fruit biomass N concentration [kg N kg DM]
* @return organ biomass
*/
double CropGrowth::get_FruitBiomassNConcentration() const
{
	return (vc_TotalBiomassNContent -
		(get_OrganBiomass(0) * get_RootNConcentration())) /
					(get_OrganBiomass(3) + (pc_ResidueNRatio *
		(vc_TotalBiomass - get_OrganBiomass(0) - get_OrganBiomass(3))));
}

/**
* @brief Returns fruit biomass N content [kg N ha-1]
* @return organ biomass
*/
double CropGrowth::get_FruitBiomassNContent() const
{
	return (get_OrganBiomass(3) * get_FruitBiomassNConcentration());
}

/**
 * @brief Returns root N content [kg N kg-1]
 * @return Root N content
 */
double CropGrowth::get_RootNConcentration() const
{
	return vc_NConcentrationRoot;
}


/**
 * @brief Returns target N content [kg N kg-1]
 * @return Target N content
 */
double CropGrowth::get_TargetNConcentration() const
{
	return vc_TargetNConcentration;
}

/**
 * @brief Returns critical N Content [kg N kg-1]
 * @return Critical N content
 */
double CropGrowth::get_CriticalNConcentration() const
{
	return vc_CriticalNConcentration;
}

/**
 * @brief Returns above-ground biomass N concentration [kg N kg-1]
 * @return Above-ground biomass N concentration
 */
double CropGrowth::get_AbovegroundBiomassNConcentration() const
{
	return vc_NConcentrationAbovegroundBiomass;
}

/**
 * @brief Returns heat sum for irrigation start [°C d]
 * @return heat sum for irrigation start
 */
double CropGrowth::get_HeatSumIrrigationStart() const
{
	return pc_HeatSumIrrigationStart;
}

/**
 * @brief Returns heat sum for irrigation end [°C d]
 * @return heat sum for irrigation end
 */
double CropGrowth::get_HeatSumIrrigationEnd() const
{
	return pc_HeatSumIrrigationEnd;
}

/**
 * @brief Returns number of above ground organs
 * @return number of above ground organs
 */
int CropGrowth::pc_NumberOfAbovegroundOrgans() const
{
	int count = 0;
	for(int i = 0, size = pc_AbovegroundOrgan.size(); i < size; i++)
	{
		if(pc_AbovegroundOrgan[i])
		{
			count++;
		}
	}
	return count;
}

namespace
{

	typedef vector<Monica::YieldComponent> VYC;

	/**
 * @brief Returns crop yield.
 *
 * @param v Vector yield component
 * @param bmv
 */
	double _cropYield(const VYC& v, const vector<double>& bmv)
	{
		double yield = 0;
		for(VYC::const_iterator ci = v.begin(); ci != v.end(); ci++)
		{
			yield += bmv.at(ci->organId - 1) * (ci->yieldPercentage);
		}
		return yield;
	}

	/**
 * @brief Returns crop yield.
 *
 * @param v Vector yield component
 * @param bmv
 */
	double _cropFreshMatterYield(const VYC& v, const vector<double>& bmv)
	{
		double freshMatterYield = 0;
		for(VYC::const_iterator ci = v.begin(); ci != v.end(); ci++)
		{
			freshMatterYield += bmv.at(ci->organId - 1) * (ci->yieldPercentage) / (ci->yieldDryMatter);
		}

		return freshMatterYield;
	}
} // namespace

/**
 * @brief Returns primary crop yield
 * @return primary yield
 */
double CropGrowth::get_PrimaryCropYield() const
{

	if(eva2_usage == NUTZUNG_GANZPFLANZE)
	{
		return _cropYield(eva2_primaryYieldComponents, vc_OrganBiomass);
	}

	return _cropYield(pc_OrganIdsForPrimaryYield, vc_OrganBiomass);
}

/**
 * @brief Returns secondary crop yield
 * @return crop yield
 */
double CropGrowth::get_SecondaryCropYield() const
{
	if(eva2_usage == NUTZUNG_GANZPFLANZE || eva2_usage == NUTZUNG_GRUENDUENGUNG)
	{
		return _cropYield(eva2_secondaryYieldComponents, vc_OrganBiomass);
	}
	return _cropYield(pc_OrganIdsForSecondaryYield, vc_OrganBiomass);
}

/**
* @brief Returns crop yield after cutting
* @return crop yield after cutting
*/
double CropGrowth::get_CropYieldAfterCutting() const
{

	if(eva2_usage == NUTZUNG_GANZPFLANZE)
	{
		return _cropYield(eva2_primaryYieldComponents, vc_OrganBiomass);
	}

	return _cropYield(pc_OrganIdsForCutting, vc_OrganBiomass);
}

/**
 * @brief Returns primary crop yield fresh matter
 * @return primary yield
 */
double CropGrowth::get_FreshPrimaryCropYield() const
{
	if(eva2_usage == NUTZUNG_GANZPFLANZE)
	{
		return _cropFreshMatterYield(eva2_primaryYieldComponents, vc_OrganBiomass);
	}
	return _cropFreshMatterYield(pc_OrganIdsForPrimaryYield, vc_OrganBiomass);
}

/**
 * @brief Returns secondary crop yield fresh matter
 * @return crop yield
 */
double CropGrowth::get_FreshSecondaryCropYield() const
{
	if(eva2_usage == NUTZUNG_GANZPFLANZE || eva2_usage == NUTZUNG_GRUENDUENGUNG)
	{
		return _cropFreshMatterYield(eva2_secondaryYieldComponents, vc_OrganBiomass);
	}
	return _cropFreshMatterYield(pc_OrganIdsForSecondaryYield, vc_OrganBiomass);
}

/**
* @brief Returns fresh matter crop yield after cutting
* @return fresh crop yield after cutting
*/
double CropGrowth::get_FreshCropYieldAfterCutting() const
{

	if(eva2_usage == NUTZUNG_GANZPFLANZE)
	{
		return _cropFreshMatterYield(eva2_primaryYieldComponents, vc_OrganBiomass);
	}

	return _cropFreshMatterYield(pc_OrganIdsForCutting, vc_OrganBiomass);
}

/**
 * @brief Returns residue biomass
 * @return residue biomass
 */
double CropGrowth::get_ResidueBiomass(bool useSecondaryCropYields) const
{
	return vc_TotalBiomass - get_OrganBiomass(0) - get_PrimaryCropYield()
		- (useSecondaryCropYields ? get_SecondaryCropYield() : 0);
}

/**
 * @brief Returns residue N concentration [kg kg-1]
 * @return residue N concentration
 */
double CropGrowth::get_ResiduesNConcentration() const
{
	return (vc_TotalBiomassNContent -
		(get_OrganBiomass(0) * get_RootNConcentration())) /
					((get_PrimaryCropYield() / pc_ResidueNRatio) +
		(vc_TotalBiomass - get_OrganBiomass(0) - get_PrimaryCropYield()));
}

/**
 * @brief Returns primary yield N concentration [kg kg-1]
 * @return primary yield N concentration
 */
double CropGrowth::get_PrimaryYieldNConcentration() const
{
	return (vc_TotalBiomassNContent -
		(get_OrganBiomass(0) * get_RootNConcentration())) /
					(get_PrimaryCropYield() + (pc_ResidueNRatio *
		(vc_TotalBiomass - get_OrganBiomass(0) - get_PrimaryCropYield())));
}

double CropGrowth::get_ResiduesNContent(bool useSecondaryCropYields) const
{
	return (get_ResidueBiomass(useSecondaryCropYields) * get_ResiduesNConcentration());
}

double CropGrowth::get_PrimaryYieldNContent() const
{
	return (get_PrimaryCropYield() * get_PrimaryYieldNConcentration());
}

double CropGrowth::get_RawProteinConcentration() const
{
	// Assuming an average N concentration of raw protein of 16%
	return (get_PrimaryYieldNConcentration() * 6.25);
}

double CropGrowth::get_SecondaryYieldNContent() const
{
	return (get_SecondaryCropYield() * get_ResiduesNConcentration());
}

/**
 * @brief Returns the accumulated crop's actual N uptake [kg N ha-1]
 * @return Sum crop actual N uptake
 */
double CropGrowth::get_SumTotalNUptake() const
{
	return vc_SumTotalNUptake;
}

/**
 * @brief Returns the crop's actual N uptake [kg N ha-1]
 * @return Actual N uptake
 */
double CropGrowth::get_ActNUptake() const
{
	return vc_TotalNUptake;
}

/**
 * @brief Returns the crop's potential N uptake [kg N ha-1]
 * @return Potential N uptake
 */
double CropGrowth::get_PotNUptake() const
{
	return vc_CropNDemand * 10000.0;
}

/**
 * @brief Returns the crop's N input via atmospheric fixation [kg N ha-1]
 * @return Biological N fixation
 */
double CropGrowth::get_BiologicalNFixation() const
{
	return vc_FixedN;
}

/**
 * @brief Returns the gross primary production [kg C ha-1 d-1]
 * @return Gross primary production
 */
double CropGrowth::get_GrossPrimaryProduction() const
{
	return vc_GrossPrimaryProduction;
}

/**
 * @brief Returns the net primary production [kg C ha-1 d-1]
 * @return Net primary production
 */
double CropGrowth::get_NetPrimaryProduction() const
{
	return vc_NetPrimaryProduction;
}

/**
 * @brief Returns the respiration [kg C ha-1 d-1]
 * @return Net primary production
 */
double CropGrowth::get_AutotrophicRespiration() const
{
	return vc_TotalRespired / 30.0 * 12.0;;  // Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
}

/**
 * Returns the individual respiration of the organs [kg C ha-1 d-1]
 * based on the current ratio of the crop's biomass.
 */
double CropGrowth::get_OrganSpecificTotalRespired(int organ)  const
{
	// get total amount of actual biomass
	double total_biomass = totalBiomass();

	// get biomass of specific organ and calculates ratio
	double organ_percentage = get_OrganBiomass(organ) / total_biomass;
	return (get_AutotrophicRespiration() * organ_percentage);
}


/**
 * @brief Returns the organ-specific net primary production [kg C ha-1 d-1]
 * @return Organ-specific net primary production
 */
double CropGrowth::get_OrganSpecificNPP(int organ)  const
{
	// get total amount of actual biomass
	double total_biomass = totalBiomass();

	// get biomass of specific organ and calculates ratio
	double organ_percentage = get_OrganBiomass(organ) / total_biomass;

	//cout << "get_OrganBiomass(organ) : " << organ << ", " << organ_percentage << std::endl; // JV!
	//cout << "total_biomass : " << total_biomass << std::endl; // JV!
	return (get_NetPrimaryProduction() * organ_percentage);
}

int CropGrowth::get_StageAfterCut() const
{
	return pc_StageAfterCut - 1;
}

void CropGrowth::applyCutting()
{
	double old_above_biomass = vc_AbovegroundBiomass;
	double removing_biomass = 0.0;

	debug() << "CropGrowth::applyCutting()" << endl;
	std::vector<double> new_OrganBiomass;      //! old WORG
	for(int organ = 1; organ < pc_NumberOfOrgans + 1; organ++)
	{

		int cut_organ_count = pc_OrganIdsForCutting.size();
		double biomasse = vc_OrganBiomass.at(organ - 1);
		debug() << "Alte Biomasse: " << biomasse << "\tOrgan: " << organ << endl;
		for(int cut_organ = 0; cut_organ < cut_organ_count; cut_organ++)
		{

			YieldComponent yc = YieldComponent(pc_OrganIdsForCutting.at(cut_organ));

			if(organ == yc.organId)
			{
				debug() << "YC yc.yieldPercentage: " << yc.yieldPercentage << endl;
				biomasse = vc_OrganBiomass.at(organ - 1) * ((1 - yc.yieldPercentage));
				vc_AbovegroundBiomass -= biomasse;

				removing_biomass += biomasse;
			}

		}
		new_OrganBiomass.push_back(biomasse);
		debug() << "Neue Biomasse: " << biomasse << endl;
	}


	vc_TotalBiomassNContent = (removing_biomass / old_above_biomass) * vc_TotalBiomassNContent;


	vc_OrganBiomass = new_OrganBiomass;

	// reset stage and temperature some after cutting
	int stage_after_cutting = pc_StageAfterCut - 1;
	for(int stage = stage_after_cutting; stage < pc_NumberOfDevelopmentalStages; stage++)
	{
		vc_CurrentTemperatureSum[stage] = 0.0;
	}
	vc_CurrentTotalTemperatureSum = 0.0;
	vc_DevelopmentalStage = stage_after_cutting;
	vc_CuttingDelayDays = pc_CuttingDelayDays;
	pc_MaxAssimilationRate = pc_MaxAssimilationRate * 0.9;

}

void
CropGrowth::applyFruitHarvest(double yieldPercentage)
{

	double old_above_biomass = vc_AbovegroundBiomass;
	double removing_biomass = 0.0;
	double residues = 0.0;

	debug() << "CropGrowth::applyFruitHarvest()" << endl;
	std::vector<double> new_OrganBiomass;

	double fruitBiomass = vc_OrganBiomass.at(3);
	debug() << "Old fruit biomass: " << fruitBiomass << endl;
	debug() << "Yield percentage: " << yieldPercentage << endl;
	fruitBiomass = vc_OrganBiomass.at(3) * yieldPercentage;
	vc_AbovegroundBiomass -= fruitBiomass;
	removing_biomass += fruitBiomass;
	residues = vc_OrganBiomass.at(3) * (1.0 - yieldPercentage);
	vc_OrganBiomass.at(3) = 0.0;

	new_OrganBiomass.push_back(fruitBiomass);
	debug() << "New fruit biomass: " << fruitBiomass << endl;

	vc_TotalBiomassNContent = (removing_biomass / old_above_biomass) * vc_TotalBiomassNContent;

	vc_OrganBiomass = new_OrganBiomass;

	// reset developmental stage and temperature sum after harvest
	for(int stage = 0; stage < pc_NumberOfDevelopmentalStages; stage++)
	{
		vc_CurrentTemperatureSum[stage] = 0.0;
	}
	vc_CurrentTotalTemperatureSum = 0.0;
	vc_DevelopmentalStage = 0;

	pc_MaxAssimilationRate = pc_MaxAssimilationRate * 0.9;

}

double
CropGrowth::get_AccumulatedETa() const
{
	return vc_AccumulatedETa;
}

double
CropGrowth::get_AccumulatedTranspiration() const
{
	return vc_AccumulatedTranspiration;
}

double
CropGrowth::get_AccumulatedPrimaryCropYield() const
{
	return vc_AccumulatedPrimaryCropYield;
}


/**
 * Returns the depth of the maximum active and effective root.
 * [m]
 */
double
CropGrowth::getEffectiveRootingDepth() const
{
	size_t nols = soilColumn.vs_NumberOfLayers();

	for(size_t i_Layer = 0; i_Layer < nols; i_Layer++)
		if(vc_RootEffectivity[i_Layer] == 0.0)
			return (i_Layer + 1) / 10.0;

	return (nols + 1) / 10.0;
}

/**
* @brief Setter for crop parameters of perennial crops after the transplant season.
* @sets crop parameters of perennial crops after the transplant season
*/
void CropGrowth::fc_UpdateCropParametersForPerennial()
{
	pc_AbovegroundOrgan = perennialCropParams->speciesParams.pc_AbovegroundOrgan;
	pc_AssimilatePartitioningCoeff = perennialCropParams->cultivarParams.pc_AssimilatePartitioningCoeff;
	pc_AssimilateReallocation = perennialCropParams->speciesParams.pc_AssimilateReallocation;
	pc_BaseDaylength = perennialCropParams->cultivarParams.pc_BaseDaylength;
	pc_BaseTemperature = perennialCropParams->speciesParams.pc_BaseTemperature;
	pc_BeginSensitivePhaseHeatStress = perennialCropParams->cultivarParams.pc_BeginSensitivePhaseHeatStress;
	pc_CarboxylationPathway = perennialCropParams->speciesParams.pc_CarboxylationPathway;
	pc_CriticalOxygenContent = perennialCropParams->speciesParams.pc_CriticalOxygenContent;
	pc_CriticalTemperatureHeatStress = perennialCropParams->cultivarParams.pc_CriticalTemperatureHeatStress;
	pc_CropHeightP1 = perennialCropParams->cultivarParams.pc_CropHeightP1;
	pc_CropHeightP2 = perennialCropParams->cultivarParams.pc_CropHeightP2;
	pc_CropName = perennialCropParams->pc_CropName();
	pc_CropSpecificMaxRootingDepth = perennialCropParams->cultivarParams.pc_CropSpecificMaxRootingDepth;
	pc_DaylengthRequirement = perennialCropParams->cultivarParams.pc_DaylengthRequirement;
	pc_DefaultRadiationUseEfficiency = perennialCropParams->speciesParams.pc_DefaultRadiationUseEfficiency;
	pc_DevelopmentAccelerationByNitrogenStress = perennialCropParams->speciesParams.pc_DevelopmentAccelerationByNitrogenStress;
	pc_DroughtStressThreshold = perennialCropParams->cultivarParams.pc_DroughtStressThreshold;
	pc_DroughtImpactOnFertilityFactor = perennialCropParams->speciesParams.pc_DroughtImpactOnFertilityFactor;
	pc_EndSensitivePhaseHeatStress = perennialCropParams->cultivarParams.pc_EndSensitivePhaseHeatStress;
	pc_PartBiologicalNFixation = perennialCropParams->speciesParams.pc_PartBiologicalNFixation;
	pc_InitialKcFactor = perennialCropParams->speciesParams.pc_InitialKcFactor;
	pc_InitialOrganBiomass = perennialCropParams->speciesParams.pc_InitialOrganBiomass;
	pc_InitialRootingDepth = perennialCropParams->speciesParams.pc_InitialRootingDepth;
	pc_LimitingTemperatureHeatStress = perennialCropParams->speciesParams.pc_LimitingTemperatureHeatStress;
	pc_LuxuryNCoeff = perennialCropParams->speciesParams.pc_LuxuryNCoeff;
	pc_MaxAssimilationRate = perennialCropParams->cultivarParams.pc_MaxAssimilationRate;
	pc_MaxCropDiameter = perennialCropParams->speciesParams.pc_MaxCropDiameter;
	pc_MaxCropHeight = perennialCropParams->cultivarParams.pc_MaxCropHeight;
	pc_MaxNUptakeParam = perennialCropParams->speciesParams.pc_MaxNUptakeParam;
	pc_MinimumNConcentration = perennialCropParams->speciesParams.pc_MinimumNConcentration;
	pc_MinimumTemperatureForAssimilation = perennialCropParams->speciesParams.pc_MinimumTemperatureForAssimilation;
	pc_MinimumTemperatureRootGrowth = perennialCropParams->speciesParams.pc_MinimumTemperatureRootGrowth;
	pc_NConcentrationAbovegroundBiomass = perennialCropParams->speciesParams.pc_NConcentrationAbovegroundBiomass;
	pc_NConcentrationB0 = perennialCropParams->speciesParams.pc_NConcentrationB0;
	pc_NConcentrationPN = perennialCropParams->speciesParams.pc_NConcentrationPN;
	pc_NConcentrationRoot = perennialCropParams->speciesParams.pc_NConcentrationRoot;
	pc_NumberOfDevelopmentalStages = perennialCropParams->speciesParams.pc_NumberOfDevelopmentalStages();
	pc_NumberOfOrgans = perennialCropParams->speciesParams.pc_NumberOfOrgans();
	pc_OptimumTemperature = perennialCropParams->cultivarParams.pc_OptimumTemperature;
	pc_OrganGrowthRespiration = perennialCropParams->speciesParams.pc_OrganGrowthRespiration;
	pc_OrganMaintenanceRespiration = perennialCropParams->speciesParams.pc_OrganMaintenanceRespiration;
	pc_OrganSenescenceRate = perennialCropParams->cultivarParams.pc_OrganSenescenceRate;
	pc_Perennial = perennialCropParams->cultivarParams.pc_Perennial;
	pc_PlantDensity = perennialCropParams->speciesParams.pc_PlantDensity;
	pc_ResidueNRatio = perennialCropParams->cultivarParams.pc_ResidueNRatio;
	pc_RootDistributionParam = perennialCropParams->speciesParams.pc_RootDistributionParam;
	pc_RootFormFactor = perennialCropParams->speciesParams.pc_RootFormFactor;
	pc_RootGrowthLag = perennialCropParams->speciesParams.pc_RootGrowthLag;
	pc_RootPenetrationRate = perennialCropParams->speciesParams.pc_RootPenetrationRate;
	pc_SpecificLeafArea = perennialCropParams->cultivarParams.pc_SpecificLeafArea;
	pc_SpecificRootLength = perennialCropParams->speciesParams.pc_SpecificRootLength;
	pc_StageAtMaxDiameter = perennialCropParams->speciesParams.pc_StageAtMaxDiameter;
	pc_StageAtMaxHeight = perennialCropParams->speciesParams.pc_StageAtMaxHeight;
	pc_StageMaxRootNConcentration = perennialCropParams->speciesParams.pc_StageMaxRootNConcentration;
	pc_StageKcFactor = perennialCropParams->cultivarParams.pc_StageKcFactor;
	pc_StageTemperatureSum = perennialCropParams->cultivarParams.pc_StageTemperatureSum;
	pc_StorageOrgan = perennialCropParams->speciesParams.pc_StorageOrgan;
	pc_VernalisationRequirement = perennialCropParams->cultivarParams.pc_VernalisationRequirement;
}

/**
* @brief Test if anthesis state is reached
* @return True, if anthesis is reached, false otherwise.
*
* Method is called after calculation of the developmental stage.
*/
bool CropGrowth::isAnthesisDay(int old_dev_stage, int new_dev_stage)
{
	if(pc_NumberOfDevelopmentalStages == 6)
	{
		return (old_dev_stage == 4 && new_dev_stage == 5);
	}
	else if(pc_NumberOfDevelopmentalStages == 7)
	{
		return (old_dev_stage == 5 && new_dev_stage == 6);
	}

	return false;
}

/**
* @brief Test if maturity state is reached
* @return True, if maturity is reached, false otherwise.
*
* Method is called after calculation of the developmental stage.
*/
bool CropGrowth::isMaturityDay(int old_dev_stage, int new_dev_stage)
{
	// corn crops
	if(pc_NumberOfDevelopmentalStages == 6)
	{
		return (old_dev_stage == 5 && new_dev_stage == 6);
	}
	// maize, sorghum and other crops with 7 developmental stages
	else if(pc_NumberOfDevelopmentalStages == 7)
	{
		return (old_dev_stage == 6 && new_dev_stage == 7);
	}

	return false;
}

/**
 * @brief Getter for anthesis day.
 * @return Julian day of crop's anthesis
 */
int
CropGrowth::getAnthesisDay() const
{
	//cout << "Getter anthesis " << vc_AnthesisDay << endl;
	return vc_AnthesisDay;
}

/**
* @brief Getter for maturity day.
* @return Julian day of crop's maturity.
*/
int
CropGrowth::getMaturityDay() const
{
	//cout << "Getter maturity " << vc_MaturityDay << endl;
	return vc_MaturityDay;
}


bool
CropGrowth::maturityReached() const
{
	debug() << "vc_MaturityReached: " << vc_MaturityReached << endl;
	return vc_MaturityReached;
}
