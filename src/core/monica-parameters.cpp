/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
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

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <mutex>

#include "db/abstract-db-connections.h"
#include "climate/climate-common.h"
#include "tools/helper.h"
#include "tools/json11-helper.h"
#include "tools/algorithms.h"
#include "monica-parameters.h"
#include "monica.h"
#include "tools/debug.h"
#include "soil/conversion.h"
#include "soil/soil.h"
#include "../io/database-io.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Soil;
using namespace Tools;
using namespace Climate;

//------------------------------------------------------------------------------

vector<ResultId> Monica::cropResultIds()
{
  return {
    primaryYield, secondaryYield, sumFertiliser,
    sumIrrigation, anthesisDay, maturityDay, harvestDay//, sumMineralisation
  };
}

pair<string, string> Monica::nameAndUnitForResultId(ResultId rid)
{
  switch(rid)
  {
  case primaryYield: return make_pair("Primär-Ertrag", "dt/ha");
  case secondaryYield: return make_pair("Sekundär-Ertrag", "dt/ha");
  case sumFertiliser: return make_pair("N-Düngung", "kg/ha");
  case sumIrrigation: return make_pair("Beregnungswasser", "mm/ha");
  }
  return make_pair("", "");
}

//------------------------------------------------------------------------------

vector<ResultId> Monica::monthlyResultIds()
{
  return {
    avg10cmMonthlyAvgCorg, avg30cmMonthlyAvgCorg,
    mean90cmMonthlyAvgWaterContent,
    monthlySumGroundWaterRecharge, monthlySumNLeaching};
}

//------------------------------------------------------------------------------

vector<int> Monica::CCGermanyResultIds()
{
  return {
    primaryYield,                   // done
    yearlySumGroundWaterRecharge,
    yearlySumNLeaching};
}

//------------------------------------------------------------------------------

vector<int> Monica::eva2CropResultIds()
{
  return {
    cropname,
    primaryYieldTM,
    secondaryYieldTM,
    sumFertiliser,
    sumETaPerCrop,
    biomassNContent,
    daysWithCrop,
    aboveBiomassNContent,
    NStress,
    WaterStress,
    HeatStress,
    OxygenStress};
}

//------------------------------------------------------------------------------

vector<int> Monica::eva2MonthlyResultIds()
{
	return{ 
		avg10cmMonthlyAvgCorg,
		avg30cmMonthlyAvgCorg,
		mean90cmMonthlyAvgWaterContent,
    monthlySumGroundWaterRecharge,
    monthlySumNLeaching,
    monthlySurfaceRunoff,
    monthlyPrecip,
    monthlyETa,
    monthlySoilMoistureL0,
    monthlySoilMoistureL1,
    monthlySoilMoistureL2,
    monthlySoilMoistureL3,
    monthlySoilMoistureL4,
    monthlySoilMoistureL5,
    monthlySoilMoistureL6,
    monthlySoilMoistureL7,
    monthlySoilMoistureL8,
    monthlySoilMoistureL9,
    monthlySoilMoistureL10,
    monthlySoilMoistureL11,
    monthlySoilMoistureL12,
    monthlySoilMoistureL13,
    monthlySoilMoistureL14,
    monthlySoilMoistureL15,
    monthlySoilMoistureL16,
    monthlySoilMoistureL17,
    monthlySoilMoistureL18};
}

//------------------------------------------------------------------------------

/**
 * Returns some information about a result id.
 * @param rid ResultID of interest
 * @return ResultIdInfo Information object of result ids
 */
ResultIdInfo Monica::resultIdInfo(ResultId rid)
{
  switch(rid)
  {
  case primaryYield:
    return ResultIdInfo("Hauptertrag", "dt/ha", "primYield");
  case secondaryYield:
    return ResultIdInfo("Nebenertrag", "dt/ha", "secYield");
  case aboveGroundBiomass:
    return ResultIdInfo("Oberirdische Biomasse", "dt/ha", "AbBiom");
  case anthesisDay:
	  return ResultIdInfo("Tag der Blüte", "Jul. day", "anthesisDay");
  case maturityDay:
	  return ResultIdInfo("Tag der Reife", "Jul. day", "maturityDay");
  case harvestDay:
      return ResultIdInfo("Tag der Ernte", "Date", "harvestDay");
  case sumFertiliser:
    return ResultIdInfo("N", "kg/ha", "sumFert");
  case sumIrrigation:
    return ResultIdInfo("Beregnungswassermenge", "mm/ha", "sumIrrig");
  case sumMineralisation:
    return ResultIdInfo("Mineralisation", "????", "sumMin");
  case avg10cmMonthlyAvgCorg:
    return ResultIdInfo("Kohlenstoffgehalt 0-10cm", "% kg C/kg Boden", "Corg10cm");
  case avg30cmMonthlyAvgCorg:
    return ResultIdInfo("Kohlenstoffgehalt 0-30cm", "% kg C/kg Boden", "Corg30cm");
  case mean90cmMonthlyAvgWaterContent:
    return ResultIdInfo("Bodenwassergehalt 0-90cm", "%nFK", "Moist90cm");
  case sum90cmYearlyNatDay:
    return ResultIdInfo("Boden-Nmin-Gehalt 0-90cm am 31.03.", "kg N/ha", "Nmin3103");
  case monthlySumGroundWaterRecharge:
    return ResultIdInfo("Grundwasserneubildung", "mm", "GWRech");
  case monthlySumNLeaching:
    return ResultIdInfo("N-Auswaschung", "kg N/ha", "monthLeachN");
  case cropHeight:
    return ResultIdInfo("Pflanzenhöhe zum Erntezeitpunkt", "m","cropHeight");
  case sum90cmYearlyNO3AtDay:
    return ResultIdInfo("Summe Nitratkonzentration in 0-90cm Boden am 31.03.", "kg N/ha","NO3_90cm");
  case sum90cmYearlyNH4AtDay:
    return ResultIdInfo("Ammoniumkonzentratio in 0-90cm Boden am 31.03.", "kg N/ha", "NH4_90cm");
  case maxSnowDepth:
    return ResultIdInfo("Maximale Schneetiefe während der Simulation","m","maxSnowDepth");
  case sumSnowDepth:
    return ResultIdInfo("Akkumulierte Schneetiefe der gesamten Simulation", "m","sumSnowDepth");
  case sumFrostDepth:
    return ResultIdInfo("Akkumulierte Frosttiefe der gesamten Simulation","m","sumFrostDepth");
  case avg30cmSoilTemperature:
    return ResultIdInfo("Durchschnittliche Bodentemperatur in 0-30cm Boden am 31.03.", "°C","STemp30cm");
  case sum30cmSoilTemperature:
    return ResultIdInfo("Akkumulierte Bodentemperature der ersten 30cm des Bodens am 31.03", "°C","sumSTemp30cm");
  case avg0_30cmSoilMoisture:
    return ResultIdInfo("Durchschnittlicher Wassergehalt in 0-30cm Boden am 31.03.", "%","Moist0_30");
  case avg30_60cmSoilMoisture:
    return ResultIdInfo("Durchschnittlicher Wassergehalt in 30-60cm Boden am 31.03.", "%","Moist30_60");
  case avg60_90cmSoilMoisture:
    return ResultIdInfo("Durchschnittlicher Wassergehalt in 60-90cm Boden am 31.03.", "%","Moist60_90");
  case avg0_90cmSoilMoisture:
    return ResultIdInfo("Durchschnittlicher Wassergehalt in 0-90cm Boden am 31.03.", "%","Moist0_90");
  case waterFluxAtLowerBoundary:
    return ResultIdInfo("Sickerwasser der unteren Bodengrenze am 31.03.", "mm/d", "waterFlux");
  case avg0_30cmCapillaryRise:
    return ResultIdInfo("Durchschnittlicher kapillarer Aufstieg in 0-30cm Boden am 31.03.", "mm/d", "capRise0_30");
  case avg30_60cmCapillaryRise:
    return ResultIdInfo("Durchschnittlicher kapillarer Aufstieg in 30-60cm Boden am 31.03.", "mm/d", "capRise30_60");
  case avg60_90cmCapillaryRise:
    return ResultIdInfo("Durchschnittlicher kapillarer Aufstieg in 60-90cm Boden am 31.03.", "mm/d", "capRise60_90");
  case avg0_30cmPercolationRate:
    return ResultIdInfo("Durchschnittliche Durchflussrate in 0-30cm Boden am 31.03.", "mm/d", "percRate0_30");
  case avg30_60cmPercolationRate:
    return ResultIdInfo("Durchschnittliche Durchflussrate in 30-60cm Boden am 31.03.", "mm/d", "percRate30_60");
  case avg60_90cmPercolationRate:
    return ResultIdInfo("Durchschnittliche Durchflussrate in 60-90cm Boden am 31.03.", "mm/d", "percRate60_90");
  case sumSurfaceRunOff:
    return ResultIdInfo("Summe des Oberflächenabflusses der gesamten Simulation", "mm", "sumSurfRunOff");
  case evapotranspiration:
    return ResultIdInfo("Evaporatranspiration am 31.03.", "mm", "ET");
  case transpiration:
    return ResultIdInfo("Transpiration am 31.03.", "mm", "transp");
  case evaporation:
    return ResultIdInfo("Evaporation am 31.03.", "mm", "evapo");
  case biomassNContent:
    return ResultIdInfo("Stickstoffanteil im Erntegut", "kg N/ha", "biomNContent");
  case aboveBiomassNContent:
    return ResultIdInfo("Stickstoffanteil in der gesamten oberirdischen Biomasse", "kg N/ha", "aboveBiomassNContent");
  case sumTotalNUptake:
    return ResultIdInfo("Summe des aufgenommenen Stickstoffs", "kg/ha", "sumNUptake");
  case sum30cmSMB_CO2EvolutionRate:
    return ResultIdInfo("SMB-CO2 Evolutionsrate in 0-30cm Boden am 31.03.", "kg/ha", "sumSMB_CO2_EvRate");
  case NH3Volatilised:
    return ResultIdInfo("Menge des verdunstenen Stickstoffs (NH3) am 31.03.", "kg N / m2 d", "NH3Volat");
  case sumNH3Volatilised:
    return ResultIdInfo("Summe des verdunstenen Stickstoffs (NH3) des gesamten Simulationszeitraums", "kg N / m2", "sumNH3Volat");
  case sum30cmActDenitrificationRate:
    return ResultIdInfo("Summe der Denitrifikationsrate in 0-30cm Boden am 31.03.", "kg N / m3 d", "denitRate");
  case leachingNAtBoundary:
    return ResultIdInfo("Menge des ausgewaschenen Stickstoffs im Boden am 31.03.", "kg / ha", "leachN");
  case yearlySumGroundWaterRecharge:
    return ResultIdInfo("Gesamt-akkumulierte Grundwasserneubildung im Jahr", "mm", "Yearly_GWRech");
  case yearlySumNLeaching:
    return ResultIdInfo("Gesamt-akkumulierte N-Auswaschung im Jahr", "kg N/ha", "Yearly_monthLeachN");
  case sumETaPerCrop:
    return ResultIdInfo("Evapotranspiration pro Vegetationszeit der Pflanze", "mm", "ETa_crop");
  case sumTraPerCrop:
	  return ResultIdInfo("Transpiration pro Vegetationszeit der Pflanze", "mm", "Tra_crop");
  case cropname:
    return ResultIdInfo("Pflanzenname", "", "cropname");
  case primaryYieldTM:
    return ResultIdInfo("Hauptertrag in TM", "dt TM/ha", "primYield");
  case secondaryYieldTM:
    return ResultIdInfo("Nebenertrag in TM", "dt TM/ha", "secYield");
  case soilMoist0_90cmAtHarvest:
	  return ResultIdInfo("Wassergehalt zur Ernte in 0-90cm", "%", "moist90Harvest");
  case corg0_30cmAtHarvest:
	  return ResultIdInfo("Corg-Gehalt zur Ernte in 0-30cm", "% kg C/kg Boden", "corg30Harvest");
  case nmin0_90cmAtHarvest:
	  return ResultIdInfo("Nmin zur Ernte in 0-90cm", "kg N/ha", "nmin90Harvest");
  case monthlySurfaceRunoff:
    return ResultIdInfo("Monatlich akkumulierte Oberflächenabfluss", "mm", "monthlySurfaceRunoff");
  case monthlyPrecip:
    return ResultIdInfo("Akkumulierte korrigierte  Niederschläge pro Monat", "mm", "monthlyPrecip");
  case monthlyETa:
    return ResultIdInfo("Akkumulierte korrigierte Evapotranspiration pro Monat", "mm", "monthlyETa");
  case monthlySoilMoistureL0:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 1", "Vol-%", "monthlySoilMoisL1");
  case monthlySoilMoistureL1:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 2", "Vol-%", "monthlySoilMoisL2");
  case monthlySoilMoistureL2:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 3", "Vol-%", "monthlySoilMoisL3");
  case monthlySoilMoistureL3:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 4", "Vol-%", "monthlySoilMoisL4");
  case monthlySoilMoistureL4:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 5", "Vol-%", "monthlySoilMoisL5");
  case monthlySoilMoistureL5:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 6", "Vol-%", "monthlySoilMoisL6");
  case monthlySoilMoistureL6:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 7", "Vol-%", "monthlySoilMoisL7");
  case monthlySoilMoistureL7:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 8", "Vol-%", "monthlySoilMoisL8");
  case monthlySoilMoistureL8:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 9", "Vol-%", "monthlySoilMoisL9");
  case monthlySoilMoistureL9:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 10", "Vol-%", "monthlySoilMoisL10");
  case monthlySoilMoistureL10:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 11", "Vol-%", "monthlySoilMoisL11");
  case monthlySoilMoistureL11:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 12", "Vol-%", "monthlySoilMoisL12");
  case monthlySoilMoistureL12:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 13", "Vol-%", "monthlySoilMoisL13");
  case monthlySoilMoistureL13:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 14", "Vol-%", "monthlySoilMoisL14");
  case monthlySoilMoistureL14:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 15", "Vol-%", "monthlySoilMoisL15");
  case monthlySoilMoistureL15:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 16", "Vol-%", "monthlySoilMoisL16");
  case monthlySoilMoistureL16:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 17", "Vol-%", "monthlySoilMoisL17");
  case monthlySoilMoistureL17:
		return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 18", "Vol-%", "monthlySoilMoisL18");
  case monthlySoilMoistureL18:
    return ResultIdInfo("Monatlicher mittlerer Wassergehalt für Schicht 19", "Vol-%", "monthlySoilMoisL19");
  case daysWithCrop:
    return ResultIdInfo("Anzahl der Tage mit Pflanzenbewuchs", "d", "daysWithCrop");
  case NStress:
    return ResultIdInfo("Akkumulierte Werte für N-Stress", "", "NStress");
  case WaterStress:
    return ResultIdInfo("Akkumulierte Werte für N-Stress", "", "waterStress");
  case HeatStress:
    return ResultIdInfo("Akkumulierte Werte für N-Stress", "", "heatStress");
  case OxygenStress:
    return ResultIdInfo("Akkumulierte Werte für N-Stress", "", "oxygenStress");
  case dev_stage:
    return ResultIdInfo("Liste mit täglichen Werten für das Entwicklungsstadium", "[]", "devStage");
  case soilMoist0_90cm:
	  return ResultIdInfo("Liste mit täglichen Werten für den Wassergehalt in 0-90cm", "[%]", "soilMoist0_90");
  case corg0_30cm:
	  return ResultIdInfo("Liste mit täglichen Werten für Corg in 0-30cm", "[]", "corg0_30");
  case nmin0_90cm:
	  return ResultIdInfo("Liste mit täglichen Werten für Nmin in 0-90cm", "[kg N / ha]", "nmin0_90");
  case ETa:
	  return ResultIdInfo("Aktuelle Evapotranspiration", "mm", "ETa");
  case dailyAGB:
	  return ResultIdInfo("Aktuelle Evapotranspiration", "kg FM ha-1", "dailyAGB");
  case dailyAGB_N:
	  return ResultIdInfo("Aktuelle Evapotranspiration", "kg N ha-1", "dailyAGB_N");
	default: ;
	}
	return ResultIdInfo("", "");
}

//------------------------------------------------------------------------------

PVResult::PVResult(json11::Json j)
  : id(j["cropId"].int_value()),
    customId(j["customId"].int_value()),
    date(Tools::Date::fromIsoDateString(j["date"].string_value()))
{
  for(auto jpvr : j["pvResults"].object_items())
    pvResults[ResultId(stoi(jpvr.first))] = jpvr.second.number_value();
}

json11::Json PVResult::to_json() const
{
  json11::Json::object pvrs;
  for(auto pvr : pvResults)
    pvrs[to_string(pvr.first)] = pvr.second;
  return json11::Json::object {
		{"type", "PVResult"},
		{"cropId", id},
		{"customId", customId},
    {"date", date.toIsoDateString()},
    {"pvResults", pvrs}};
}

//------------------------------------------------------------------------------

/**
 * @brief Constructor
 * @param oid organ ID
 * @param yp Yield percentage
 */
YieldComponent::YieldComponent(int oid, double yp, double ydm)
  : organId(oid),
    yieldPercentage(yp),
    yieldDryMatter(ydm)
{}

YieldComponent::YieldComponent(json11::Json j)
  : organId(Tools::int_value(j, "organId")),
    yieldPercentage(Tools::number_value(j, "yieldPercentage")),
    yieldDryMatter(Tools::number_value(j, "yieldDryMatter"))
{}

json11::Json YieldComponent::to_json() const
{
  return json11::Json::object{
    {"type", "YieldComponent"},
    {"organId", organId},
    {"yieldPercentage", yieldPercentage},
    {"yieldDryMatter", yieldDryMatter}};
}

//------------------------------------------------------------------------------

CropParameters::CropParameters(json11::Json j)
  : pc_CropName(string_value(j, "CropName")),
    pc_Perennial(bool_value(j, "Perennial")),
    pc_NumberOfDevelopmentalStages(int_value(j, "NumberOfDevelopmentalStages")),
    pc_NumberOfOrgans(int_value(j, "NumberOfOrgans")),
    pc_CarboxylationPathway(int_value(j, "CarboxylationPathway")),
    pc_DefaultRadiationUseEfficiency(number_value(j, "DefaultRadiationUseEfficiency")),
    pc_PartBiologicalNFixation(number_value(j, "PartBiologicalNFixation")),
    pc_InitialKcFactor(number_value(j, "InitialKcFactor")),
    pc_LuxuryNCoeff(number_value(j, "LuxuryNCoeff")),
    pc_MaxAssimilationRate(number_value(j, "MaxAssimilationRate")),
    pc_MaxCropDiameter(number_value(j, "MaxCropDiameter")),
    pc_MaxCropHeight(number_value(j, "MaxCropHeight")),
    pc_CropHeightP1(number_value(j, "CropHeightP1")),
    pc_CropHeightP2(number_value(j, "CropHeightP2")),
    pc_StageAtMaxHeight(number_value(j, "StageAtMaxHeight")),
    pc_StageAtMaxDiameter(number_value(j, "StageAtMaxDiameter")),
    pc_MinimumNConcentration(number_value(j, "MinimumNConcentration")),
    pc_MinimumTemperatureForAssimilation(number_value(j, "MinimumTemperatureForAssimilation")),
    pc_NConcentrationAbovegroundBiomass(number_value(j, "NConcentrationAbovegroundBiomass")),
    pc_NConcentrationB0(number_value(j, "NConcentrationB0")),
    pc_NConcentrationPN(number_value(j, "NConcentrationPN")),
    pc_NConcentrationRoot(number_value(j, "NConcentrationRoot")),
    pc_ResidueNRatio(number_value(j, "ResidueNRatio")),
    pc_DevelopmentAccelerationByNitrogenStress(int_value(j, "DevelopmentAccelerationByNitrogenStress")),
    pc_FieldConditionModifier(number_value(j, "FieldConditionModifier")),
    pc_AssimilateReallocation(number_value(j, "AssimilateReallocation")),
    pc_LT50cultivar(number_value(j, "LT50cultivar")),
    pc_FrostHardening(number_value(j, "FrostHardening")),
    pc_FrostDehardening(number_value(j, "FrostDehardening")),
    pc_LowTemperatureExposure(number_value(j, "LowTemperatureExposure")),
    pc_RespiratoryStress(number_value(j, "RespiratoryStress")),
    pc_LatestHarvestDoy(int_value(j, "LatestHarvestDoy")),
//    std::vector<std::vector<double>> pc_AssimilatePartitioningCoeff,
//    std::vector<std::vector<double>> pc_OrganSenescenceRate,
    pc_BaseDaylength(double_vector(j, "BaseDaylength")),
    pc_BaseTemperature(double_vector(j, "BaseTemperature")),
    pc_OptimumTemperature(double_vector(j, "OptimumTemperature")),
    pc_DaylengthRequirement(double_vector(j, "DaylengthRequirement")),
    pc_DroughtStressThreshold(double_vector(j, "DroughtStressThreshold")),
    pc_OrganMaintenanceRespiration(double_vector(j, "OrganMaintenanceRespiration")),
    pc_OrganGrowthRespiration(double_vector(j, "OrganGrowthRespiration")),
    pc_SpecificLeafArea(double_vector(j, "SpecificLeafArea")),
    pc_StageMaxRootNConcentration(double_vector(j, "StageMaxRootNConcentration")),
    pc_StageKcFactor(double_vector(j, "StageKcFactor")),
    pc_StageTemperatureSum(double_vector(j, "StageTemperatureSum")),
    pc_VernalisationRequirement(double_vector(j, "VernalisationRequirement")),
    pc_InitialOrganBiomass(double_vector(j, "InitialOrganBiomass")),
    pc_CriticalOxygenContent(double_vector(j, "CriticalOxygenContent")),
    pc_CropSpecificMaxRootingDepth(number_value(j, "CropSpecificMaxRootingDepth")),
    pc_AbovegroundOrgan(int_vector(j, "AbovegroundOrgan")),
    pc_StorageOrgan(int_vector(j, "StorageOrgan")),
    pc_SamplingDepth(number_value(j, "SamplingDepth")),
    pc_TargetNSamplingDepth(number_value(j, "TargetNSamplingDepth")),
    pc_TargetN30(number_value(j, "TargetN30")),
    pc_HeatSumIrrigationStart(number_value(j, "HeatSumIrrigationStart")),
    pc_HeatSumIrrigationEnd(number_value(j, "HeatSumIrrigationEnd")),
    pc_MaxNUptakeParam(number_value(j, "MaxNUptakeParam")),
    pc_RootDistributionParam(number_value(j, "RootDistributionParam")),
    pc_PlantDensity(number_value(j, "PlantDensity")),
    pc_RootGrowthLag(number_value(j, "RootGrowthLag")),
    pc_MinimumTemperatureRootGrowth(number_value(j, "MinimumTemperatureRootGrowth")),
    pc_InitialRootingDepth(number_value(j, "InitialRootingDepth")),
    pc_RootPenetrationRate(number_value(j, "RootPenetrationRate")),
    pc_RootFormFactor(number_value(j, "RootFormFactor")),
    pc_SpecificRootLength(number_value(j, "SpecificRootLength")),
    pc_StageAfterCut(int_value(j, "StageAfterCut")),
    pc_CriticalTemperatureHeatStress(number_value(j, "CriticalTemperatureHeatStress")),
    pc_LimitingTemperatureHeatStress(number_value(j, "LimitingTemperatureHeatStress")),
    pc_BeginSensitivePhaseHeatStress(number_value(j, "BeginSensitivePhaseHeatStress")),
    pc_EndSensitivePhaseHeatStress(number_value(j, "EndSensitivePhaseHeatStress")),
    pc_CuttingDelayDays(int_value(j, "CuttingDelayDays")),
    pc_DroughtImpactOnFertilityFactor(number_value(j, "DroughtImpactOnFertilityFactor")),
    pc_OrganIdsForPrimaryYield(toVector<YieldComponent>(j["OrganIdsForPrimaryYield"])),
    pc_OrganIdsForSecondaryYield(toVector<YieldComponent>(j["OrganIdsForSecondaryYield"])),
    pc_OrganIdsForCutting(toVector<YieldComponent>(j["OrganIdsForCutting"]))
{
  for(auto js : j["AssimilatePartitioningCoeff"].array_items())
    pc_AssimilatePartitioningCoeff.push_back(double_vector(js));

  for(auto js : j["OrganSenescenceRate"].array_items())
    pc_OrganSenescenceRate.push_back(double_vector(js));
}

json11::Json CropParameters::to_json() const
{
  json11::Json::array apcs;
  for(auto v : pc_AssimilatePartitioningCoeff)
    apcs.push_back(toPrimJsonArray(v));

  json11::Json::array osrs;
  for(auto v : pc_OrganSenescenceRate)
    osrs.push_back(toPrimJsonArray(v));

  return json11::Json::object{
    {"type", "CropParameters"},
    {"CropName", pc_CropName},
    {"Perennial", pc_Perennial},
    {"NumberOfDevelopmentalStages", pc_NumberOfDevelopmentalStages},
    {"NumberOfOrgans", pc_NumberOfOrgans},
    {"CarboxylationPathway", pc_CarboxylationPathway},
    {"DefaultRadiationUseEfficiency", pc_DefaultRadiationUseEfficiency},
    {"PartBiologicalNFixation", pc_PartBiologicalNFixation},
    {"InitialKcFactor", pc_InitialKcFactor},
    {"LuxuryNCoeff", pc_LuxuryNCoeff},
    {"MaxAssimilationRate", pc_MaxAssimilationRate},
    {"MaxCropDiameter", pc_MaxCropDiameter},
    {"MaxCropHeight", pc_MaxCropHeight},
    {"CropHeightP1", pc_CropHeightP1},
    {"CropHeightP2", pc_CropHeightP2},
    {"StageAtMaxHeight", pc_StageAtMaxHeight},
    {"StageAtMaxDiameter", pc_StageAtMaxDiameter},
    {"MinimumNConcentration", pc_MinimumNConcentration},
    {"MinimumTemperatureForAssimilation", pc_MinimumTemperatureForAssimilation},
    {"NConcentrationAbovegroundBiomass", pc_NConcentrationAbovegroundBiomass},
    {"NConcentrationB0", pc_NConcentrationB0},
    {"NConcentrationPN", pc_NConcentrationPN},
    {"NConcentrationRoot", pc_NConcentrationRoot},
    {"ResidueNRatio", pc_ResidueNRatio},
    {"DevelopmentAccelerationByNitrogenStress", pc_DevelopmentAccelerationByNitrogenStress},
    {"FieldConditionModifier", pc_FieldConditionModifier},
    {"AssimilateReallocation", pc_AssimilateReallocation},
    {"LT50cultivar", pc_LT50cultivar},
    {"FrostHardening", pc_FrostHardening},
    {"FrostDehardening", pc_FrostDehardening},
    {"LowTemperatureExposure", pc_LowTemperatureExposure},
    {"RespiratoryStress", pc_RespiratoryStress},
    {"LatestHarvestDoy", pc_LatestHarvestDoy},
    {"AssimilatePartitioningCoeff", apcs},
    {"OrganSenescenceRate", osrs},
    {"BaseDaylength", toPrimJsonArray(pc_BaseDaylength)},
    {"BaseTemperature", toPrimJsonArray(pc_BaseTemperature)},
    {"OptimumTemperature", toPrimJsonArray(pc_OptimumTemperature)},
    {"DaylengthRequirement", toPrimJsonArray(pc_DaylengthRequirement)},
    {"DroughtStressThreshold", toPrimJsonArray(pc_DroughtStressThreshold)},
    {"OrganMaintenanceRespiration", toPrimJsonArray(pc_OrganMaintenanceRespiration)},
    {"OrganGrowthRespiration", toPrimJsonArray(pc_OrganGrowthRespiration)},
    {"SpecificLeafArea", toPrimJsonArray(pc_SpecificLeafArea)},
    {"StageMaxRootNConcentration", toPrimJsonArray(pc_StageMaxRootNConcentration)},
    {"StageKcFactor", toPrimJsonArray(pc_StageKcFactor)},
    {"StageTemperatureSum", toPrimJsonArray(pc_StageTemperatureSum)},
    {"VernalisationRequirement", toPrimJsonArray(pc_VernalisationRequirement)},
    {"InitialOrganBiomass", toPrimJsonArray(pc_InitialOrganBiomass)},
    {"CriticalOxygenContent", toPrimJsonArray(pc_CriticalOxygenContent)},
    {"CropSpecificMaxRootingDepth", pc_CropSpecificMaxRootingDepth},
    {"AbovegroundOrgan", toPrimJsonArray(pc_AbovegroundOrgan)},
    {"StorageOrgan", toPrimJsonArray(pc_StorageOrgan)},
    {"SamplingDepth", pc_SamplingDepth},
    {"TargetNSamplingDepth", pc_TargetNSamplingDepth},
    {"TargetN30", pc_TargetN30},
    {"HeatSumIrrigationStart", pc_HeatSumIrrigationStart},
    {"HeatSumIrrigationEnd", pc_HeatSumIrrigationEnd},
    {"MaxNUptakeParam", pc_MaxNUptakeParam},
    {"RootDistributionParam", pc_RootDistributionParam},
    {"PlantDensity", pc_PlantDensity},
    {"RootGrowthLag", pc_RootGrowthLag},
    {"MinimumTemperatureRootGrowth", pc_MinimumTemperatureRootGrowth},
    {"InitialRootingDepth", pc_InitialRootingDepth},
    {"RootPenetrationRate", pc_RootPenetrationRate},
    {"RootFormFactor", pc_RootFormFactor},
    {"SpecificRootLength", pc_SpecificRootLength},
    {"StageAfterCut", pc_StageAfterCut},
    {"CriticalTemperatureHeatStress", pc_CriticalTemperatureHeatStress},
    {"LimitingTemperatureHeatStress", pc_LimitingTemperatureHeatStress},
    {"BeginSensitivePhaseHeatStress", pc_BeginSensitivePhaseHeatStress},
    {"EndSensitivePhaseHeatStress", pc_EndSensitivePhaseHeatStress},
    {"CuttingDelayDays", pc_CuttingDelayDays},
    {"DroughtImpactOnFertilityFactor", pc_DroughtImpactOnFertilityFactor},
    {"OrganIdsForPrimaryYield", toJsonArray(pc_OrganIdsForPrimaryYield)},
    {"OrganIdsForSecondaryYield", toJsonArray(pc_OrganIdsForSecondaryYield)},
    {"OrganIdsForCutting", toJsonArray(pc_OrganIdsForCutting)}};
}

/**
 * @brief Returns a string of information about crop parameters.
 *
 * Generates a string that contains all relevant crop parameter information.
 *
 * @return String of crop information.
 */
string CropParameters::toString() const
{
  ostringstream s;

  s << "pc_CropName:\t" << pc_CropName << endl;

  s << "------------------------------------------------" << endl;

  s << "pc_NumberOfDevelopmentalStages:\t" << pc_NumberOfDevelopmentalStages << endl;
  s << "pc_NumberOfOrgans:\t\t\t\t" << pc_NumberOfOrgans << endl;

  s << "------------------------------------------------" << endl;

  // assimilate partitioning coefficient matrix
  s << "pc_AssimilatePartitioningCoeff:\t" << endl;
  for (unsigned int i = 0; i < pc_AssimilatePartitioningCoeff.size(); i++)   {
    for (unsigned int j = 0; j < pc_AssimilatePartitioningCoeff[i].size(); j++) {
      s << pc_AssimilatePartitioningCoeff[i][j] << " ";
    }
    s << endl;
  }
  s << "------------------------------------------------" << endl;

  s << "pc_CarboxylationPathway:\t\t\t\t" << pc_CarboxylationPathway << endl;
  s << "pc_MaxAssimilationRate:\t\t\t\t\t" << pc_MaxAssimilationRate << endl;
  s << "pc_MinimumTemperatureForAssimilation:\t" << pc_MinimumTemperatureForAssimilation << endl;
  s << "pc_CropSpecificMaxRootingDepth:\t\t\t" << pc_CropSpecificMaxRootingDepth << endl;
  s << "pc_InitialKcFactor:\t\t\t\t\t\t" << pc_InitialKcFactor << endl;
  s << "pc_MaxCropDiameter:\t\t\t\t\t\t" << pc_MaxCropDiameter << endl;
  s << "pc_StageAtMaxDiameter:\t\t\t\t\t" << pc_StageAtMaxDiameter << endl;
  s << "pc_PlantDensity:\t\t\t\t\t\t" << pc_PlantDensity << endl;
  s << "pc_DefaultRadiationUseEfficiency:\t\t" << pc_DefaultRadiationUseEfficiency << endl;
  s << "pc_StageAfterCut:\t\t\t\t\t\t" << pc_StageAfterCut << endl;
  s << "pc_CuttingDelayDays:\t\t\t\t\t" << pc_CuttingDelayDays << endl;

  s << "------------------------------------------------" << endl;

  s << "pc_RootDistributionParam:\t\t\t" << pc_RootDistributionParam << endl;
  s << "pc_RootGrowthLag:\t\t\t\t\t" << pc_RootGrowthLag << endl;
  s << "pc_MinimumTemperatureRootGrowth:\t" << pc_MinimumTemperatureRootGrowth << endl;
  s << "pc_InitialRootingDepth:\t\t\t\t" << pc_InitialRootingDepth << endl;
  s << "pc_RootPenetrationRate:\t\t\t\t" << pc_RootPenetrationRate << endl;
  s << "pc_RootFormFactor:\t\t\t\t\t" << pc_RootFormFactor << endl;
  s << "pc_SpecificRootLength:\t\t\t\t" << pc_SpecificRootLength << endl;

  s << "------------------------------------------------" << endl;

  s << "pc_MaxCropHeight:\t\t" << pc_MaxCropHeight << endl;
  s << "pc_CropHeightP1:\t\t" << pc_CropHeightP1 << endl;
  s << "pc_CropHeightP2:\t\t" << pc_CropHeightP2 << endl;
  s << "pc_StageAtMaxHeight:\t" << pc_StageAtMaxHeight << endl;

  s << "------------------------------------------------" << endl;

  s << "pc_FixingN:\t\t\t\t\t" << pc_PartBiologicalNFixation << endl;
  s << "pc_MinimumNConcentration:\t" << pc_MinimumNConcentration << endl;
  s << "pc_LuxuryNCoeff:\t\t\t" << pc_LuxuryNCoeff << endl;
  s << "pc_NConcentrationB0:\t\t" << pc_NConcentrationB0 << endl;
  s << "pc_NConcentrationPN:\t\t" << pc_NConcentrationPN << endl;
  s << "pc_NConcentrationRoot:\t\t" << pc_NConcentrationRoot << endl;
  s << "pc_ResidueNRatio:\t\t\t" << pc_ResidueNRatio << endl;
  s << "pc_MaxNUptakeParam:\t\t\t" << pc_MaxNUptakeParam << endl;

  s << "------------------------------------------------" << endl;

  s << "pc_DevelopmentAccelerationByNitrogenStress:\t" << pc_DevelopmentAccelerationByNitrogenStress << endl;
  s << "pc_NConcentrationAbovegroundBiomass:\t\t" << pc_NConcentrationAbovegroundBiomass << endl;
  s << "pc_DroughtImpactOnFertilityFactor:\t\t\t" << pc_DroughtImpactOnFertilityFactor << endl;

  s << "------------------------------------------------" << endl;

  s << "pc_SamplingDepth:\t\t\t\t\t" << pc_SamplingDepth << endl;
  s << "pc_TargetNSamplingDepth:\t\t\t" << pc_TargetNSamplingDepth << endl;
  s << "pc_TargetN30:\t\t\t\t\t\t" << pc_TargetN30 << endl;
  s << "pc_HeatSumIrrigationStart:\t\t\t" << pc_HeatSumIrrigationStart << endl;
  s << "pc_HeatSumIrrigationEnd:\t\t\t" << pc_HeatSumIrrigationEnd << endl;
  s << "pc_CriticalTemperatureHeatStress:\t" << pc_CriticalTemperatureHeatStress << endl;
  s << "pc_LimitingTemperatureHeatStress:\t" << pc_LimitingTemperatureHeatStress << endl;
  s << "pc_BeginSensitivePhaseHeatStress:\t" << pc_BeginSensitivePhaseHeatStress << endl;
  s << "pc_EndSensitivePhaseHeatStress:\t\t" << pc_EndSensitivePhaseHeatStress << endl;

  s << "------------------------------------------------" << endl;
  s << "pc_LatestHarvestDoy:\t\t" << pc_LatestHarvestDoy  << endl;

  //s << endl;
  s << "------------------------------------------------" << endl;
  // above-ground organ
  s << "pc_AbovegroundOrgan:" << endl;
  for (unsigned i = 0; i < pc_AbovegroundOrgan.size(); i++)
    s << (pc_AbovegroundOrgan[i] == 1) << " ";

  s << endl;
  s << endl;

  // initial organic biomass
  s  << "pc_InitialOrganBiomass:" << endl;
  for (unsigned int i = 0; i < pc_InitialOrganBiomass.size(); i++)
    s << pc_InitialOrganBiomass[i] << " ";

  s << endl;
  s << endl;

  // organ maintenance respiration rate
  s << "pc_OrganMaintenanceRespiration:" << endl;
  for (unsigned int i = 0; i < pc_OrganMaintenanceRespiration.size(); i++)
    s << pc_OrganMaintenanceRespiration[i] << " ";

  s << endl;
  s << endl;

  // organ growth respiration rate
  s  << "pc_OrganGrowthRespiration:" << endl;
  for (unsigned int i = 0; i < pc_OrganGrowthRespiration.size(); i++)
    s << pc_OrganGrowthRespiration[i] << " ";

  s << endl;
  s << endl;

  // organ senescence rate
  s << "pc_OrganSenescenceRate:" << endl;
  for (unsigned int i = 0; i < pc_OrganSenescenceRate.size(); i++) {
    for (unsigned int j = 0; j < pc_OrganSenescenceRate[i].size(); j++) {
      s << pc_OrganSenescenceRate[i][j] << " ";
    }
    s << endl;
 }

  s << "------------------------------------------------" << endl;
  //s << endl;
  //s << endl;

  // stage temperature sum
  s << "pc_StageTemperatureSum:" << endl;
  for (unsigned int i = 0; i < pc_StageTemperatureSum.size(); i++)
    s << pc_StageTemperatureSum[i] << " ";

  s << endl;
  s << endl;

  // Base day length
  s << "pc_BaseDaylength: " << endl;
  for (unsigned int i = 0; i < pc_BaseDaylength.size(); i++)
    s << pc_BaseDaylength[i] << " ";

  s << endl;
  s << endl;

  // base temperature
  s << "pc_BaseTemperature: " << endl;
  for (unsigned int i = 0; i < pc_BaseTemperature.size(); i++)
    s << pc_BaseTemperature[i] << " ";

  s << endl;
  s << endl;

  // optimum temperature
  s << "pc_OptimumTemperature: " << endl;
  for (unsigned int i = 0; i < pc_OptimumTemperature.size(); i++)
    s << pc_OptimumTemperature[i] << " ";

  s << endl;
  s << endl;

  // day length requirement
  s << "pc_DaylengthRequirement: " << endl;
  for (unsigned int i = 0; i < pc_DaylengthRequirement.size(); i++)
    s << pc_DaylengthRequirement[i] << " ";

  s << endl;
  s << endl;

  // specific leaf area
  s << "pc_SpecificLeafArea:" << endl;
  for (unsigned int i = 0; i < pc_SpecificLeafArea.size(); i++)
    s << pc_SpecificLeafArea[i] << " ";

  s << endl;
  s << endl;

  // stage max root n content
  s << "pc_StageMaxRootNConcentration:" << endl;
  for (unsigned int i = 0; i < pc_StageMaxRootNConcentration.size(); i++)
    s << pc_StageMaxRootNConcentration[i] << " ";

  s << endl;
  s << endl;

  // stage kc factor
  s << "pc_StageKcFactor:" << endl;
  for (unsigned int i = 0; i < pc_StageKcFactor.size(); i++)
    s << pc_StageKcFactor[i] << " ";

  s << endl;
  s << endl;

  // drought stress treshold
  s << "pc_DroughtStressThreshold:" << endl;
  for (unsigned int i = 0; i < pc_DroughtStressThreshold.size(); i++)
    s << pc_DroughtStressThreshold[i] << " ";

  s << endl;
  s << endl;

  // vernalisation requirement
  s << "pc_VernalisationRequirement:" << endl;
  for (unsigned int i = 0; i < pc_VernalisationRequirement.size(); i++)
    s << pc_VernalisationRequirement[i] << " ";

  s << endl;
  s << endl;

  // critical oxygen content
  s << "pc_CriticalOxygenContent:" << endl;
  for (unsigned int i = 0; i < pc_CriticalOxygenContent.size(); i++)
    s << pc_CriticalOxygenContent[i] << " ";

  s << endl;

  return s.str();
}

void CropParameters::resizeStageOrganVectors()
{
  pc_AssimilatePartitioningCoeff.resize(pc_NumberOfDevelopmentalStages,
                                        std::vector<double>(pc_NumberOfOrgans));
  pc_OrganSenescenceRate.resize(pc_NumberOfDevelopmentalStages,
                                std::vector<double>(pc_NumberOfOrgans));
}

//------------------------------------------------------------------------------

MineralFertiliserParameters::MineralFertiliserParameters(json11::Json j)
  : name(j["name"].string_value()),
    vo_Carbamid(j["Carbamid"].number_value()),
    vo_NH4(j["NH4"].number_value()),
    vo_NO3(j["NO3"].number_value())
{}

MineralFertiliserParameters::MineralFertiliserParameters(const std::string& name, double carbamid,
                            double no3, double nh4)
  : name(name),
    vo_Carbamid(carbamid),
    vo_NH4(nh4),
    vo_NO3(no3)
{}

json11::Json MineralFertiliserParameters::to_json() const
{
  return json11::Json::object {
    { "type", "MineralFertiliserParameters" },
    {"name", name},
    {"Carbamid", vo_Carbamid},
    {"NH4", vo_NH4},
    {"NO3", vo_NO3}};
}

//-----------------------------------------------------------------------------------------

NMinUserParameters::NMinUserParameters(double min, double max, int delayInDays)
	: min(min),
	max(max),
	delayInDays(delayInDays) { }

NMinUserParameters::NMinUserParameters(json11::Json j)
  : min(j["min"].number_value()),
    max(j["max"].number_value()),
    delayInDays(j["delayInDays"].int_value())
{}

json11::Json NMinUserParameters::to_json() const
{
  return json11::Json::object {
    {"type", "NMinUserParameters"},
    {"min", min},
    {"max", max},
    {"delayInDays", delayInDays}};
}

//------------------------------------------------------------------------------

IrrigationParameters::IrrigationParameters(double nitrateConcentration,
                                           double sulfateConcentration)
  : nitrateConcentration(nitrateConcentration),
    sulfateConcentration(sulfateConcentration)
{}

IrrigationParameters::IrrigationParameters(json11::Json j)
  : nitrateConcentration(j["nitrateConcentration"].number_value()),
    sulfateConcentration(j["sulfateConcentration"].number_value())
{}

json11::Json IrrigationParameters::to_json() const
{
  return json11::Json::object {
    {"type", "IrrigationParameters"},
    {"nitrateConcentration", nitrateConcentration},
    {"sulfateConcentration", sulfateConcentration}};
}

//------------------------------------------------------------------------------

AutomaticIrrigationParameters::AutomaticIrrigationParameters(double a, double t, double nc, double sc)
  : IrrigationParameters(nc, sc),
    amount(a),
    treshold(t) {}

AutomaticIrrigationParameters::AutomaticIrrigationParameters(json11::Json j)
  : IrrigationParameters(j["irrigationParameters"]),
    amount(j["amount"].number_value()),
    treshold(j["treshold"].number_value())
{}

json11::Json AutomaticIrrigationParameters::to_json() const
{
  return json11::Json::object {
    {"type", "AutomaticIrrigationParameters"},
    {"irrigationParameters", IrrigationParameters::to_json()},
    {"amount", json11::Json::array {amount, "mm"}},
    {"treshold", treshold}};
}

//------------------------------------------------------------------------------

MeasuredGroundwaterTableInformation::MeasuredGroundwaterTableInformation(json11::Json j)
  : groundwaterInformationAvailable(Tools::bool_value(j, "groundwaterInformationAvailable"))
{
  if(j.has_shape({{"groundwaterInfo", json11::Json::OBJECT}}, std::string()))
    for(auto p : j["groundwaterInfo"].object_items())
      groundwaterInfo[Tools::Date::fromIsoDateString(p.first)] = p.second.number_value();
}

json11::Json MeasuredGroundwaterTableInformation::to_json() const
{
  json11::Json::object gi;
  for(auto p : groundwaterInfo)
    gi[p.first.toIsoDateString()] = p.second;

  return json11::Json::object {
    {"type", "MeasuredGroundwaterTableInformation"},
    {"groundwaterInformationAvailable", groundwaterInformationAvailable},
    {"groundwaterInfo", gi}};
}

void MeasuredGroundwaterTableInformation::readInGroundwaterInformation(std::string path)
{
  ifstream ifs(path.c_str(), ios::in);
   if (!ifs.is_open())
   {
     cout << "ERROR while opening file " << path.c_str() << endl;
     return;
   }

   groundwaterInformationAvailable = true;

   // read in information from groundwater table file
   string s;
   while (getline(ifs, s))
   {
     // date, value
     std::string date_string;
     double gw_cm;

     istringstream ss(s);
     ss >> date_string >> gw_cm;

     Date gw_date = Tools::fromMysqlString(date_string.c_str());

     if (!gw_date.isValid())
     {
       debug() << "ERROR - Invalid date in \"" << path.c_str() << "\"" << endl;
       debug() << "Line: " << s.c_str() << endl;
       continue;
     }
     cout << "Added gw value\t" << gw_date.toString().c_str() << "\t" << gw_cm << endl;
     groundwaterInfo[gw_date] = gw_cm;
   }
}

double MeasuredGroundwaterTableInformation::getGroundwaterInformation(Tools::Date gwDate) const
{
  if (groundwaterInformationAvailable && groundwaterInfo.size()>0)
  {
    auto it = groundwaterInfo.find(gwDate);
    if(it != groundwaterInfo.end())
      return it->second;
  }
  return -1;
}

//------------------------------------------------------------------------------

GeneralParameters::GeneralParameters(double layerThickness)
  : ps_LayerThickness(int(ps_ProfileDepth / layerThickness), layerThickness)
{}

GeneralParameters::GeneralParameters(json11::Json j)
  : ps_ProfileDepth(number_value(j, "ProfileDepth")),
    ps_MaxMineralisationDepth(number_value(j, "MaxMineralisationDepth")),
    pc_NitrogenResponseOn(bool_value(j, "NitrogenResponseOn")),
    pc_WaterDeficitResponseOn(bool_value(j, "WaterDeficitResponseOn")),
    pc_EmergenceFloodingControlOn(bool_value(j, "EmergenceFloodingControlOn")),
    pc_EmergenceMoistureControlOn(bool_value(j, "EmergenceMoistureControlOn")),
    ps_LayerThickness(double_vector(j, "LayerThickness")),
    useNMinMineralFertilisingMethod(bool_value(j, "useNMinMineralFertilisingMethod")),
    nMinFertiliserPartition(j["nMinFertiliserPartition"]),
    nMinUserParams(j["nMinUserParams"]),
    atmosphericCO2(number_value(j, "atmosphericCO2")),
    useAutomaticIrrigation(bool_value(j, "useAutomaticIrrigation")),
    autoIrrigationParams(j["autoIrrigationParams"]),
    useSecondaryYields(bool_value(j, "useSecondaryYields")),
    windSpeedHeight(number_value(j, "windSpeedHeight")),
    albedo(number_value(j, "albedo")),
    groundwaterInformation(j["groundwaterInformation"]),
    pathToOutputDir(string_value(j, "pathToOutputDir"))
{
}

json11::Json GeneralParameters::to_json() const
{
  return json11::Json::object {
    {"type", "GeneralParameters"},
    {"ProfileDepth", ps_ProfileDepth},
    {"MaxMineralisationDepth", ps_MaxMineralisationDepth},
    {"NitrogenResponseOn", pc_NitrogenResponseOn},
    {"WaterDeficitResponseOn", pc_WaterDeficitResponseOn},
    {"EmergenceFloodingControlOn", pc_EmergenceFloodingControlOn},
    {"EmergenceMoistureControlOn", pc_EmergenceMoistureControlOn},
    {"LayerThickness", toPrimJsonArray(ps_LayerThickness)},
    {"useNMinMineralFertilisingMethod", useNMinMineralFertilisingMethod},
    {"nMinFertiliserPartition", nMinFertiliserPartition},
    {"nMinUserParams", nMinUserParams},
    {"atmosphericCO2", atmosphericCO2},
    {"useAutomaticIrrigation", useAutomaticIrrigation},
    {"autoIrrigationParams", autoIrrigationParams},
    {"useSecondaryYields", useSecondaryYields},
    {"windSpeedHeight", windSpeedHeight},
    {"albedo", albedo},
    {"groundwaterInformation", groundwaterInformation},
    {"pathToOutputDir", pathToOutputDir}};
}

//------------------------------------------------------------------------------

SiteParameters::SiteParameters(json11::Json j)
  : vs_Latitude(j["Latitude"].number_value()),
    vs_Slope(j["Slope"].number_value()),
    vs_HeightNN(j["HeightNN"].number_value()),
    vs_GroundwaterDepth(j["GroundwaterDepth"].number_value()),
    vs_Soil_CN_Ratio(j["Soil_CN_Ratio"].number_value()),
    vs_DrainageCoeff(j["DrainageCoeff"].number_value()),
    vq_NDeposition(j["NDeposition"].number_value()),
    vs_MaxEffectiveRootingDepth(j["MaxEffectiveRootingDepth"].number_value())
{}

json11::Json SiteParameters::to_json() const
{
  return J11Object {
    {"type", "SiteParameters"},
    {"Latitude", J11Array {vs_Latitude, "", "latitude in decimal degrees"}},
    {"Slope", J11Array {vs_Slope, "m m-1"}},
    {"HeightNN", J11Array {vs_HeightNN, "m", "height above sea level"}},
    {"GroundwaterDepth", J11Array {vs_GroundwaterDepth, "m"}},
    {"Soil_CN_Ratio", vs_Soil_CN_Ratio},
    {"DrainageCoeff", vs_DrainageCoeff},
    {"NDeposition", vq_NDeposition},
    {"MaxEffectiveRootingDepth", vs_MaxEffectiveRootingDepth}};
}

//------------------------------------------------------------------------------

AutomaticHarvestParameters::AutomaticHarvestParameters(HarvestTime yt)
  : _harvestTime(yt)
{}

AutomaticHarvestParameters::AutomaticHarvestParameters(json11::Json j)
  : _harvestTime(HarvestTime(int_value(j, "harvestTime"))),
    _latestHarvestDOY(int_value(j, "latestHarvestDOY"))
{}

json11::Json AutomaticHarvestParameters::to_json() const
{
  return J11Object {
    {"harvestTime", int(_harvestTime)},
    {"latestHavestDOY", _latestHarvestDOY}};
}

/**
 * @brief Returns a short summary with information about automatic yield parameter configuration.
 * @brief String
 */
string AutomaticHarvestParameters::toString() const
{
  ostringstream s;
  if (_harvestTime == AutomaticHarvestParameters::HarvestTime::maturity) {
    s << "Automatic harvestTime: Maturity ";
  }

  return s.str();
}

//------------------------------------------------------------------------------

NMinCropParameters::NMinCropParameters(double samplingDepth, double nTarget, double nTarget30)
  : samplingDepth(samplingDepth),
    nTarget(nTarget),
    nTarget30(nTarget30) {}

NMinCropParameters::NMinCropParameters(json11::Json j)
  : samplingDepth(j["samplingDepth"].number_value()),
    nTarget(j["nTarget"].number_value()),
    nTarget30(j["nTarget30"].number_value())
{}

json11::Json NMinCropParameters::to_json() const
{
  return json11::Json::object {
    {"type", "NMinCropParameters"},
    {"samplingDepth", samplingDepth},
    {"nTarget", nTarget},
    {"nTarget30", nTarget30}};
}

//------------------------------------------------------------------------------

OrganicMatterParameters::OrganicMatterParameters(json11::Json j)
  : name(j["name"].string_value()),
  vo_AOM_DryMatterContent(j["AOM_DryMatterContent"].number_value()),
  vo_AOM_NH4Content(j["AOM_NH4Content"].number_value()),
  vo_AOM_NO3Content(j["AOM_NO3Content"].number_value()),
  vo_AOM_CarbamidContent(j["AOM_CarbamidContent"].number_value()),
  vo_AOM_SlowDecCoeffStandard(j["AOM_SlowDecCoeffStandard"].number_value()),
  vo_AOM_FastDecCoeffStandard(j["AOM_FastDecCoeffStandard"].number_value()),
  vo_PartAOM_to_AOM_Slow(j["PartAOM_to_AOM_Slow"].number_value()),
  vo_PartAOM_to_AOM_Fast(j["PartAOM_to_AOM_Fast"].number_value()),
  vo_CN_Ratio_AOM_Slow(j["CN_Ratio_AOM_Slow"].number_value()),
  vo_CN_Ratio_AOM_Fast(j["CN_Ratio_AOM_Fast"].number_value()),
  vo_PartAOM_Slow_to_SMB_Slow(j["PartAOM_Slow_to_SMB_Slow"].number_value()),
  vo_PartAOM_Slow_to_SMB_Fast(j["PartAOM_Slow_to_SMB_Fast"].number_value()),
  vo_NConcentration(j["NConcentration"].number_value())
{}

json11::Json OrganicMatterParameters::to_json() const
{
  return J11Object {
    {"name", name},
    {"AOM_DryMatterContent", J11Array {vo_AOM_DryMatterContent, "kg DM kg FM-1", "Dry matter content of added organic matter"}},
    {"AOM_AOM_NH4Content", J11Array {vo_AOM_NH4Content, "kg N kg DM-1", "Ammonium content in added organic matter"}},
    {"AOM_AOM_NO3Content", J11Array {vo_AOM_NO3Content, "kg N kg DM-1", "Nitrate content in added organic matter"}},
    {"AOM_AOM_NO3Content", J11Array {vo_AOM_NO3Content, "kg N kg DM-1", "Carbamide content in added organic matter"}},
    {"AOM_AOM_SlowDecCoeffStandard", J11Array {vo_AOM_SlowDecCoeffStandard, "d-1", "Decomposition rate coefficient of slow AOM at standard conditions"}},
    {"AOM_AOM_FastDecCoeffStandard", J11Array {vo_AOM_FastDecCoeffStandard, "d-1", "Decomposition rate coefficient of fast AOM at standard conditions"}},
    {"AOM_PartAOM_to_AOM_Slow", J11Array {vo_PartAOM_to_AOM_Slow, "kg kg-1", "Part of AOM that is assigned to the slowly decomposing pool"}},
    {"AOM_PartAOM_to_AOM_Fast", J11Array {vo_PartAOM_to_AOM_Fast, "kg kg-1", "Part of AOM that is assigned to the rapidly decomposing pool"}},
    {"AOM_CN_Ratio_AOM_Slow", J11Array {vo_CN_Ratio_AOM_Slow, "", "C to N ratio of the slowly decomposing AOM pool"}},
    {"AOM_CN_Ratio_AOM_Fast", J11Array {vo_CN_Ratio_AOM_Fast, "", "C to N ratio of the rapidly decomposing AOM pool"}},
    {"AOM_PartAOM_Slow_to_SMB_Slow", J11Array {vo_PartAOM_Slow_to_SMB_Slow, "kg kg-1", "Part of AOM slow consumed by slow soil microbial biomass"}},
    {"AOM_PartAOM_Slow_to_SMB_Fast", J11Array {vo_PartAOM_Slow_to_SMB_Fast, "kg kg-1", "Part of AOM slow consumed by fast soil microbial biomass"}},
    {"AOM_NConcentration", vo_NConcentration}};
}

//-----------------------------------------------------------------------------------------

UserCropParameters::UserCropParameters(json11::Json j)
  : pc_CanopyReflectionCoefficient(j["CanopyReflectionCoefficient"].number_value()),
    pc_ReferenceMaxAssimilationRate(j["ReferenceMaxAssimilationRate"].number_value()),
    pc_ReferenceLeafAreaIndex(j["ReferenceLeafAreaIndex"].number_value()),
    pc_MaintenanceRespirationParameter1(j["MaintenanceRespirationParameter1"].number_value()),
    pc_MaintenanceRespirationParameter2(j["MaintenanceRespirationParameter2"].number_value()),
    pc_MinimumNConcentrationRoot(j["MinimumNConcentrationRoot"].number_value()),
    pc_MinimumAvailableN(j["MinimumAvailableN"].number_value()),
    pc_ReferenceAlbedo(j["ReferenceAlbedo"].number_value()),
    pc_StomataConductanceAlpha(j["StomataConductanceAlpha"].number_value()),
    pc_SaturationBeta(j["SaturationBeta"].number_value()),
    pc_GrowthRespirationRedux(j["GrowthRespirationRedux"].number_value()),
    pc_MaxCropNDemand(j["MaxCropNDemand"].number_value()),
    pc_GrowthRespirationParameter1(j["GrowthRespirationParameter1"].number_value()),
    pc_GrowthRespirationParameter2(j["GrowthRespirationParameter2"].number_value()),
    pc_Tortuosity(j["Tortuosity"].number_value())
{}

json11::Json UserCropParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserCropParameters"},
    {"CanopyReflectionCoefficient", pc_CanopyReflectionCoefficient},
    {"ReferenceMaxAssimilationRate", pc_ReferenceMaxAssimilationRate},
    {"ReferenceLeafAreaIndex", pc_ReferenceLeafAreaIndex},
    {"MaintenanceRespirationParameter1", pc_MaintenanceRespirationParameter1},
    {"MaintenanceRespirationParameter2", pc_MaintenanceRespirationParameter2},
    {"MinimumNConcentrationRoot", pc_MinimumNConcentrationRoot},
    {"MinimumAvailableN", pc_MinimumAvailableN},
    {"ReferenceAlbedo", pc_ReferenceAlbedo},
    {"StomataConductanceAlpha", pc_StomataConductanceAlpha},
    {"SaturationBeta", pc_SaturationBeta},
    {"GrowthRespirationRedux", pc_GrowthRespirationRedux},
    {"MaxCropNDemand", pc_MaxCropNDemand},
    {"GrowthRespirationParameter1", pc_GrowthRespirationParameter1},
    {"GrowthRespirationParameter2", pc_GrowthRespirationParameter2},
    {"Tortuosity", pc_Tortuosity}};
}

//-----------------------------------------------------------------------------------------

UserEnvironmentParameters::UserEnvironmentParameters(json11::Json j)
  : p_UseAutomaticIrrigation(j["UseAutomaticIrrigation"].bool_value()),
    p_UseNMinMineralFertilisingMethod(j["UseNMinMineralFertilisingMethod"].bool_value()),
    p_UseSecondaryYields(j["UseSecondaryYields"].bool_value()),
    p_UseAutomaticHarvestTrigger(j["UseAutomaticHarvestTrigger"].bool_value()),
    p_LayerThickness(j["LayerThickness"].number_value()),
    p_Albedo(j["Albedo"].number_value()),
    p_AthmosphericCO2(j["AthmosphericCO2"].number_value()),
    p_WindSpeedHeight(j["WindSpeedHeight"].number_value()),
    p_LeachingDepth(j["LeachingDepth"].number_value()),
    p_timeStep(j["timeStep"].number_value()),
    p_MaxGroundwaterDepth(j["MaxGroundwaterDepth"].number_value()),
    p_MinGroundwaterDepth(j["MinGroundwaterDepth"].number_value()),
    p_NumberOfLayers(j["NumberOfLayers"].int_value()),
    p_StartPVIndex(j["StartPVIndex"].int_value()),
    p_JulianDayAutomaticFertilising(j["JulianDayAutomaticFertilising"].int_value()),
    p_MinGroundwaterDepthMonth(j["MinGroundwaterDepthMonth"].int_value())
{}

json11::Json UserEnvironmentParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserEnvironmentParameters"},
    {"UseAutomaticIrrigation", p_UseAutomaticIrrigation},
    {"UseNMinMineralFertilisingMethod", p_UseNMinMineralFertilisingMethod},
    {"UseSecondaryYields", p_UseSecondaryYields},
    {"UseAutomaticHarvestTrigger", p_UseAutomaticHarvestTrigger},
    {"LayerThickness", p_LayerThickness},
    {"Albedo", p_Albedo},
    {"AthmosphericCO2", p_AthmosphericCO2},
    {"WindSpeedHeight", p_WindSpeedHeight},
    {"LeachingDepth", p_LeachingDepth},
    {"timeStep", p_timeStep},
    {"MaxGroundwaterDepth", p_MaxGroundwaterDepth},
    {"MinGroundwaterDepth", p_MinGroundwaterDepth},
    {"NumberOfLayers", p_NumberOfLayers},
    {"StartPVIndex", p_StartPVIndex},
    {"JulianDayAutomaticFertilising", p_JulianDayAutomaticFertilising},
    {"MinGroundwaterDepthMonth", p_MinGroundwaterDepthMonth}};
}

//-----------------------------------------------------------------------------------------

UserInitialValues::UserInitialValues(json11::Json j)
  : p_initPercentageFC(j["initPercentageFC"].number_value()),
    p_initSoilNitrate(j["initSoilNitrate"].number_value()),
    p_initSoilAmmonium(j["initSoilAmmonium"].number_value())
{}

json11::Json UserInitialValues::to_json() const
{
  return json11::Json::object {
    {"type", "UserInitialValues"},
    {"initPercentageFC", J11Array {p_initPercentageFC, "", "Initial soil moisture content in percent field capacity"}},
    {"initSoilNitrate", J11Array {p_initSoilNitrate, "kg NO3-N m-3", "Initial soil nitrate content"}},
    {"initSoilAmmonium", J11Array {p_initSoilAmmonium, "kg NH4-N m-3", "Initial soil ammonium content"}}};
}

//-----------------------------------------------------------------------------------------

UserSoilMoistureParameters::UserSoilMoistureParameters(json11::Json j)
  : pm_CriticalMoistureDepth(j["CriticalMoistureDepth"].number_value()),
    pm_SaturatedHydraulicConductivity(j["SaturatedHydraulicConductivity"].number_value()),
    pm_SurfaceRoughness(j["SurfaceRoughness"].number_value()),
    pm_GroundwaterDischarge(j["GroundwaterDischarge"].number_value()),
    pm_HydraulicConductivityRedux(j["HydraulicConductivityRedux"].number_value()),
    pm_SnowAccumulationTresholdTemperature(j["SnowAccumulationTresholdTemperature"].number_value()),
    pm_KcFactor(j["KcFactor"].number_value()),
    pm_TemperatureLimitForLiquidWater(j["TemperatureLimitForLiquidWater"].number_value()),
    pm_CorrectionSnow(j["CorrectionSnow"].number_value()),
    pm_CorrectionRain(j["CorrectionRain"].number_value()),
    pm_SnowMaxAdditionalDensity(j["SnowMaxAdditionalDensity"].number_value()),
    pm_NewSnowDensityMin(j["NewSnowDensityMin"].number_value()),
    pm_SnowRetentionCapacityMin(j["SnowRetentionCapacityMin"].number_value()),
    pm_RefreezeParameter1(j["RefreezeParameter1"].number_value()),
    pm_RefreezeParameter2(j["RefreezeParameter2"].number_value()),
    pm_RefreezeTemperature(j["RefreezeTemperature"].number_value()),
    pm_SnowMeltTemperature(j["SnowMeltTemperature"].number_value()),
    pm_SnowPacking(j["SnowPacking"].number_value()),
    pm_SnowRetentionCapacityMax(j["SnowRetentionCapacityMax"].number_value()),
    pm_EvaporationZeta(j["EvaporationZeta"].number_value()),
    pm_XSACriticalSoilMoisture(j["XSACriticalSoilMoisture"].number_value()),
    pm_MaximumEvaporationImpactDepth(j["MaximumEvaporationImpactDepth"].number_value()),
    pm_MaxPercolationRate(j["MaxPercolationRate"].number_value()),
    pm_MoistureInitValue(j["MoistureInitValue"].number_value())
{}

json11::Json UserSoilMoistureParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserSoilMoistureParameters"},
    {"CriticalMoistureDepth", pm_CriticalMoistureDepth},
    {"SaturatedHydraulicConductivity", pm_SaturatedHydraulicConductivity},
    {"SurfaceRoughness", pm_SurfaceRoughness},
    {"GroundwaterDischarge", pm_GroundwaterDischarge},
    {"HydraulicConductivityRedux", pm_HydraulicConductivityRedux},
    {"SnowAccumulationTresholdTemperature", pm_SnowAccumulationTresholdTemperature},
    {"KcFactor", pm_KcFactor},
    {"TemperatureLimitForLiquidWater", pm_TemperatureLimitForLiquidWater},
    {"CorrectionSnow", pm_CorrectionSnow},
    {"CorrectionRain", pm_CorrectionRain},
    {"SnowMaxAdditionalDensity", pm_SnowMaxAdditionalDensity},
    {"NewSnowDensityMin", pm_NewSnowDensityMin},
    {"SnowRetentionCapacityMin", pm_SnowRetentionCapacityMin},
    {"RefreezeParameter1", pm_RefreezeParameter1},
    {"RefreezeParameter2", pm_RefreezeParameter2},
    {"RefreezeTemperature", pm_RefreezeTemperature},
    {"SnowMeltTemperature", pm_SnowMeltTemperature},
    {"SnowPacking", pm_SnowPacking},
    {"SnowRetentionCapacityMax", pm_SnowRetentionCapacityMax},
    {"EvaporationZeta", pm_EvaporationZeta},
    {"XSACriticalSoilMoisture", pm_XSACriticalSoilMoisture},
    {"MaximumEvaporationImpactDepth", pm_MaximumEvaporationImpactDepth},
    {"MaxPercolationRate", pm_MaxPercolationRate},
    {"MoistureInitValue", pm_MoistureInitValue}};
}

//-----------------------------------------------------------------------------------------

UserSoilTemperatureParameters::UserSoilTemperatureParameters(json11::Json j)
  : pt_NTau(j["NTau"].number_value()),
    pt_InitialSurfaceTemperature(j["InitialSurfaceTemperature"].number_value()),
    pt_BaseTemperature(j["BaseTemperature"].number_value()),
    pt_QuartzRawDensity(j["QuartzRawDensity"].number_value()),
    pt_DensityAir(j["DensityAir"].number_value()),
    pt_DensityWater(j["DensityWater"].number_value()),
    pt_DensityHumus(j["DensityHumus"].number_value()),
    pt_SpecificHeatCapacityAir(j["SpecificHeatCapacityAir"].number_value()),
    pt_SpecificHeatCapacityQuartz(j["SpecificHeatCapacityQuartz"].number_value()),
    pt_SpecificHeatCapacityWater(j["SpecificHeatCapacityWater"].number_value()),
    pt_SpecificHeatCapacityHumus(j["SpecificHeatCapacityHumus"].number_value()),
    pt_SoilAlbedo(j["SoilAlbedo"].number_value()),
    pt_SoilMoisture(j["SoilMoisture"].number_value())
{}

json11::Json UserSoilTemperatureParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserSoilTemperatureParameters"},
    {"NTau", pt_NTau},
    {"InitialSurfaceTemperature", pt_InitialSurfaceTemperature},
    {"BaseTemperature", pt_BaseTemperature},
    {"QuartzRawDensity", pt_QuartzRawDensity},
    {"DensityAir", pt_DensityAir},
    {"DensityWater", pt_DensityWater},
    {"DensityHumus", pt_DensityHumus},
    {"SpecificHeatCapacityAir", pt_SpecificHeatCapacityAir},
    {"SpecificHeatCapacityQuartz", pt_SpecificHeatCapacityQuartz},
    {"SpecificHeatCapacityWater", pt_SpecificHeatCapacityWater},
    {"SpecificHeatCapacityHumus", pt_SpecificHeatCapacityHumus},
    {"SoilAlbedo", pt_SoilAlbedo},
    {"SoilMoisture", pt_SoilMoisture}};
}

//-----------------------------------------------------------------------------------------

UserSoilTransportParameters::UserSoilTransportParameters(json11::Json j)
  : pq_DispersionLength(j["DispersionLength"].number_value()),
    pq_AD(j["AD"].number_value()),
    pq_DiffusionCoefficientStandard(j["DiffusionCoefficientStandard"].number_value()),
    pq_NDeposition(j["NDeposition"].number_value())
{}

json11::Json UserSoilTransportParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserSoilTransportParameters"},
    {"DispersionLength", pq_DispersionLength},
    {"AD", pq_AD},
    {"DiffusionCoefficientStandard", pq_DiffusionCoefficientStandard},
    {"NDeposition", pq_NDeposition}};
}

//-----------------------------------------------------------------------------------------

UserSoilOrganicParameters::UserSoilOrganicParameters(json11::Json j)
  : po_SOM_SlowDecCoeffStandard(j["SOM_SlowDecCoeffStandard"].number_value()),
    po_SOM_FastDecCoeffStandard(j["SOM_FastDecCoeffStandard"].number_value()),
    po_SMB_SlowMaintRateStandard(j["SMB_SlowMaintRateStandard"].number_value()),
    po_SMB_FastMaintRateStandard(j["SMB_FastMaintRateStandard"].number_value()),
    po_SMB_SlowDeathRateStandard(j["SMB_SlowDeathRateStandard"].number_value()),
    po_SMB_FastDeathRateStandard(j["SMB_FastDeathRateStandard"].number_value()),
    po_SMB_UtilizationEfficiency(j["SMB_UtilizationEfficiency"].number_value()),
    po_SOM_SlowUtilizationEfficiency(j["SOM_SlowUtilizationEfficiency"].number_value()),
    po_SOM_FastUtilizationEfficiency(j["SOM_FastUtilizationEfficiency"].number_value()),
    po_AOM_SlowUtilizationEfficiency(j["AOM_SlowUtilizationEfficiency"].number_value()),
    po_AOM_FastUtilizationEfficiency(j["AOM_FastUtilizationEfficiency"].number_value()),
    po_AOM_FastMaxC_to_N(j["AOM_FastMaxC_to_N"].number_value()),
    po_PartSOM_Fast_to_SOM_Slow(j["PartSOM_Fast_to_SOM_Slow"].number_value()),
    po_PartSMB_Slow_to_SOM_Fast(j["PartSMB_Slow_to_SOM_Fast"].number_value()),
    po_PartSMB_Fast_to_SOM_Fast(j["PartSMB_Fast_to_SOM_Fast"].number_value()),
    po_PartSOM_to_SMB_Slow(j["PartSOM_to_SMB_Slow"].number_value()),
    po_PartSOM_to_SMB_Fast(j["PartSOM_to_SMB_Fast"].number_value()),
    po_CN_Ratio_SMB(j["CN_Ratio_SMB"].number_value()),
    po_LimitClayEffect(j["LimitClayEffect"].number_value()),
    po_AmmoniaOxidationRateCoeffStandard(j["AmmoniaOxidationRateCoeffStandard"].number_value()),
    po_NitriteOxidationRateCoeffStandard(j["NitriteOxidationRateCoeffStandard"].number_value()),
    po_TransportRateCoeff(j["TransportRateCoeff"].number_value()),
    po_SpecAnaerobDenitrification(j["SpecAnaerobDenitrification"].number_value()),
    po_ImmobilisationRateCoeffNO3(j["ImmobilisationRateCoeffNO3"].number_value()),
    po_ImmobilisationRateCoeffNH4(j["ImmobilisationRateCoeffNH4"].number_value()),
    po_Denit1(j["Denit1"].number_value()),
    po_Denit2(j["Denit2"].number_value()),
    po_Denit3(j["Denit3"].number_value()),
    po_HydrolysisKM(j["HydrolysisKM"].number_value()),
    po_ActivationEnergy(j["ActivationEnergy"].number_value()),
    po_HydrolysisP1(j["HydrolysisP1"].number_value()),
    po_HydrolysisP2(j["HydrolysisP2"].number_value()),
    po_AtmosphericResistance(j["AtmosphericResistance"].number_value()),
    po_N2OProductionRate(j["N2OProductionRate"].number_value()),
    po_Inhibitor_NH3(j["Inhibitor_NH3"].number_value())
{}

json11::Json UserSoilOrganicParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserSoilOrganicParameters"},
    {"SOM_SlowDecCoeffStandard", J11Array {po_SOM_SlowDecCoeffStandard, "d-1"}},
    {"SOM_FastDecCoeffStandard", J11Array {po_SOM_FastDecCoeffStandard, "d-1"}},
    {"SMB_SlowMaintRateStandard", J11Array {po_SMB_SlowMaintRateStandard, "d-1"}},
    {"SMB_FastMaintRateStandard", J11Array {po_SMB_FastMaintRateStandard, "d-1"}},
    {"SMB_SlowDeathRateStandard", J11Array {po_SMB_SlowDeathRateStandard, "d-1"}},
    {"SMB_FastDeathRateStandard", J11Array {po_SMB_FastDeathRateStandard, "d-1"}},
    {"SMB_UtilizationEfficiency", J11Array {po_SMB_UtilizationEfficiency, "d-1"}},
    {"SOM_SlowUtilizationEfficiency", J11Array {po_SOM_SlowUtilizationEfficiency, ""}},
    {"SOM_FastUtilizationEfficiency", J11Array {po_SOM_FastUtilizationEfficiency, ""}},
    {"AOM_SlowUtilizationEfficiency", J11Array {po_AOM_SlowUtilizationEfficiency, ""}},
    {"AOM_FastUtilizationEfficiency", J11Array {po_AOM_FastUtilizationEfficiency, ""}},
    {"AOM_FastMaxC_to_N", J11Array {po_AOM_FastMaxC_to_N, ""}},
    {"PartSOM_Fast_to_SOM_Slow", J11Array {po_PartSOM_Fast_to_SOM_Slow, ""}},
    {"PartSMB_Slow_to_SOM_Fast", J11Array {po_PartSMB_Slow_to_SOM_Fast, ""}},
    {"PartSMB_Fast_to_SOM_Fast", J11Array {po_PartSMB_Fast_to_SOM_Fast, ""}},
    {"PartSOM_to_SMB_Slow", J11Array {po_PartSOM_to_SMB_Slow, ""}},
    {"PartSOM_to_SMB_Fast", J11Array {po_PartSOM_to_SMB_Fast, ""}},
    {"CN_Ratio_SMB", J11Array {po_CN_Ratio_SMB, ""}},
    {"LimitClayEffect", J11Array {po_LimitClayEffect, "kg kg-1"}},
    {"AmmoniaOxidationRateCoeffStandard", J11Array {po_AmmoniaOxidationRateCoeffStandard, "d-1"}},
    {"NitriteOxidationRateCoeffStandard", J11Array {po_NitriteOxidationRateCoeffStandard, "d-1"}},
    {"TransportRateCoeff", J11Array {po_TransportRateCoeff, "d-1"}},
    {"SpecAnaerobDenitrification", J11Array {po_SpecAnaerobDenitrification, "g gas-N g CO2-C-1"}},
    {"ImmobilisationRateCoeffNO3", J11Array {po_ImmobilisationRateCoeffNO3, "d-1"}},
    {"ImmobilisationRateCoeffNH4", J11Array {po_ImmobilisationRateCoeffNH4, "d-1"}},
    {"Denit1", J11Array {po_Denit1, ""}},
    {"Denit2", J11Array {po_Denit2, ""}},
    {"Denit3", J11Array {po_Denit3, ""}},
    {"HydrolysisKM", J11Array {po_HydrolysisKM, ""}},
    {"ActivationEnergy", J11Array {po_ActivationEnergy, ""}},
    {"HydrolysisP1", J11Array {po_HydrolysisP1, ""}},
    {"HydrolysisP2", J11Array {po_HydrolysisP2, ""}},
    {"AtmosphericResistance", J11Array {po_AtmosphericResistance, "s m-1"}},
    {"N2OProductionRate", J11Array {po_N2OProductionRate, "d-1"}},
    {"Inhibitor_NH3", J11Array {po_Inhibitor_NH3, "kg N m-3"}}};
}

//-----------------------------------------------------------------------------------------

SensitivityAnalysisParameters::SensitivityAnalysisParameters()
  : p_MeanFieldCapacity(UNDEFINED),
    p_MeanBulkDensity(UNDEFINED),
    p_HeatConductivityFrozen(UNDEFINED),
    p_HeatConductivityUnfrozen(UNDEFINED),
    p_LatentHeatTransfer(UNDEFINED),
    p_ReducedHydraulicConductivity(UNDEFINED),
    vs_FieldCapacity(UNDEFINED),
    vs_Saturation(UNDEFINED),
    vs_PermanentWiltingPoint(UNDEFINED),
    vs_SoilMoisture(UNDEFINED),
    vs_SoilTemperature(UNDEFINED),
    vc_SoilCoverage(UNDEFINED),
    vc_MaxRootingDepth(UNDEFINED),
    vc_RootDiameter(UNDEFINED),
    sa_crop_id(-1)
{
  crop_parameters.pc_InitialKcFactor = UNDEFINED;
  crop_parameters.pc_StageAtMaxHeight = UNDEFINED;
  crop_parameters.pc_CropHeightP1 = UNDEFINED;
  crop_parameters.pc_CropHeightP2 = UNDEFINED;
  crop_parameters.pc_LuxuryNCoeff = UNDEFINED;
  crop_parameters.pc_ResidueNRatio = UNDEFINED;
  crop_parameters.pc_CropSpecificMaxRootingDepth = UNDEFINED;
  crop_parameters.pc_RootPenetrationRate = UNDEFINED;
  crop_parameters.pc_RootGrowthLag = UNDEFINED;
  crop_parameters.pc_InitialRootingDepth = UNDEFINED;
  crop_parameters.pc_RootFormFactor = UNDEFINED;
  crop_parameters.pc_MaxNUptakeParam = UNDEFINED;
  crop_parameters.pc_CarboxylationPathway = UNDEFINED_INT;
  crop_parameters.pc_MaxAssimilationRate = UNDEFINED;
  crop_parameters.pc_MaxCropDiameter = UNDEFINED;
  crop_parameters.pc_MinimumNConcentration = UNDEFINED;
  crop_parameters.pc_NConcentrationB0 = UNDEFINED;
  crop_parameters.pc_NConcentrationPN =  UNDEFINED;
  crop_parameters.pc_NConcentrationRoot = UNDEFINED;
  crop_parameters.pc_PlantDensity = UNDEFINED;
  crop_parameters.pc_ResidueNRatio = UNDEFINED;

  organic_matter_parameters.vo_AOM_DryMatterContent = UNDEFINED;
  organic_matter_parameters.vo_AOM_NH4Content = UNDEFINED;
  organic_matter_parameters.vo_AOM_NO3Content = UNDEFINED;
  organic_matter_parameters.vo_AOM_CarbamidContent = UNDEFINED;
  organic_matter_parameters.vo_PartAOM_to_AOM_Slow = UNDEFINED;
  organic_matter_parameters.vo_PartAOM_to_AOM_Fast = UNDEFINED;
  organic_matter_parameters.vo_CN_Ratio_AOM_Slow = UNDEFINED;
  organic_matter_parameters.vo_CN_Ratio_AOM_Fast = UNDEFINED;
}


//-----------------------------------------------------------------------------------------

CentralParameterProvider::CentralParameterProvider()
  : precipCorrectionValues(MONTH - 1, 1.0)
{

}

/**
 * @brief Returns a precipitation correction value for a specific month.
 * @param month Month
 * @return Correction value that should be applied to precipitation value read from database.
 */
double CentralParameterProvider::getPrecipCorrectionValue(int month) const
{
  assert(month<12);
  assert(month>=0);

	return precipCorrectionValues.at(month);
	//cerr << "Requested correction value for precipitation for an invalid month.\nMust be in range of 0<=value<12." << endl;
  //return 1.0;
}

/**
 * Sets a correction value for a specific month.
 * @param month Month the value should be used for.
 * @param value Correction value that should be added.
 */
void CentralParameterProvider::setPrecipCorrectionValue(int month, double value)
{
  assert(month<12);
  assert(month>=0);
  precipCorrectionValues[month]=value;

  // debug
  //  cout << "Added precip correction value for month " << month << ":\t " << value << endl;
}

// --------------------------------------------------------------------
