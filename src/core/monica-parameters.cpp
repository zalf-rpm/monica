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
    yieldPercentage(Tools::double_value(j, "yieldPercentage")),
    yieldDryMatter(Tools::double_value(j, "yieldDryMatter"))
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

SpeciesParameters::SpeciesParameters(json11::Json j)
  : pc_SpeciesId(string_value(j, "SpeciesName"))
//  , pc_NumberOfDevelopmentalStages(int_value(j, "NumberOfDevelopmentalStages"))
//  , pc_NumberOfOrgans(int_value(j, "NumberOfOrgans"))
  , pc_CarboxylationPathway(int_value(j, "CarboxylationPathway"))
  , pc_DefaultRadiationUseEfficiency(double_value(j, "DefaultRadiationUseEfficiency"))
  , pc_PartBiologicalNFixation(double_value(j, "PartBiologicalNFixation"))
  , pc_InitialKcFactor(double_value(j, "InitialKcFactor"))
  , pc_LuxuryNCoeff(double_value(j, "LuxuryNCoeff"))
  , pc_MaxCropDiameter(double_value(j, "MaxCropDiameter"))
  , pc_StageAtMaxHeight(double_value(j, "StageAtMaxHeight"))
  , pc_StageAtMaxDiameter(double_value(j, "StageAtMaxDiameter"))
  , pc_MinimumNConcentration(double_value(j, "MinimumNConcentration"))
  , pc_MinimumTemperatureForAssimilation(double_value(j, "MinimumTemperatureForAssimilation"))
  , pc_NConcentrationAbovegroundBiomass(double_value(j, "NConcentrationAbovegroundBiomass"))
  , pc_NConcentrationB0(double_value(j, "NConcentrationB0"))
  , pc_NConcentrationPN(double_value(j, "NConcentrationPN"))
  , pc_NConcentrationRoot(double_value(j, "NConcentrationRoot"))
  , pc_DevelopmentAccelerationByNitrogenStress(int_value(j, "DevelopmentAccelerationByNitrogenStress"))
  , pc_FieldConditionModifier(double_value(j, "FieldConditionModifier"))
  , pc_AssimilateReallocation(double_value(j, "AssimilateReallocation"))
  , pc_BaseTemperature(double_vector(j, "BaseTemperature"))
  , pc_OrganMaintenanceRespiration(double_vector(j, "OrganMaintenanceRespiration"))
  , pc_OrganGrowthRespiration(double_vector(j, "OrganGrowthRespiration"))
  , pc_StageMaxRootNConcentration(double_vector(j, "StageMaxRootNConcentration"))
  , pc_InitialOrganBiomass(double_vector(j, "InitialOrganBiomass"))
  , pc_CriticalOxygenContent(double_vector(j, "CriticalOxygenContent"))
  , pc_AbovegroundOrgan(bool_vector(j, "AbovegroundOrgan"))
  , pc_StorageOrgan(bool_vector(j, "StorageOrgan"))
  , pc_SamplingDepth(double_value(j, "SamplingDepth"))
  , pc_TargetNSamplingDepth(double_value(j, "TargetNSamplingDepth"))
  , pc_TargetN30(double_value(j, "TargetN30"))
  , pc_MaxNUptakeParam(double_value(j, "MaxNUptakeParam"))
  , pc_RootDistributionParam(double_value(j, "RootDistributionParam"))
  , pc_PlantDensity(double_value(j, "PlantDensity"))
  , pc_RootGrowthLag(double_value(j, "RootGrowthLag"))
  , pc_MinimumTemperatureRootGrowth(double_value(j, "MinimumTemperatureRootGrowth"))
  , pc_InitialRootingDepth(double_value(j, "InitialRootingDepth"))
  , pc_RootPenetrationRate(double_value(j, "RootPenetrationRate"))
  , pc_RootFormFactor(double_value(j, "RootFormFactor"))
  , pc_SpecificRootLength(double_value(j, "SpecificRootLength"))
  , pc_StageAfterCut(int_value(j, "StageAfterCut"))
  , pc_LimitingTemperatureHeatStress(double_value(j, "LimitingTemperatureHeatStress"))
  , pc_CuttingDelayDays(int_value(j, "CuttingDelayDays"))
  , pc_DroughtImpactOnFertilityFactor(double_value(j, "DroughtImpactOnFertilityFactor"))
{
}

json11::Json SpeciesParameters::to_json() const
{
  auto species = J11Object {
  {"type", "SpeciesParameters"},
  {"SpeciesName", pc_SpeciesId},
//  {"NumberOfDevelopmentalStages", pc_NumberOfDevelopmentalStages},
//  {"NumberOfOrgans", pc_NumberOfOrgans},
  {"CarboxylationPathway", pc_CarboxylationPathway},
  {"DefaultRadiationUseEfficiency", pc_DefaultRadiationUseEfficiency},
  {"PartBiologicalNFixation", pc_PartBiologicalNFixation},
  {"InitialKcFactor", pc_InitialKcFactor},
  {"LuxuryNCoeff", pc_LuxuryNCoeff},
  {"MaxCropDiameter", pc_MaxCropDiameter},
  {"StageAtMaxHeight", pc_StageAtMaxHeight},
  {"StageAtMaxDiameter", pc_StageAtMaxDiameter},
  {"MinimumNConcentration", pc_MinimumNConcentration},
  {"MinimumTemperatureForAssimilation", pc_MinimumTemperatureForAssimilation},
  {"NConcentrationAbovegroundBiomass", pc_NConcentrationAbovegroundBiomass},
  {"NConcentrationB0", pc_NConcentrationB0},
  {"NConcentrationPN", pc_NConcentrationPN},
  {"NConcentrationRoot", pc_NConcentrationRoot},
  {"DevelopmentAccelerationByNitrogenStress", pc_DevelopmentAccelerationByNitrogenStress},
  {"FieldConditionModifier", pc_FieldConditionModifier},
  {"AssimilateReallocation", pc_AssimilateReallocation},
  {"BaseTemperature", toPrimJsonArray(pc_BaseTemperature)},
  {"OrganMaintenanceRespiration", toPrimJsonArray(pc_OrganMaintenanceRespiration)},
  {"OrganGrowthRespiration", toPrimJsonArray(pc_OrganGrowthRespiration)},
  {"StageMaxRootNConcentration", toPrimJsonArray(pc_StageMaxRootNConcentration)},
  {"InitialOrganBiomass", toPrimJsonArray(pc_InitialOrganBiomass)},
  {"CriticalOxygenContent", toPrimJsonArray(pc_CriticalOxygenContent)},
  {"AbovegroundOrgan", toPrimJsonArray(pc_AbovegroundOrgan)},
  {"StorageOrgan", toPrimJsonArray(pc_StorageOrgan)},
  {"SamplingDepth", pc_SamplingDepth},
  {"TargetNSamplingDepth", pc_TargetNSamplingDepth},
  {"TargetN30", pc_TargetN30},
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
  {"LimitingTemperatureHeatStress", pc_LimitingTemperatureHeatStress},
  {"CuttingDelayDays", pc_CuttingDelayDays},
  {"DroughtImpactOnFertilityFactor", pc_DroughtImpactOnFertilityFactor}};

  return species;
}

//------------------------------------------------------------------------------

CultivarParameters::CultivarParameters(json11::Json j)
  : pc_CultivarId(string_value(j, "CultivarName"))
  , pc_Description(string_value(j, "Description"))
  , pc_Perennial(bool_value(j, "Perennial"))
  , pc_MaxAssimilationRate(double_value(j, "MaxAssimilationRate"))
  , pc_MaxCropHeight(double_value(j, "MaxCropHeight"))
  , pc_ResidueNRatio(double_value(j, "ResidueNRatio"))
  , pc_LT50cultivar(double_value(j, "LT50cultivar"))
  , pc_CropHeightP1(double_value(j, "CropHeightP1"))
  , pc_CropHeightP2(double_value(j, "CropHeightP2"))
  , pc_CropSpecificMaxRootingDepth(double_value(j, "CropSpecificMaxRootingDepth"))
//  , std::vector<std::vector<double>> pc_AssimilatePartitioningCoeff,
  , pc_BaseDaylength(double_vector(j, "BaseDaylength"))
  , pc_OptimumTemperature(double_vector(j, "OptimumTemperature"))
  , pc_DaylengthRequirement(double_vector(j, "DaylengthRequirement"))
  , pc_DroughtStressThreshold(double_vector(j, "DroughtStressThreshold"))
  , pc_SpecificLeafArea(double_vector(j, "SpecificLeafArea"))
  , pc_StageKcFactor(double_vector(j, "StageKcFactor"))
  , pc_StageTemperatureSum(double_vector(j, "StageTemperatureSum"))
  , pc_VernalisationRequirement(double_vector(j, "VernalisationRequirement"))
  , pc_HeatSumIrrigationStart(double_value(j, "HeatSumIrrigationStart"))
  , pc_HeatSumIrrigationEnd(double_value(j, "HeatSumIrrigationEnd"))
  , pc_CriticalTemperatureHeatStress(double_value(j, "CriticalTemperatureHeatStress"))
  , pc_BeginSensitivePhaseHeatStress(double_value(j, "BeginSensitivePhaseHeatStress"))
  , pc_EndSensitivePhaseHeatStress(double_value(j, "EndSensitivePhaseHeatStress"))
  , pc_FrostHardening(double_value(j, "FrostHardening"))
  , pc_FrostDehardening(double_value(j, "FrostDehardening"))
  , pc_LowTemperatureExposure(double_value(j, "LowTemperatureExposure"))
  , pc_RespiratoryStress(double_value(j, "RespiratoryStress"))
  , pc_LatestHarvestDoy(int_value(j, "LatestHarvestDoy"))
  , pc_OrganIdsForPrimaryYield(toVector<YieldComponent>(j["OrganIdsForPrimaryYield"]))
  , pc_OrganIdsForSecondaryYield(toVector<YieldComponent>(j["OrganIdsForSecondaryYield"]))
  , pc_OrganIdsForCutting(toVector<YieldComponent>(j["OrganIdsForCutting"]))
{
  for(auto js : j["AssimilatePartitioningCoeff"].array_items())
    pc_AssimilatePartitioningCoeff.push_back(double_vector(js));
  for(auto js : j["OrganSenescenceRate"].array_items())
    pc_OrganSenescenceRate.push_back(double_vector(js));
}
json11::Json CultivarParameters::to_json() const
{
  J11Array apcs;
  for(auto v : pc_AssimilatePartitioningCoeff)
    apcs.push_back(toPrimJsonArray(v));

  J11Array osrs;
  for(auto v : pc_OrganSenescenceRate)
    osrs.push_back(toPrimJsonArray(v));

  auto cultivar = J11Object {
  {"type", "CultivarParameters"},
  {"CultivarName", pc_CultivarId},
  {"Description", pc_Description},
  {"Perennial", pc_Perennial},
  {"MaxAssimilationRate", pc_MaxAssimilationRate},
  {"MaxCropHeight", J11Array {pc_MaxCropHeight, "m"}},
  {"ResidueNRatio", pc_ResidueNRatio},
  {"LT50cultivar", pc_LT50cultivar},
  {"CropHeightP1", pc_CropHeightP1},
  {"CropHeightP2", pc_CropHeightP2},
  {"CropSpecificMaxRootingDepth", pc_CropSpecificMaxRootingDepth},
  {"AssimilatePartitioningCoeff", apcs},
  {"OrganSenescenceRate", osrs},
  {"BaseDaylength", J11Array {toPrimJsonArray(pc_BaseDaylength), "h"}},
  {"OptimumTemperature", J11Array {toPrimJsonArray(pc_OptimumTemperature), "°C"}},
  {"DaylengthRequirement", J11Array {toPrimJsonArray(pc_DaylengthRequirement), "h"}},
  {"DroughtStressThreshold", toPrimJsonArray(pc_DroughtStressThreshold)},
  {"SpecificLeafArea", J11Array {toPrimJsonArray(pc_SpecificLeafArea), "ha kg-1"}},
  {"StageKcFactor", J11Array {toPrimJsonArray(pc_StageKcFactor), "1;0"}},
  {"StageTemperatureSum", J11Array {toPrimJsonArray(pc_StageTemperatureSum), "°C d"}},
  {"VernalisationRequirement", toPrimJsonArray(pc_VernalisationRequirement)},
  {"HeatSumIrrigationStart", pc_HeatSumIrrigationStart},
  {"HeatSumIrrigationEnd", pc_HeatSumIrrigationEnd},
  {"CriticalTemperatureHeatStress", J11Array {pc_CriticalTemperatureHeatStress, "°C"}},
  {"BeginSensitivePhaseHeatStress", J11Array {pc_BeginSensitivePhaseHeatStress, "°C d"}},
  {"EndSensitivePhaseHeatStress", J11Array {pc_EndSensitivePhaseHeatStress, "°C d"}},
  {"FrostHardening", pc_FrostHardening},
  {"FrostDehardening", pc_FrostDehardening},
  {"LowTemperatureExposure", pc_LowTemperatureExposure},
  {"RespiratoryStress", pc_RespiratoryStress},
  {"LatestHarvestDoy", pc_LatestHarvestDoy},
  {"OrganIdsForPrimaryYield", toJsonArray(pc_OrganIdsForPrimaryYield)},
  {"OrganIdsForSecondaryYield", toJsonArray(pc_OrganIdsForSecondaryYield)},
  {"OrganIdsForCutting", toJsonArray(pc_OrganIdsForCutting)}};

  return cultivar;
}

//------------------------------------------------------------------------------

CropParameters::CropParameters(json11::Json j)
  : CropParameters(j["species"], j["cultivar"])
{}

CropParameters::CropParameters(json11::Json sj, json11::Json cj)
  : speciesParams(sj)
  , cultivarParams(cj)
{}

json11::Json CropParameters::to_json() const
{
  return J11Object {
    {"type", "CropParameters"},
    {"species", speciesParams.to_json()},
    {"cultivar", cultivarParams.to_json()}};
}


//------------------------------------------------------------------------------

MineralFertiliserParameters::MineralFertiliserParameters(const string& id,
                                                         const std::string& name,
                                                         double carbamid,
                                                         double no3,
                                                         double nh4)
  : id(id),
    name(name),
    vo_Carbamid(carbamid),
    vo_NH4(nh4),
    vo_NO3(no3)
{}

MineralFertiliserParameters::MineralFertiliserParameters(json11::Json j)
  : id(string_value(j, "id")),
    name(string_value(j, "name")),
    vo_Carbamid(double_value(j, "Carbamid")),
    vo_NH4(double_value(j, "NH4")),
    vo_NO3(double_value(j, "NO3"))
{}

json11::Json MineralFertiliserParameters::to_json() const
{
  return J11Object {
    {"type", "MineralFertiliserParameters"},
    {"id", id},
    {"name", name},
    {"Carbamid", vo_Carbamid},
    {"NH4", vo_NH4},
    {"NO3", vo_NO3}};
}

//-----------------------------------------------------------------------------------------

NMinUserParameters::NMinUserParameters(double min,
                                       double max,
                                       int delayInDays)
  : min(min),
    max(max),
    delayInDays(delayInDays) { }

NMinUserParameters::NMinUserParameters(json11::Json j)
  : min(double_value(j, "min")),
    max(double_value(j, "max")),
    delayInDays(int_value(j, "delayInDays"))
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
  : nitrateConcentration(double_value(j, "nitrateConcentration")),
    sulfateConcentration(double_value(j, "sulfateConcentration"))
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
    amount(double_value(j, "amount")),
    treshold(double_value(j, "treshold"))
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
	string err;
  if(j.has_shape({{"groundwaterInfo", json11::Json::OBJECT}}, err))
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

SiteParameters::SiteParameters(json11::Json j)
  : vs_Latitude(double_value(j, "Latitude")),
    vs_Slope(double_value(j, "Slope")),
    vs_HeightNN(double_value(j, "HeightNN")),
    vs_GroundwaterDepth(double_value(j, "GroundwaterDepth")),
    vs_Soil_CN_Ratio(double_value(j, "Soil_CN_Ratio")),
    vs_DrainageCoeff(double_value(j, "DrainageCoeff")),
    vq_NDeposition(double_value(j, "NDeposition")),
    vs_MaxEffectiveRootingDepth(double_value(j, "MaxEffectiveRootingDepth"))
{
  string err;
  if(j.has_shape({{"SoilParameters", json11::Json::ARRAY}}, err))
  {
    vs_SoilParameters = make_shared<SoilPMs>();
    for(auto sp : j["SoilParameters"].array_items())
      vs_SoilParameters->push_back(sp);
  }
}

json11::Json SiteParameters::to_json() const
{
  auto sps = J11Object {
  {"type", "SiteParameters"},
  {"Latitude", J11Array {vs_Latitude, "", "latitude in decimal degrees"}},
  {"Slope", J11Array {vs_Slope, "m m-1"}},
  {"HeightNN", J11Array {vs_HeightNN, "m", "height above sea level"}},
  {"GroundwaterDepth", J11Array {vs_GroundwaterDepth, "m"}},
  {"Soil_CN_Ratio", vs_Soil_CN_Ratio},
  {"DrainageCoeff", vs_DrainageCoeff},
  {"NDeposition", vq_NDeposition},
  {"MaxEffectiveRootingDepth", vs_MaxEffectiveRootingDepth}};

  if(vs_SoilParameters)
    sps["SoilParameters"] = toJsonArray(*vs_SoilParameters);

  return sps;
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
  : samplingDepth(double_value(j, "samplingDepth")),
    nTarget(double_value(j, "nTarget")),
    nTarget30(double_value(j, "nTarget30"))
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
  : vo_AOM_DryMatterContent(double_value(j, "AOM_DryMatterContent"))
  , vo_AOM_NH4Content(double_value(j, "AOM_NH4Content"))
  , vo_AOM_NO3Content(double_value(j, "AOM_NO3Content"))
  , vo_AOM_CarbamidContent(double_value(j, "AOM_CarbamidContent"))
  , vo_AOM_SlowDecCoeffStandard(double_value(j, "AOM_SlowDecCoeffStandard"))
  , vo_AOM_FastDecCoeffStandard(double_value(j, "AOM_FastDecCoeffStandard"))
  , vo_PartAOM_to_AOM_Slow(double_value(j, "PartAOM_to_AOM_Slow"))
  , vo_PartAOM_to_AOM_Fast(double_value(j, "PartAOM_to_AOM_Fast"))
  , vo_CN_Ratio_AOM_Slow(double_value(j, "CN_Ratio_AOM_Slow"))
  , vo_CN_Ratio_AOM_Fast(double_value(j, "CN_Ratio_AOM_Fast"))
  , vo_PartAOM_Slow_to_SMB_Slow(double_value(j, "PartAOM_Slow_to_SMB_Slow"))
  , vo_PartAOM_Slow_to_SMB_Fast(double_value(j, "PartAOM_Slow_to_SMB_Fast"))
  , vo_NConcentration(double_value(j, "NConcentration"))
{}

json11::Json OrganicMatterParameters::to_json() const
{
  return J11Object {
    {"type", "OrganicMatterParameters"},
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

OrganicFertiliserParameters::OrganicFertiliserParameters(json11::Json j)
  : OrganicMatterParameters(j)
  , id(string_value(j, "id"))
  , name(string_value(j, "name"))
{}

json11::Json OrganicFertiliserParameters::to_json() const
{
  auto omp = OrganicMatterParameters::to_json().object_items();
  omp["type"] = "OrganicFertiliserParameters";
  omp["id"] = id;
  omp["name"] = name;
  return omp;
}

//-----------------------------------------------------------------------------------------

CropResidueParameters::CropResidueParameters(json11::Json j)
  : OrganicMatterParameters(j)
  , species(string_value(j, "species"))
  , cultivar(string_value(j, "cultivar"))
{}

json11::Json CropResidueParameters::to_json() const
{
  auto omp = OrganicMatterParameters::to_json().object_items();
  omp["type"] = "CropResidueParameters";
  omp["species"] = species;
  omp["cultivar"] = cultivar;
  return omp;
}

//-----------------------------------------------------------------------------------------

UserCropParameters::UserCropParameters(json11::Json j)
  : pc_CanopyReflectionCoefficient(double_value(j, "CanopyReflectionCoefficient"))
  , pc_ReferenceMaxAssimilationRate(double_value(j, "ReferenceMaxAssimilationRate"))
  , pc_ReferenceLeafAreaIndex(double_value(j, "ReferenceLeafAreaIndex"))
  , pc_MaintenanceRespirationParameter1(double_value(j, "MaintenanceRespirationParameter1"))
  , pc_MaintenanceRespirationParameter2(double_value(j, "MaintenanceRespirationParameter2"))
  , pc_MinimumNConcentrationRoot(double_value(j, "MinimumNConcentrationRoot"))
  , pc_MinimumAvailableN(double_value(j, "MinimumAvailableN"))
  , pc_ReferenceAlbedo(double_value(j, "ReferenceAlbedo"))
  , pc_StomataConductanceAlpha(double_value(j, "StomataConductanceAlpha"))
  , pc_SaturationBeta(double_value(j, "SaturationBeta"))
  , pc_GrowthRespirationRedux(double_value(j, "GrowthRespirationRedux"))
  , pc_MaxCropNDemand(double_value(j, "MaxCropNDemand"))
  , pc_GrowthRespirationParameter1(double_value(j, "GrowthRespirationParameter1"))
  , pc_GrowthRespirationParameter2(double_value(j, "GrowthRespirationParameter2"))
  , pc_Tortuosity(double_value(j, "Tortuosity"))
  , pc_NitrogenResponseOn(bool_value(j, "NitrogenResponseOn"))
  , pc_WaterDeficitResponseOn(bool_value(j, "WaterDeficitResponseOn"))
  , pc_EmergenceFloodingControlOn(bool_value(j, "EmergenceFloodingControlOn"))
  , pc_EmergenceMoistureControlOn(bool_value(j, "EmergenceMoistureControlOn"))

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
    {"Tortuosity", pc_Tortuosity},
    {"NitrogenResponseOn", pc_NitrogenResponseOn},
    {"WaterDeficitResponseOn", pc_WaterDeficitResponseOn},
    {"EmergenceFloodingControlOn", pc_EmergenceFloodingControlOn},
    {"EmergenceMoistureControlOn", pc_EmergenceMoistureControlOn},
  };
}

//-----------------------------------------------------------------------------------------

UserEnvironmentParameters::UserEnvironmentParameters(json11::Json j)
  : p_UseAutomaticIrrigation(bool_value(j, "UseAutomaticIrrigation"))
  , p_AutoIrrigationParams(j["AutoIrrigationParams"])
  , p_UseNMinMineralFertilisingMethod(bool_value(j, "UseNMinMineralFertilisingMethod"))
  , p_NMinFertiliserPartition(j["NMinFertiliserPartition"])
  , p_NMinUserParams(j["NMinUserParams"])
  , p_UseSecondaryYields(bool_value(j, "UseSecondaryYields"))
  , p_UseAutomaticHarvestTrigger(bool_value(j, "UseAutomaticHarvestTrigger"))
  , p_NumberOfLayers(int_value(j, "NumberOfLayers"))
  , p_LayerThickness(double_value(j, "LayerThickness"))
  , p_Albedo(double_value(j, "Albedo"))
  , p_AtmosphericCO2(double_value(j, "AthmosphericCO2"))
  , p_WindSpeedHeight(double_value(j, "WindSpeedHeight"))
  , p_LeachingDepth(double_value(j, "LeachingDepth"))
  , p_timeStep(double_value(j, "timeStep"))
  , p_MaxGroundwaterDepth(double_value(j, "MaxGroundwaterDepth"))
  , p_MinGroundwaterDepth(double_value(j, "MinGroundwaterDepth"))
  , p_MinGroundwaterDepthMonth(int_value(j, "MinGroundwaterDepthMonth"))
  , p_StartPVIndex(int_value(j, "StartPVIndex"))
  , p_JulianDayAutomaticFertilising(int_value(j, "JulianDayAutomaticFertilising"))
{}

json11::Json UserEnvironmentParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserEnvironmentParameters"},
    {"UseAutomaticIrrigation", p_UseAutomaticIrrigation},
    {"AutoIrrigationParams", p_AutoIrrigationParams},
    {"UseNMinMineralFertilisingMethod", p_UseNMinMineralFertilisingMethod},
    {"NMinFertiliserPartition", p_NMinFertiliserPartition},
    {"NMinUserParams", p_NMinUserParams},
    {"UseSecondaryYields", p_UseSecondaryYields},
    {"UseAutomaticHarvestTrigger", p_UseAutomaticHarvestTrigger},
    {"NumberOfLayers", p_NumberOfLayers},
    {"LayerThickness", p_LayerThickness},
    {"Albedo", p_Albedo},
    {"AthmosphericCO2", p_AtmosphericCO2},
    {"WindSpeedHeight", p_WindSpeedHeight},
    {"LeachingDepth", p_LeachingDepth},
    {"timeStep", p_timeStep},
    {"MaxGroundwaterDepth", p_MaxGroundwaterDepth},
    {"MinGroundwaterDepth", p_MinGroundwaterDepth},
    {"MinGroundwaterDepthMonth", p_MinGroundwaterDepthMonth},
    {"StartPVIndex", p_StartPVIndex},
    {"JulianDayAutomaticFertilising", p_JulianDayAutomaticFertilising}
  };
}

//-----------------------------------------------------------------------------------------

UserSoilMoistureParameters::UserSoilMoistureParameters(json11::Json j)
  : pm_CriticalMoistureDepth(double_value(j, "CriticalMoistureDepth")),
    pm_SaturatedHydraulicConductivity(double_value(j, "SaturatedHydraulicConductivity")),
    pm_SurfaceRoughness(double_value(j, "SurfaceRoughness")),
    pm_GroundwaterDischarge(double_value(j, "GroundwaterDischarge")),
    pm_HydraulicConductivityRedux(double_value(j, "HydraulicConductivityRedux")),
    pm_SnowAccumulationTresholdTemperature(double_value(j, "SnowAccumulationTresholdTemperature")),
    pm_KcFactor(double_value(j, "KcFactor")),
    pm_TemperatureLimitForLiquidWater(double_value(j, "TemperatureLimitForLiquidWater")),
    pm_CorrectionSnow(double_value(j, "CorrectionSnow")),
    pm_CorrectionRain(double_value(j, "CorrectionRain")),
    pm_SnowMaxAdditionalDensity(double_value(j, "SnowMaxAdditionalDensity")),
    pm_NewSnowDensityMin(double_value(j, "NewSnowDensityMin")),
    pm_SnowRetentionCapacityMin(double_value(j, "SnowRetentionCapacityMin")),
    pm_RefreezeParameter1(double_value(j, "RefreezeParameter1")),
    pm_RefreezeParameter2(double_value(j, "RefreezeParameter2")),
    pm_RefreezeTemperature(double_value(j, "RefreezeTemperature")),
    pm_SnowMeltTemperature(double_value(j, "SnowMeltTemperature")),
    pm_SnowPacking(double_value(j, "SnowPacking")),
    pm_SnowRetentionCapacityMax(double_value(j, "SnowRetentionCapacityMax")),
    pm_EvaporationZeta(double_value(j, "EvaporationZeta")),
    pm_XSACriticalSoilMoisture(double_value(j, "XSACriticalSoilMoisture")),
    pm_MaximumEvaporationImpactDepth(double_value(j, "MaximumEvaporationImpactDepth")),
    pm_MaxPercolationRate(double_value(j, "MaxPercolationRate")),
    pm_MoistureInitValue(double_value(j, "MoistureInitValue"))
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
  : pt_NTau(double_value(j, "NTau")),
    pt_InitialSurfaceTemperature(double_value(j, "InitialSurfaceTemperature")),
    pt_BaseTemperature(double_value(j, "BaseTemperature")),
    pt_QuartzRawDensity(double_value(j, "QuartzRawDensity")),
    pt_DensityAir(double_value(j, "DensityAir")),
    pt_DensityWater(double_value(j, "DensityWater")),
    pt_DensityHumus(double_value(j, "DensityHumus")),
    pt_SpecificHeatCapacityAir(double_value(j, "SpecificHeatCapacityAir")),
    pt_SpecificHeatCapacityQuartz(double_value(j, "SpecificHeatCapacityQuartz")),
    pt_SpecificHeatCapacityWater(double_value(j, "SpecificHeatCapacityWater")),
    pt_SpecificHeatCapacityHumus(double_value(j, "SpecificHeatCapacityHumus")),
    pt_SoilAlbedo(double_value(j, "SoilAlbedo")),
    pt_SoilMoisture(double_value(j, "SoilMoisture"))
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
  : pq_DispersionLength(double_value(j, "DispersionLength")),
    pq_AD(double_value(j, "AD")),
    pq_DiffusionCoefficientStandard(double_value(j, "DiffusionCoefficientStandard")),
    pq_NDeposition(double_value(j, "NDeposition"))
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
  : po_SOM_SlowDecCoeffStandard(double_value(j, "SOM_SlowDecCoeffStandard"))
  , po_SOM_FastDecCoeffStandard(double_value(j, "SOM_FastDecCoeffStandard"))
  , po_SMB_SlowMaintRateStandard(double_value(j, "SMB_SlowMaintRateStandard"))
  , po_SMB_FastMaintRateStandard(double_value(j, "SMB_FastMaintRateStandard"))
  , po_SMB_SlowDeathRateStandard(double_value(j, "SMB_SlowDeathRateStandard"))
  , po_SMB_FastDeathRateStandard(double_value(j, "SMB_FastDeathRateStandard"))
  , po_SMB_UtilizationEfficiency(double_value(j, "SMB_UtilizationEfficiency"))
  , po_SOM_SlowUtilizationEfficiency(double_value(j, "SOM_SlowUtilizationEfficiency"))
  , po_SOM_FastUtilizationEfficiency(double_value(j, "SOM_FastUtilizationEfficiency"))
  , po_AOM_SlowUtilizationEfficiency(double_value(j, "AOM_SlowUtilizationEfficiency"))
  , po_AOM_FastUtilizationEfficiency(double_value(j, "AOM_FastUtilizationEfficiency"))
  , po_AOM_FastMaxC_to_N(double_value(j, "AOM_FastMaxC_to_N"))
  , po_PartSOM_Fast_to_SOM_Slow(double_value(j, "PartSOM_Fast_to_SOM_Slow"))
  , po_PartSMB_Slow_to_SOM_Fast(double_value(j, "PartSMB_Slow_to_SOM_Fast"))
  , po_PartSMB_Fast_to_SOM_Fast(double_value(j, "PartSMB_Fast_to_SOM_Fast"))
  , po_PartSOM_to_SMB_Slow(double_value(j, "PartSOM_to_SMB_Slow"))
  , po_PartSOM_to_SMB_Fast(double_value(j, "PartSOM_to_SMB_Fast"))
  , po_CN_Ratio_SMB(double_value(j, "CN_Ratio_SMB"))
  , po_LimitClayEffect(double_value(j, "LimitClayEffect"))
  , po_AmmoniaOxidationRateCoeffStandard(double_value(j, "AmmoniaOxidationRateCoeffStandard"))
  , po_NitriteOxidationRateCoeffStandard(double_value(j, "NitriteOxidationRateCoeffStandard"))
  , po_TransportRateCoeff(double_value(j, "TransportRateCoeff"))
  , po_SpecAnaerobDenitrification(double_value(j, "SpecAnaerobDenitrification"))
  , po_ImmobilisationRateCoeffNO3(double_value(j, "ImmobilisationRateCoeffNO3"))
  , po_ImmobilisationRateCoeffNH4(double_value(j, "ImmobilisationRateCoeffNH4"))
  , po_Denit1(double_value(j, "Denit1"))
  , po_Denit2(double_value(j, "Denit2"))
  , po_Denit3(double_value(j, "Denit3"))
  , po_HydrolysisKM(double_value(j, "HydrolysisKM"))
  , po_ActivationEnergy(double_value(j, "ActivationEnergy"))
  , po_HydrolysisP1(double_value(j, "HydrolysisP1"))
  , po_HydrolysisP2(double_value(j, "HydrolysisP2"))
  , po_AtmosphericResistance(double_value(j, "AtmosphericResistance"))
  , po_N2OProductionRate(double_value(j, "N2OProductionRate"))
  , po_Inhibitor_NH3(double_value(j, "Inhibitor_NH3"))
  , ps_MaxMineralisationDepth(double_value(j, "MaxMineralisationDepth"))
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
    {"Inhibitor_NH3", J11Array {po_Inhibitor_NH3, "kg N m-3"}},
    {"MaxMineralisationDepth", ps_MaxMineralisationDepth}
  };
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
