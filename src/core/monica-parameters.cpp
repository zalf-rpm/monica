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

#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <mutex>

#include "monica-parameters.h"
#include "db/abstract-db-connections.h"
#include "climate/climate-common.h"
#include "tools/helper.h"
#include "tools/algorithms.h"
#include "tools/debug.h"
#include "soil/conversion.h"
#include "soil/soil.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Soil;
using namespace Tools;
using namespace Climate;
using namespace json11;

/*
const map<string, Result2>& Monica::result2()
{
	static map<string, Result2> res2 = 
	{{"Date", {0, "Date", "", "Date"}},
		{"Crop", {1, "Crop", "", "Crop name"}},
		{"TraDef", {2, "TraDef", "0;1", "TranspirationDeficit"}},
		{"Tra", {3, "Tra", "mm", "ActualTranspiration"}},
		{"NDef", {4, "NDef", "0;1", "CropNRedux"}},
		{"HeatRed", {5, "HeatRed", "0;1", " HeatStressRedux"}},
	{"FrostRed", {6, "FrostRed", "0;1", "FrostStressRedux"}},
	{"OxRed", {7, "OxRed", "0;1", "OxygenDeficit"}},

	{"Stage", {8, "Stage", "", "DevelopmentalStage"}},
	{"TempSum", {9, "TempSum", "°Cd", "CurrentTemperatureSum"}},
	{"VernF", {10, "VernF", "0;1", "VernalisationFactor"}},
	{"DaylF", {11, "DaylF", "0;1", "DaylengthFactor"}},
	{"IncRoot", {12, "IncRoot", "kg ha-1", "OrganGrowthIncrement root"}},
	{"IncLeaf", {13, "IncLeaf", "kg ha-1", "OrganGrowthIncrement leaf"}},
	{"IncShoot", {14, "IncShoot", "kg ha-1", "OrganGrowthIncrement shoot"}},
	{"IncFruit", {15, "IncFruit", "kg ha-1", "OrganGrowthIncrement fruit"}},
	{"RelDev", {16, "RelDev", "0;1", "RelativeTotalDevelopment"}},
	{"LT50", {17, "LT50", "°C", "LT50"}},
	{"AbBiom", {18, "AbBiom", "kg ha-1", "AbovegroundBiomass"}},
	{"Root", {19, "Root", "kgDM ha-1", "get_OrganBiomass(i)"}},
	{"Leaf", {20, "Leaf", "kgDM ha-1", "get_OrganBiomass(i)"}},
	{"Shoot", {21, "Shoot", "kgDM ha-1", "get_OrganBiomass(i)"}},
	{"Fruit", {22, "Fruit", "kgDM ha-1", "get_OrganBiomass(i)"}},
	{"Struct", {23, "Struct", "kgDM ha-1", "get_OrganBiomass(i)"}},
	{"Sugar", {24, "Sugar", "kgDM ha-1", "get_OrganBiomass(i)"}},
	{"Yield", {25, "Yield", "kgDM ha-1", "get_PrimaryCropYield"}},
	{"SumYield", {26, "SumYield", "kgDM ha-1", "get_AccumulatedPrimaryCropYield"}},
	{"GroPhot", {27, "GroPhot", "kgCH2O ha-1", "GrossPhotosynthesisHaRate"}},
	{"NetPhot", {28, "NetPhot", "kgCH2O ha-1", "NetPhotosynthesis"}},
	{"MaintR", {29, "MaintR", "kgCH2O ha-1", "MaintenanceRespirationAS"}},
	{"GrowthR", {30, "GrowthR", "kgCH2O ha-1", "GrowthRespirationAS"}},
	{"StomRes", {31, "StomRes", "s m-1", "StomataResistance"}},
	{"Height", {32, "Height", "m", "CropHeight"}},
	{"LAI", {33, "LAI", "m2 m-2", "LeafAreaIndex"}},
	{"RootDep", {34, "RootDep", "layer#", "RootingDepth"}},
	{"EffRootDep", {35, "EffRootDep", "m", "Effective RootingDepth"}},

	{"TotBiomN", {36, "TotBiomN", "kgN ha-1", "TotalBiomassNContent"}},
	{"AbBiomN", {37, "AbBiomN", "kgN ha-1", "AbovegroundBiomassNContent"}},
	{"SumNUp", {38, "SumNUp", "kgN ha-1", "SumTotalNUptake"}},
	{"ActNup", {39, "ActNup", "kgN ha-1", "ActNUptake"}},
	{"PotNup", {40, "PotNup", "kgN ha-1", "PotNUptake"}},
	{"NFixed", {41, "NFixed", "kgN ha-1", "NFixed"}},
	{"Target", {42, "Target", "kgN ha-1", "TargetNConcentration"}},
	{"CritN", {43, "CritN", "kgN ha-1", "CriticalNConcentration"}},
	{"AbBiomNc", {44, "AbBiomNc", "kgN ha-1", "AbovegroundBiomassNConcentration"}},
	{"YieldNc", {45, "YieldNc", "kgN ha-1", "PrimaryYieldNConcentration"}},

	{"Protein", {46, "Protein", "kg kg-1", "RawProteinConcentration"}},

	{"NPP", {47, "NPP", "kgC ha-1", "NPP"}},
	{"NPPRoot", {48, "NPPRoot", "kgC ha-1", "NPP root"}},
	{"NPPLeaf", {49, "NPPLeaf", "kgC ha-1", "NPP leaf"}},
	{"NPPShoot", {50, "NPPShoot", "kgC ha-1", "NPP shoot"}},
	{"NPPFruit", {51, "NPPFruit", "kgC ha-1", "NPP fruit"}},
	{"NPPStruct", {52, "NPPStruct", "kgC ha-1", "NPP struct"}},
	{"NPPSugar", {53, "NPPSugar", "kgC ha-1", "NPP sugar"}},

	{"GPP", {54, "GPP", "kgC ha-1", "GPP"}},

	{"Ra", {55, "Ra", "kgC ha-1", "Ra"}},
	{"RaRoot", {56, "RaRoot", "kgC ha-1", "Ra root"}},
	{"RaLeaf", {57, "RaLeaf", "kgC ha-1", "Ra leaf"}},
	{"RaShoot", {58, "RaShoot", "kgC ha-1", "Ra shoot"}},
	{"RaFruit", {59, "RaFruit", "kgC ha-1", "Ra fruit"}},
	{"RaStruct", {60, "RaStruct", "kgC ha-1", "Ra struct"}},
	{"RaSugar", {61, "RaSugar", "kgC ha-1", "Ra sugar"}},

	{"Mois", {62, "Mois", "m3 m-3", "Soil moisture content"}},

	{"Precip", {63, "Precip", "mm", "Precipitation"}},
	{"Irrig", {64, "Irrig", "mm", "Irrigation"}},
	{"Infilt", {65, "Infilt", "mm", "Infiltration"}},
	{"Surface", {66, "Surface", "mm", "Surface water storage"}},
	{"RunOff", {67, "RunOff", "mm", "Surface water runoff"}},
	{"SnowD", {68, "SnowD", "mm", "Snow depth"}},
	{"FrostD", {69, "FrostD", "m", "Frost front depth in soil"}},
	{"ThawD", {70, "ThawD", "m", "Thaw front depth in soil"}},

	{"PASW", {71, "PASW", "m3 m-3", "PASW"}},

	{"SurfTemp", {72, "SurfTemp", "°C", ""}},
	{"STemp", {73, "STemp", "°C", ""}},
{"Act_Ev", {78, "Act_Ev", "mm", ""}},
{"Act_ET", {79, "Act_ET", "mm", ""}},
{"ET0", {80, "ET0", "mm", ""}},
{"Kc", {81, "Kc", "", ""}},
{"AtmCO2", {82, "AtmCO2", "ppm", "Atmospheric CO2 concentration"}},
{"Groundw", {83, "Groundw", "m", ""}},
{"Recharge", {84, "Recharge", "mm", ""}},
{"NLeach", {85, "NLeach", "kgN ha-1", ""}},
{"NO3", {86, "NO3", "kgN m-3", ""}},
{"Carb", {87, "Carb", "kgN m-3", "Soil Carbamid"}},
{"NH4", {88, "NH4", "kgN m-3", ""}},
	{"NO2", {89, "NO2", "kgN m-3", ""}},
	{"SOC", {90, "SOC", "kgC kg-1", "get_SoilOrganicC"}},
	{"SOC-X-Y", {91, "SOC-X-Y", "gC m-2", "SOC-X-Y"}},

	{"AOMf", {93, "AOMf", "kgC m-3", "get_AOM_FastSum"}},
{"AOMs", {94, "AOMs", "kgC m-3", "get_AOM_SlowSum"}},
{"SMBf", {95, "SMBf", "kgC m-3", "get_SMB_Fast"}},
{"SMBs", {96, "SMBs", "kgC m-3", "get_SMB_Slow"}},
{"SOMf", {97, "SOMf", "kgC m-3", "get_SOM_Fast"}},
{"SOMs", {98, "SOMs", "kgC m-3", "get_SOM_Slow"}},
{"CBal", {99, "CBal", "kgC m-3", "get_CBalance"}},

{"Nmin", {100, "Nmin", "kgN ha-1", "NetNMineralisationRate"}},
{"NetNmin", {101, "NetNmin", "kgN ha-1", "NetNmin"}},
{"Denit", {102, "Denit", "kgN ha-1", "Denit"}},
{"N2O", {103, "N2O", "kgN ha-1", "N2O"}},
{"SoilpH", {104, "SoilpH", "", "SoilpH"}},
{"NEP", {105, "NEP", "kgC ha-1", "NEP"}},
{"NEE", {106, "NEE", "kgC ha-", "NEE"}},
{"Rh", {107, "Rh", "kgC ha-", "Rh"}},

{"Tmin", {108, "Tmin", "", ""}},
{"Tavg", {109, "Tavg", "", ""}},
{"Tmax", {110, "Tmax", "", ""}},
{"Wind", {111, "Wind", "", ""}},
{"Globrad", {112, "Globrad", "", ""}},
{"Relhumid", {113, "Relhumid", "", ""}},
{"Sunhours", {114, "Sunhours", "", ""}},


//Result2{, "Root10", "kgDM ha-1", ""}},
//Result2{, "AbGBiom", "kgDM ha-1", ""}},
//Result2{, "Yield", "kgDM ha-1", ""}},
//Result2{, "EarNo", "#", ""}},
//Result2{, "GrainNo", "#", ""}},

//Result2{, "NGrain", "-", ""}},

{"BedGrad", {115, "BedGrad", "0;1", ""}},
//Result2{116, "M0-10", "m3 m-3", ""}},
//Result2{117, "M10-20", "m3 m-3", ""}},
//Result2{118, "M20-30", "m3 m-3", ""}},
//Result2{119, "M30-40", "m3 m-3", ""}},
//Result2{120, "M40-50", "m3 m-3", ""}},
//Result2{121, "M50-60", "m3 m-3", ""}},
//Result2{122, "M60-70", "m3 m-3", ""}},
//Result2{123, "M70-80", "m3 m-3", ""}},
//Result2{124, "M80-90", "m3 m-3", ""}},
//Result2{125, "M0-30", "m3 m-3", ""}},
//Result2{126, "M30-60", "m3 m-3", ""}},
//Result2{127, "M60-90", "m3 m-3", ""}},
//Result2{128, "M0-60", "m3 m-3", ""}},
//Result2{129, "M0-90", "m3 m-3", ""}},
//Result2{130, "PAW0-200", "mm", ""}},
//Result2{131, "PAW0-130", "mm", ""}},
//Result2{132, "PAW0-150", "mm", ""}},
{"N", {133, "N", "kgN m-3", ""}},
//Result2{133, "N0-30", "kgN ha-1", ""}},
//Result2{134, "N30-60", "kgN ha-1", ""}},
//Result2{135, "N60-90", "kgN ha-1", ""}},
//Result2{136, "N90-120", "kgN ha-1", ""}},
//Result2{137, "N0-60", "kgN ha-1", ""}},
//Result2{138, "N0-90", "kgN ha-1", ""}},
//Result2{139, "N0-200", "kgN ha-1", ""}},
//Result2{140, "N0-130", "kgN ha-1", ""}},
//Result2{141, "N0-150", "kgN ha-1", ""}},
//Result2{142, "NH430", "kgN ha-1", ""}},
//Result2{143, "NH460", "kgN ha-1", ""}},
//Result2{144, "NH490", "kgN ha-1", ""}},
{"Co", {145, "Co", "kgC m-3", ""}},
//Result2{145, "Co0-10", "kgC ha-1", ""}},
//Result2{146, "Co0-30", "kgC ha-1", ""}},
//Result2{147, "T0-10", "°C", ""}},
//Result2{148, "T20-30", "°C", ""}},
//Result2{149, "T50-60", "°C", ""}},
//Result2{150, "CO2", "kgC ha-1", "DecomposerRespiration"}},
{"NH3", {151, "NH3", "kgN ha-1", "NH3_Volatilised"}},
//Result2{, "N2O", "-", ""}},
//Result2{, "N2", "-", ""}},
//Result2{, "Ngas", "-", ""}},
{"NFert", {152, "NFert", "kgN ha-1", "dailySumFertiliser"}},
	{"WaterContent", {153, "WaterContent", "%nFC", "soil water content"}},
	{"CapillaryRise", {154, "CapillaryRise", "mm", "capillary rise"}},
	{"PercolationRate",{155, "PercolationRate", "mm", "percolation rate"}},
	{"SMB-CO2-ER", {156, "SMB-CO2-ER", "", "soilOrganic.get_SMB_CO2EvolutionRate"}},
	{"Evapotranspiration", {157, "Evapotranspiration", "mm", "monica.getEvapotranspiration()"}},
	{"Evaporation", {158, "Evaporation", "mm", "monica.getEvaporation()"}},
	{"Transpiration", {159, "Transpiration", "mm", "monica.getTranspiration()"}}
};


	return res2;
}
*/


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
{
  merge(j);
}

Errors YieldComponent::merge(json11::Json j)
{
  set_int_value(organId, j, "organId");
  set_double_value(yieldPercentage, j, "yieldPercentage");
  set_double_value(yieldDryMatter, j, "yieldDryMatter");

	return{};
}

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
{
  merge(j);
}

Errors SpeciesParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_string_value(pc_SpeciesId, j, "SpeciesName");
  set_int_value(pc_CarboxylationPathway, j, "CarboxylationPathway");
  set_double_value(pc_DefaultRadiationUseEfficiency, j, "DefaultRadiationUseEfficiency");
  set_double_value(pc_PartBiologicalNFixation, j, "PartBiologicalNFixation");
  set_double_value(pc_InitialKcFactor, j, "InitialKcFactor");
  set_double_value(pc_LuxuryNCoeff, j, "LuxuryNCoeff");
  set_double_value(pc_MaxCropDiameter, j, "MaxCropDiameter");
  set_double_value(pc_StageAtMaxHeight, j, "StageAtMaxHeight");
  set_double_value(pc_StageAtMaxDiameter, j, "StageAtMaxDiameter");
  set_double_value(pc_MinimumNConcentration, j, "MinimumNConcentration");
  set_double_value(pc_MinimumTemperatureForAssimilation, j, "MinimumTemperatureForAssimilation");
  set_double_value(pc_OptimumTemperatureForAssimilation, j, "OptimumTemperatureForAssimilation");
  set_double_value(pc_MaximumTemperatureForAssimilation, j, "MaximumTemperatureForAssimilation");
  set_double_value(pc_NConcentrationAbovegroundBiomass, j, "NConcentrationAbovegroundBiomass");
  set_double_value(pc_NConcentrationB0, j, "NConcentrationB0");
  set_double_value(pc_NConcentrationPN, j, "NConcentrationPN");
  set_double_value(pc_NConcentrationRoot, j, "NConcentrationRoot");
  set_int_value(pc_DevelopmentAccelerationByNitrogenStress, j, "DevelopmentAccelerationByNitrogenStress");
  set_double_value(pc_FieldConditionModifier, j, "FieldConditionModifier");
  set_double_value(pc_AssimilateReallocation, j, "AssimilateReallocation");
  set_double_vector(pc_BaseTemperature, j, "BaseTemperature");
  set_double_vector(pc_OrganMaintenanceRespiration, j, "OrganMaintenanceRespiration");
  set_double_vector(pc_OrganGrowthRespiration, j, "OrganGrowthRespiration");
  set_double_vector(pc_StageMaxRootNConcentration, j, "StageMaxRootNConcentration");
  set_double_vector(pc_InitialOrganBiomass, j, "InitialOrganBiomass");
  set_double_vector(pc_CriticalOxygenContent, j, "CriticalOxygenContent");
  set_bool_vector(pc_AbovegroundOrgan, j, "AbovegroundOrgan");
  set_bool_vector(pc_StorageOrgan, j, "StorageOrgan");
  set_double_value(pc_SamplingDepth, j, "SamplingDepth");
  set_double_value(pc_TargetNSamplingDepth, j, "TargetNSamplingDepth");
  set_double_value(pc_TargetN30, j, "TargetN30");
  set_double_value(pc_MaxNUptakeParam, j, "MaxNUptakeParam");
  set_double_value(pc_RootDistributionParam, j, "RootDistributionParam");
  set_int_value(pc_PlantDensity, j, "PlantDensity");
  set_double_value(pc_RootGrowthLag, j, "RootGrowthLag");
  set_double_value(pc_MinimumTemperatureRootGrowth, j, "MinimumTemperatureRootGrowth");
  set_double_value(pc_InitialRootingDepth, j, "InitialRootingDepth");
  set_double_value(pc_RootPenetrationRate, j, "RootPenetrationRate");
  set_double_value(pc_RootFormFactor, j, "RootFormFactor");
  set_double_value(pc_SpecificRootLength, j, "SpecificRootLength");
  set_int_value(pc_StageAfterCut, j, "StageAfterCut");
  set_double_value(pc_LimitingTemperatureHeatStress, j, "LimitingTemperatureHeatStress");
  set_int_value(pc_CuttingDelayDays, j, "CuttingDelayDays");
  set_double_value(pc_DroughtImpactOnFertilityFactor, j, "DroughtImpactOnFertilityFactor");

	set_double_value(pc_efMono, j, "ef-mono");
	set_double_value(pc_efMonos, j, "ef-monos");
	set_double_value(pc_efIso, j, "ef-iso");
	set_double_value(pc_VCMAX25, j, "VCMAX25");

	return res;
}

json11::Json SpeciesParameters::to_json() const
{
  auto species = J11Object 
	{{"type", "SpeciesParameters"}
	,{"SpeciesName", pc_SpeciesId}
  ,{"CarboxylationPathway", pc_CarboxylationPathway}
	,{"DefaultRadiationUseEfficiency", pc_DefaultRadiationUseEfficiency}
	,{"PartBiologicalNFixation", pc_PartBiologicalNFixation}
  ,{"InitialKcFactor", pc_InitialKcFactor}
  ,{"LuxuryNCoeff", pc_LuxuryNCoeff}
  ,{"MaxCropDiameter", pc_MaxCropDiameter}
  ,{"StageAtMaxHeight", pc_StageAtMaxHeight}
  ,{"StageAtMaxDiameter", pc_StageAtMaxDiameter}
  ,{"MinimumNConcentration", pc_MinimumNConcentration}
  ,{"MinimumTemperatureForAssimilation", pc_MinimumTemperatureForAssimilation}
  ,{"OptimumTemperatureForAssimilation", pc_OptimumTemperatureForAssimilation}
  ,{"MaximumTemperatureForAssimilation", pc_MaximumTemperatureForAssimilation}
  ,{"NConcentrationAbovegroundBiomass", pc_NConcentrationAbovegroundBiomass}
  ,{"NConcentrationB0", pc_NConcentrationB0}
  ,{"NConcentrationPN", pc_NConcentrationPN}
  ,{"NConcentrationRoot", pc_NConcentrationRoot}
  ,{"DevelopmentAccelerationByNitrogenStress", pc_DevelopmentAccelerationByNitrogenStress}
  ,{"FieldConditionModifier", pc_FieldConditionModifier}
  ,{"AssimilateReallocation", pc_AssimilateReallocation}
  ,{"BaseTemperature", toPrimJsonArray(pc_BaseTemperature)}
  ,{"OrganMaintenanceRespiration", toPrimJsonArray(pc_OrganMaintenanceRespiration)}
  ,{"OrganGrowthRespiration", toPrimJsonArray(pc_OrganGrowthRespiration)}
  ,{"StageMaxRootNConcentration", toPrimJsonArray(pc_StageMaxRootNConcentration)}
  ,{"InitialOrganBiomass", toPrimJsonArray(pc_InitialOrganBiomass)}
  ,{"CriticalOxygenContent", toPrimJsonArray(pc_CriticalOxygenContent)}
  ,{"AbovegroundOrgan", toPrimJsonArray(pc_AbovegroundOrgan)}
  ,{"StorageOrgan", toPrimJsonArray(pc_StorageOrgan)}
  ,{"SamplingDepth", pc_SamplingDepth}
  ,{"TargetNSamplingDepth", pc_TargetNSamplingDepth}
  ,{"TargetN30", pc_TargetN30}
  ,{"MaxNUptakeParam", pc_MaxNUptakeParam}
  ,{"RootDistributionParam", pc_RootDistributionParam}
	,{"PlantDensity", J11Array{pc_PlantDensity, "plants m-2"}}
  ,{"RootGrowthLag", pc_RootGrowthLag}
  ,{"MinimumTemperatureRootGrowth", pc_MinimumTemperatureRootGrowth}
  ,{"InitialRootingDepth", pc_InitialRootingDepth}
  ,{"RootPenetrationRate", pc_RootPenetrationRate}
  ,{"RootFormFactor", pc_RootFormFactor}
  ,{"SpecificRootLength", pc_SpecificRootLength}
  ,{"StageAfterCut", pc_StageAfterCut}
  ,{"LimitingTemperatureHeatStress", pc_LimitingTemperatureHeatStress}
  ,{"CuttingDelayDays", pc_CuttingDelayDays}
  ,{"DroughtImpactOnFertilityFactor", pc_DroughtImpactOnFertilityFactor}
	,{ "ef-mono", J11Array{pc_efMono, "ug gDW-1 h-1"} }
	,{ "ef-monos", J11Array{pc_efMonos, "ug gDW-1 h-1"} }
	,{ "ef-iso", J11Array{pc_efIso, "ug gDW-1 h-1"} }
	,{ "VCMAX25", pc_VCMAX25}
	};

  return species;
}

//------------------------------------------------------------------------------

CultivarParameters::CultivarParameters(json11::Json j)
{
  merge(j);
}

Errors CultivarParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);
	
	string err;
  if(j.has_shape({{"OrganIdsForPrimaryYield", json11::Json::ARRAY}}, err))
    pc_OrganIdsForPrimaryYield = toVector<YieldComponent>(j["OrganIdsForPrimaryYield"]);
	else
		res.errors.push_back(string("Couldn't read 'OrganIdsForPrimaryYield' key from JSON object:\n") + j.dump());
  
	if(j.has_shape({{"OrganIdsForSecondaryYield", json11::Json::ARRAY}}, err))
    pc_OrganIdsForSecondaryYield = toVector<YieldComponent>(j["OrganIdsForSecondaryYield"]);
	else
		res.errors.push_back(string("Couldn't read 'OrganIdsForSecondaryYield' key from JSON object:\n") + j.dump());

  if(j.has_shape({{"OrganIdsForCutting", json11::Json::ARRAY}}, err))
    pc_OrganIdsForCutting = toVector<YieldComponent>(j["OrganIdsForCutting"]);
	else
		res.warnings.push_back(string("Couldn't read 'OrganIdsForCutting' key from JSON object:\n") + j.dump());

  set_string_value(pc_CultivarId, j, "CultivarName");
  set_string_value(pc_Description, j, "Description");
  set_bool_value(pc_Perennial, j, "Perennial");
  set_double_value(pc_MaxAssimilationRate, j, "MaxAssimilationRate");
  set_double_value(pc_MaxCropHeight, j, "MaxCropHeight");
  set_double_value(pc_ResidueNRatio, j, "ResidueNRatio");
  set_double_value(pc_LT50cultivar, j, "LT50cultivar");
  set_double_value(pc_CropHeightP1, j, "CropHeightP1");
  set_double_value(pc_CropHeightP2, j, "CropHeightP2");
  set_double_value(pc_CropSpecificMaxRootingDepth, j, "CropSpecificMaxRootingDepth");
  set_double_vector(pc_BaseDaylength, j, "BaseDaylength");
  set_double_vector(pc_OptimumTemperature, j, "OptimumTemperature");
  set_double_vector(pc_DaylengthRequirement, j, "DaylengthRequirement");
  set_double_vector(pc_DroughtStressThreshold, j, "DroughtStressThreshold");
  set_double_vector(pc_SpecificLeafArea, j, "SpecificLeafArea");
  set_double_vector(pc_StageKcFactor, j, "StageKcFactor");
  set_double_vector(pc_StageTemperatureSum, j, "StageTemperatureSum");
  set_double_vector(pc_VernalisationRequirement, j, "VernalisationRequirement");
  set_double_value(pc_HeatSumIrrigationStart, j, "HeatSumIrrigationStart");
  set_double_value(pc_HeatSumIrrigationEnd, j, "HeatSumIrrigationEnd");
  set_double_value(pc_CriticalTemperatureHeatStress, j, "CriticalTemperatureHeatStress");
  set_double_value(pc_BeginSensitivePhaseHeatStress, j, "BeginSensitivePhaseHeatStress");
  set_double_value(pc_EndSensitivePhaseHeatStress, j, "EndSensitivePhaseHeatStress");
  set_double_value(pc_FrostHardening, j, "FrostHardening");
  set_double_value(pc_FrostDehardening, j, "FrostDehardening");
  set_double_value(pc_LowTemperatureExposure, j, "LowTemperatureExposure");
  set_double_value(pc_RespiratoryStress, j, "RespiratoryStress");
  set_int_value(pc_LatestHarvestDoy, j, "LatestHarvestDoy");

  for(auto js : j["AssimilatePartitioningCoeff"].array_items())
    pc_AssimilatePartitioningCoeff.push_back(double_vector(js));
  for(auto js : j["OrganSenescenceRate"].array_items())
    pc_OrganSenescenceRate.push_back(double_vector(js));

	return res;
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
{
  merge(j["species"], j["cultivar"]);
}

CropParameters::CropParameters(json11::Json sj, json11::Json cj)
{
  merge(sj, cj);
}

Errors CropParameters::merge(json11::Json j)
{
  return merge(j["species"], j["cultivar"]);
}

Errors CropParameters::merge(json11::Json sj, json11::Json cj)
{
	Errors res;
	res.append(speciesParams.merge(sj));
  res.append(cultivarParams.merge(cj));
	return res;
}

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
{
  merge(j);
}

Errors MineralFertiliserParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_string_value(id, j, "id");
  set_string_value(name, j, "name");
  set_double_value(vo_Carbamid, j, "Carbamid");
  set_double_value(vo_NH4, j, "NH4");
  set_double_value(vo_NO3, j, "NO3");

	return res;
}

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
  : min(min)
  , max(max)
  , delayInDays(delayInDays) { }

NMinUserParameters::NMinUserParameters(json11::Json j)
{
  merge(j);
}

Errors NMinUserParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(min, j, "min");
  set_double_value(max, j, "max");
  set_int_value(delayInDays, j, "delayInDays");

	return res;
}

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
{
  merge(j);
}

Errors IrrigationParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);
	
  set_double_value(nitrateConcentration, j, "nitrateConcentration");
  set_double_value(sulfateConcentration, j, "sulfateConcentration");

	return res;
}

json11::Json IrrigationParameters::to_json() const
{
  return json11::Json::object {
    {"type", "IrrigationParameters"},
		{"nitrateConcentration", J11Array {nitrateConcentration, "mg dm-3"}},
		{"sulfateConcentration", J11Array {sulfateConcentration, "mg dm-3"}}};
}

//------------------------------------------------------------------------------

AutomaticIrrigationParameters::AutomaticIrrigationParameters(double a, 
																														 double t, 
																														 double nc, 
																														 double sc)
  : IrrigationParameters(nc, sc)
  , amount(a)
  , threshold(t)
{}

AutomaticIrrigationParameters::AutomaticIrrigationParameters(json11::Json j)
{
  merge(j);
}

Errors AutomaticIrrigationParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  res.append(IrrigationParameters::merge(j["irrigationParameters"]));
  set_double_value(amount, j, "amount");
  set_double_value(threshold, j, "threshold");

	return res;
}

json11::Json AutomaticIrrigationParameters::to_json() const
{
  return json11::Json::object {
    {"type", "AutomaticIrrigationParameters"},
    {"irrigationParameters", IrrigationParameters::to_json()},
    {"amount", J11Array {amount, "mm"}},
    {"threshold", threshold}};
}

//------------------------------------------------------------------------------

MeasuredGroundwaterTableInformation::MeasuredGroundwaterTableInformation(json11::Json j)
{
  merge(j);
}

Errors MeasuredGroundwaterTableInformation::merge(json11::Json j)
{
	Errors res;

  set_bool_value(groundwaterInformationAvailable, j, "groundwaterInformationAvailable");

  string err;
	if(j.has_shape({{"groundwaterInfo", json11::Json::OBJECT}}, err))
		for(auto p : j["groundwaterInfo"].object_items())
			groundwaterInfo[Tools::Date::fromIsoDateString(p.first)] = p.second.number_value();
	else
		res.errors.push_back(string("Couldn't read 'groundwaterInfo' key from JSON object:\n") + j.dump());

	return res;
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
{
  merge(j);
}

Errors SiteParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

	string err = "";
  set_double_value(vs_Latitude, j, "Latitude");
  set_double_value(vs_Slope, j, "Slope");
  set_double_value(vs_HeightNN, j, "HeightNN");
  set_double_value(vs_GroundwaterDepth, j, "GroundwaterDepth");
  set_double_value(vs_Soil_CN_Ratio, j, "Soil_CN_Ratio");
  set_double_value(vs_DrainageCoeff, j, "DrainageCoeff");
  set_double_value(vq_NDeposition, j, "NDeposition");
  set_double_value(vs_MaxEffectiveRootingDepth, j, "MaxEffectiveRootingDepth");

	if(j.has_shape({{"SoilProfileParameters", json11::Json::ARRAY}}, err))
	{
		const auto& sps = j["SoilProfileParameters"].array_items();
		vs_SoilParameters = make_shared<SoilPMs>();
		size_t layerCount = 0;
		for(size_t spi = 0, spsCount = sps.size(); spi < spsCount; spi++)
		{
			Json sp = sps.at(spi);

			//repeat layers if there is an associated Thickness parameter
			string err;
			int repeatLayer = 1;
			if(sp.has_shape({{"Thickness", Json::NUMBER}}, err)
				 || sp.has_shape({{"Thickness", Json::ARRAY}}, err))
				repeatLayer = min(20 - int(layerCount), max(1, Tools::roundRT<int>(double_valueD(sp, "Thickness", 0.1)*10.0, 0)));

			//simply repeat the last layer as often as necessary to fill the 20 layers
			if(spi + 1 == spsCount)
				repeatLayer = 20 - layerCount;

			for(int i = 1; i <= repeatLayer; i++)
			{
				SoilParameters sps;
				auto es = sps.merge(sp);
				vs_SoilParameters->push_back(sps);
				if(es.failure())
					res.append(es);
			}

			layerCount += repeatLayer;
		}
	}
	else
		res.errors.push_back(string("Couldn't read 'SoilProfileParameters' JSON array from JSON object:\n") + j.dump());

	return res;
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
    sps["SoilProfileParameters"] = toJsonArray(*vs_SoilParameters);

  return sps;
}

//------------------------------------------------------------------------------

AutomaticHarvestParameters::AutomaticHarvestParameters(HarvestTime yt)
  : _harvestTime(yt)
{}

AutomaticHarvestParameters::AutomaticHarvestParameters(json11::Json j)
{
  merge(j);
}

Errors AutomaticHarvestParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  int ht = -1;
  set_int_value(ht, j, "harvestTime");
  if(ht > -1)
    _harvestTime = HarvestTime(ht);
  set_int_value(_latestHarvestDOY, j, "latestHarvestDOY");

	return res;
}

json11::Json AutomaticHarvestParameters::to_json() const
{
  return J11Object {
    {"harvestTime", int(_harvestTime)},
    {"latestHavestDOY", _latestHarvestDOY}};
}

//------------------------------------------------------------------------------

NMinCropParameters::NMinCropParameters(double samplingDepth, double nTarget, double nTarget30)
  : samplingDepth(samplingDepth),
    nTarget(nTarget),
    nTarget30(nTarget30) {}

NMinCropParameters::NMinCropParameters(json11::Json j)
{
  merge(j);
}

Errors NMinCropParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(samplingDepth, j, "samplingDepth");
  set_double_value(nTarget, j, "nTarget");
  set_double_value(nTarget30, j, "nTarget30");

	return res;
}

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
{
  merge(j);
}

Errors OrganicMatterParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(vo_AOM_DryMatterContent, j, "AOM_DryMatterContent");
  set_double_value(vo_AOM_NH4Content, j, "AOM_NH4Content");
  set_double_value(vo_AOM_NO3Content, j, "AOM_NO3Content");
  set_double_value(vo_AOM_CarbamidContent, j, "AOM_CarbamidContent");
  set_double_value(vo_AOM_SlowDecCoeffStandard, j, "AOM_SlowDecCoeffStandard");
  set_double_value(vo_AOM_FastDecCoeffStandard, j, "AOM_FastDecCoeffStandard");
  set_double_value(vo_PartAOM_to_AOM_Slow, j, "PartAOM_to_AOM_Slow");
  set_double_value(vo_PartAOM_to_AOM_Fast, j, "PartAOM_to_AOM_Fast");
  set_double_value(vo_CN_Ratio_AOM_Slow, j, "CN_Ratio_AOM_Slow");
  set_double_value(vo_CN_Ratio_AOM_Fast, j, "CN_Ratio_AOM_Fast");
  set_double_value(vo_PartAOM_Slow_to_SMB_Slow, j, "PartAOM_Slow_to_SMB_Slow");
  set_double_value(vo_PartAOM_Slow_to_SMB_Fast, j, "PartAOM_Slow_to_SMB_Fast");
  set_double_value(vo_NConcentration, j, "NConcentration");

	return res;
}

json11::Json OrganicMatterParameters::to_json() const
{
  return J11Object {
    {"type", "OrganicMatterParameters"},
    {"AOM_DryMatterContent", J11Array {vo_AOM_DryMatterContent, "kg DM kg FM-1", "Dry matter content of added organic matter"}},
    {"AOM_NH4Content", J11Array {vo_AOM_NH4Content, "kg N kg DM-1", "Ammonium content in added organic matter"}},
    {"AOM_NO3Content", J11Array {vo_AOM_NO3Content, "kg N kg DM-1", "Nitrate content in added organic matter"}},
    {"AOM_NO3Content", J11Array {vo_AOM_NO3Content, "kg N kg DM-1", "Carbamide content in added organic matter"}},
    {"AOM_SlowDecCoeffStandard", J11Array {vo_AOM_SlowDecCoeffStandard, "d-1", "Decomposition rate coefficient of slow AOM at standard conditions"}},
    {"AOM_FastDecCoeffStandard", J11Array {vo_AOM_FastDecCoeffStandard, "d-1", "Decomposition rate coefficient of fast AOM at standard conditions"}},
    {"PartAOM_to_AOM_Slow", J11Array {vo_PartAOM_to_AOM_Slow, "kg kg-1", "Part of AOM that is assigned to the slowly decomposing pool"}},
    {"PartAOM_to_AOM_Fast", J11Array {vo_PartAOM_to_AOM_Fast, "kg kg-1", "Part of AOM that is assigned to the rapidly decomposing pool"}},
    {"CN_Ratio_AOM_Slow", J11Array {vo_CN_Ratio_AOM_Slow, "", "C to N ratio of the slowly decomposing AOM pool"}},
    {"CN_Ratio_AOM_Fast", J11Array {vo_CN_Ratio_AOM_Fast, "", "C to N ratio of the rapidly decomposing AOM pool"}},
    {"PartAOM_Slow_to_SMB_Slow", J11Array {vo_PartAOM_Slow_to_SMB_Slow, "kg kg-1", "Part of AOM slow consumed by slow soil microbial biomass"}},
    {"PartAOM_Slow_to_SMB_Fast", J11Array {vo_PartAOM_Slow_to_SMB_Fast, "kg kg-1", "Part of AOM slow consumed by fast soil microbial biomass"}},
    {"NConcentration", vo_NConcentration}};
}

//-----------------------------------------------------------------------------------------

OrganicFertiliserParameters::OrganicFertiliserParameters(json11::Json j)
{
  merge(j);
}

Errors OrganicFertiliserParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  res.append(OrganicMatterParameters::merge(j));

  set_string_value(id, j, "id");
  set_string_value(name, j, "name");

	return res;
}

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
{
  merge(j);
}

Errors CropResidueParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  res.append(OrganicMatterParameters::merge(j));
  set_string_value(species, j, "species");
  set_string_value(residueType, j, "residueType");

	return res;
}

json11::Json CropResidueParameters::to_json() const
{
  auto omp = OrganicMatterParameters::to_json().object_items();
  omp["type"] = "CropResidueParameters";
  omp["species"] = species;
  omp["residueType"] = residueType;
  return omp;
}

//-----------------------------------------------------------------------------------------

SimulationParameters::SimulationParameters(json11::Json j)
{
  merge(j);
}

Errors SimulationParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_iso_date_value(startDate, j, "startDate");
  set_iso_date_value(endDate, j, "endDate");

  set_bool_value(pc_NitrogenResponseOn, j, "NitrogenResponseOn");
  set_bool_value(pc_WaterDeficitResponseOn, j, "WaterDeficitResponseOn");
  set_bool_value(pc_EmergenceFloodingControlOn, j, "EmergenceFloodingControlOn");
  set_bool_value(pc_EmergenceMoistureControlOn, j, "EmergenceMoistureControlOn");

  set_bool_value(p_UseAutomaticIrrigation, j, "UseAutomaticIrrigation");
  p_AutoIrrigationParams.merge(j["AutoIrrigationParams"]);
	
  set_bool_value(p_UseNMinMineralFertilisingMethod, j, "UseNMinMineralFertilisingMethod");
  p_NMinFertiliserPartition.merge(j["NMinFertiliserPartition"]);
  p_NMinUserParams.merge(j["NMinUserParams"]);
	set_int_value(p_JulianDayAutomaticFertilising, j, "JulianDayAutomaticFertilising");

  set_bool_value(p_UseSecondaryYields, j, "UseSecondaryYields");
  set_bool_value(p_UseAutomaticHarvestTrigger, j, "UseAutomaticHarvestTrigger");
  set_int_value(p_NumberOfLayers, j, "NumberOfLayers");
  set_double_value(p_LayerThickness, j, "LayerThickness");

  set_int_value(p_StartPVIndex, j, "StartPVIndex");
  
	return res;
}

json11::Json SimulationParameters::to_json() const
{
  return json11::Json::object 
	{{"type", "SimulationParameters"}
	,{"startDate", startDate.toIsoDateString()}
	,{"endDate", endDate.toIsoDateString()}
	,{"NitrogenResponseOn", pc_NitrogenResponseOn}
	,{"WaterDeficitResponseOn", pc_WaterDeficitResponseOn}
	,{"EmergenceFloodingControlOn", pc_EmergenceFloodingControlOn}
	,{"EmergenceMoistureControlOn", pc_EmergenceMoistureControlOn }
  ,{"UseAutomaticIrrigation", p_UseAutomaticIrrigation}
  ,{"AutoIrrigationParams", p_AutoIrrigationParams}
  ,{"UseNMinMineralFertilisingMethod", p_UseNMinMineralFertilisingMethod}
  ,{"NMinFertiliserPartition", p_NMinFertiliserPartition}
  ,{"NMinUserParams", p_NMinUserParams}
	,{"JulianDayAutomaticFertilising", p_JulianDayAutomaticFertilising}
  ,{"UseSecondaryYields", p_UseSecondaryYields}
  ,{"UseAutomaticHarvestTrigger", p_UseAutomaticHarvestTrigger}
  ,{"NumberOfLayers", p_NumberOfLayers}
  ,{"LayerThickness", p_LayerThickness}
  ,{"StartPVIndex", p_StartPVIndex}
	};
}

//-----------------------------------------------------------------------------------------

UserCropParameters::UserCropParameters(json11::Json j)
{
  merge(j);
}

Errors UserCropParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(pc_CanopyReflectionCoefficient, j, "CanopyReflectionCoefficient");
  set_double_value(pc_ReferenceMaxAssimilationRate, j, "ReferenceMaxAssimilationRate");
  set_double_value(pc_ReferenceLeafAreaIndex, j, "ReferenceLeafAreaIndex");
  set_double_value(pc_MaintenanceRespirationParameter1, j, "MaintenanceRespirationParameter1");
  set_double_value(pc_MaintenanceRespirationParameter2, j, "MaintenanceRespirationParameter2");
  set_double_value(pc_MinimumNConcentrationRoot, j, "MinimumNConcentrationRoot");
  set_double_value(pc_MinimumAvailableN, j, "MinimumAvailableN");
  set_double_value(pc_ReferenceAlbedo, j, "ReferenceAlbedo");
  set_double_value(pc_StomataConductanceAlpha, j, "StomataConductanceAlpha");
  set_double_value(pc_SaturationBeta, j, "SaturationBeta");
  set_double_value(pc_GrowthRespirationRedux, j, "GrowthRespirationRedux");
  set_double_value(pc_MaxCropNDemand, j, "MaxCropNDemand");
  set_double_value(pc_GrowthRespirationParameter1, j, "GrowthRespirationParameter1");
  set_double_value(pc_GrowthRespirationParameter2, j, "GrowthRespirationParameter2");
  set_double_value(pc_Tortuosity, j, "Tortuosity");
  set_bool_value(pc_AdjustRootDepthForSoilProps, j, "AdjustRootDepthForSoilProps");

	return res;
}

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
	{"AdjustRootDepthForSoilProps", pc_AdjustRootDepthForSoilProps }
  };
}

//-----------------------------------------------------------------------------------------

UserEnvironmentParameters::UserEnvironmentParameters(json11::Json j)
{
  merge(j);
}

Errors UserEnvironmentParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(p_Albedo, j, "Albedo");
  set_double_value(p_AtmosphericCO2, j, "AtmosphericCO2");
  set_double_value(p_WindSpeedHeight, j, "WindSpeedHeight");
  set_double_value(p_LeachingDepth, j, "LeachingDepth");
  set_double_value(p_timeStep, j, "timeStep");
  set_double_value(p_MaxGroundwaterDepth, j, "MaxGroundwaterDepth");
  set_double_value(p_MinGroundwaterDepth, j, "MinGroundwaterDepth");
  set_int_value(p_MinGroundwaterDepthMonth, j, "MinGroundwaterDepthMonth");

	return res;
}

json11::Json UserEnvironmentParameters::to_json() const
{
  return json11::Json::object {
    {"type", "UserEnvironmentParameters"},
    {"Albedo", p_Albedo},
    {"AtmosphericCO2", p_AtmosphericCO2},
    {"WindSpeedHeight", p_WindSpeedHeight},
    {"LeachingDepth", p_LeachingDepth},
    {"timeStep", p_timeStep},
    {"MaxGroundwaterDepth", p_MaxGroundwaterDepth},
    {"MinGroundwaterDepth", p_MinGroundwaterDepth},
    {"MinGroundwaterDepthMonth", p_MinGroundwaterDepthMonth}
  };
}

//-----------------------------------------------------------------------------------------

UserSoilMoistureParameters::UserSoilMoistureParameters()
{
	getCapillaryRiseRate = [](string soilTexture, int distance) { return 0; };
}


UserSoilMoistureParameters::UserSoilMoistureParameters(json11::Json j)
	: UserSoilMoistureParameters()
{
  merge(j);
}

Errors UserSoilMoistureParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(pm_CriticalMoistureDepth, j, "CriticalMoistureDepth");
  set_double_value(pm_SaturatedHydraulicConductivity, j, "SaturatedHydraulicConductivity");
  set_double_value(pm_SurfaceRoughness, j, "SurfaceRoughness");
  set_double_value(pm_GroundwaterDischarge, j, "GroundwaterDischarge");
  set_double_value(pm_HydraulicConductivityRedux, j, "HydraulicConductivityRedux");
  set_double_value(pm_SnowAccumulationTresholdTemperature, j, "SnowAccumulationTresholdTemperature");
  set_double_value(pm_KcFactor, j, "KcFactor");
  set_double_value(pm_TemperatureLimitForLiquidWater, j, "TemperatureLimitForLiquidWater");
  set_double_value(pm_CorrectionSnow, j, "CorrectionSnow");
  set_double_value(pm_CorrectionRain, j, "CorrectionRain");
  set_double_value(pm_SnowMaxAdditionalDensity, j, "SnowMaxAdditionalDensity");
  set_double_value(pm_NewSnowDensityMin, j, "NewSnowDensityMin");
  set_double_value(pm_SnowRetentionCapacityMin, j, "SnowRetentionCapacityMin");
  set_double_value(pm_RefreezeParameter1, j, "RefreezeParameter1");
  set_double_value(pm_RefreezeParameter2, j, "RefreezeParameter2");
  set_double_value(pm_RefreezeTemperature, j, "RefreezeTemperature");
  set_double_value(pm_SnowMeltTemperature, j, "SnowMeltTemperature");
  set_double_value(pm_SnowPacking, j, "SnowPacking");
  set_double_value(pm_SnowRetentionCapacityMax, j, "SnowRetentionCapacityMax");
  set_double_value(pm_EvaporationZeta, j, "EvaporationZeta");
  set_double_value(pm_XSACriticalSoilMoisture, j, "XSACriticalSoilMoisture");
  set_double_value(pm_MaximumEvaporationImpactDepth, j, "MaximumEvaporationImpactDepth");
  set_double_value(pm_MaxPercolationRate, j, "MaxPercolationRate");
  set_double_value(pm_MoistureInitValue, j, "MoistureInitValue");

	return res;
}

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
{
  merge(j);
}

Errors UserSoilTemperatureParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(pt_NTau, j, "NTau");
  set_double_value(pt_InitialSurfaceTemperature, j, "InitialSurfaceTemperature");
  set_double_value(pt_BaseTemperature, j, "BaseTemperature");
  set_double_value(pt_QuartzRawDensity, j, "QuartzRawDensity");
  set_double_value(pt_DensityAir, j, "DensityAir");
  set_double_value(pt_DensityWater, j, "DensityWater");
  set_double_value(pt_DensityHumus, j, "DensityHumus");
  set_double_value(pt_SpecificHeatCapacityAir, j, "SpecificHeatCapacityAir");
  set_double_value(pt_SpecificHeatCapacityQuartz, j, "SpecificHeatCapacityQuartz");
  set_double_value(pt_SpecificHeatCapacityWater, j, "SpecificHeatCapacityWater");
  set_double_value(pt_SpecificHeatCapacityHumus, j, "SpecificHeatCapacityHumus");
  set_double_value(pt_SoilAlbedo, j, "SoilAlbedo");
  set_double_value(pt_SoilMoisture, j, "SoilMoisture");

	return res;
}

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
{
  merge(j);
}

Errors UserSoilTransportParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(pq_DispersionLength, j, "DispersionLength");
  set_double_value(pq_AD, j, "AD");
  set_double_value(pq_DiffusionCoefficientStandard, j, "DiffusionCoefficientStandard");
  set_double_value(pq_NDeposition, j, "NDeposition");

	return res;
}

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
{
  merge(j);
}

Errors UserSoilOrganicParameters::merge(json11::Json j)
{
	Errors res = Json11Serializable::merge(j);

  set_double_value(po_SOM_SlowDecCoeffStandard, j, "SOM_SlowDecCoeffStandard");
  set_double_value(po_SOM_FastDecCoeffStandard, j, "SOM_FastDecCoeffStandard");
  set_double_value(po_SMB_SlowMaintRateStandard, j, "SMB_SlowMaintRateStandard");
  set_double_value(po_SMB_FastMaintRateStandard, j, "SMB_FastMaintRateStandard");
  set_double_value(po_SMB_SlowDeathRateStandard, j, "SMB_SlowDeathRateStandard");
  set_double_value(po_SMB_FastDeathRateStandard, j, "SMB_FastDeathRateStandard");
  set_double_value(po_SMB_UtilizationEfficiency, j, "SMB_UtilizationEfficiency");
  set_double_value(po_SOM_SlowUtilizationEfficiency, j, "SOM_SlowUtilizationEfficiency");
  set_double_value(po_SOM_FastUtilizationEfficiency, j, "SOM_FastUtilizationEfficiency");
  set_double_value(po_AOM_SlowUtilizationEfficiency, j, "AOM_SlowUtilizationEfficiency");
  set_double_value(po_AOM_FastUtilizationEfficiency, j, "AOM_FastUtilizationEfficiency");
  set_double_value(po_AOM_FastMaxC_to_N, j, "AOM_FastMaxC_to_N");
  set_double_value(po_PartSOM_Fast_to_SOM_Slow, j, "PartSOM_Fast_to_SOM_Slow");
  set_double_value(po_PartSMB_Slow_to_SOM_Fast, j, "PartSMB_Slow_to_SOM_Fast");
  set_double_value(po_PartSMB_Fast_to_SOM_Fast, j, "PartSMB_Fast_to_SOM_Fast");
  set_double_value(po_PartSOM_to_SMB_Slow, j, "PartSOM_to_SMB_Slow");
  set_double_value(po_PartSOM_to_SMB_Fast, j, "PartSOM_to_SMB_Fast");
  set_double_value(po_CN_Ratio_SMB, j, "CN_Ratio_SMB");
  set_double_value(po_LimitClayEffect, j, "LimitClayEffect");
  set_double_value(po_AmmoniaOxidationRateCoeffStandard, j, "AmmoniaOxidationRateCoeffStandard");
  set_double_value(po_NitriteOxidationRateCoeffStandard, j, "NitriteOxidationRateCoeffStandard");
  set_double_value(po_TransportRateCoeff, j, "TransportRateCoeff");
  set_double_value(po_SpecAnaerobDenitrification, j, "SpecAnaerobDenitrification");
  set_double_value(po_ImmobilisationRateCoeffNO3, j, "ImmobilisationRateCoeffNO3");
  set_double_value(po_ImmobilisationRateCoeffNH4, j, "ImmobilisationRateCoeffNH4");
  set_double_value(po_Denit1, j, "Denit1");
  set_double_value(po_Denit2, j, "Denit2");
  set_double_value(po_Denit3, j, "Denit3");
  set_double_value(po_HydrolysisKM, j, "HydrolysisKM");
  set_double_value(po_ActivationEnergy, j, "ActivationEnergy");
  set_double_value(po_HydrolysisP1, j, "HydrolysisP1");
  set_double_value(po_HydrolysisP2, j, "HydrolysisP2");
  set_double_value(po_AtmosphericResistance, j, "AtmosphericResistance");
  set_double_value(po_N2OProductionRate, j, "N2OProductionRate");
  set_double_value(po_Inhibitor_NH3, j, "Inhibitor_NH3");
  set_double_value(ps_MaxMineralisationDepth, j, "MaxMineralisationDepth");

	return res;
}

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
  : _pathToOutputDir(".")
  , precipCorrectionValues(12, 1.0)
{

}

CentralParameterProvider::CentralParameterProvider(json11::Json j)
{
	merge(j);
}

Errors CentralParameterProvider::merge(json11::Json j)
{
	Errors res;

	res.append(userCropParameters.merge(j["userCropParameters"]));
	res.append(userEnvironmentParameters.merge(j["userEnvironmentParameters"]));
	res.append(userSoilMoistureParameters.merge(j["userSoilMoistureParameters"]));
	res.append(userSoilTemperatureParameters.merge(j["userSoilTemperatureParameters"]));
	res.append(userSoilTransportParameters.merge(j["userSoilTransportParameters"]));
	res.append(userSoilOrganicParameters.merge(j["userSoilOrganicParameters"]));
	res.append(simulationParameters.merge(j["simulationParameters"]));
	res.append(siteParameters.merge(j["siteParameters"]));
	//groundwaterInformation.merge(j["groundwaterInformation"]);

	//set_bool_value(_writeOutputFiles, j, "writeOutputFiles");

	return res;
}

json11::Json CentralParameterProvider::to_json() const
{
	return json11::Json::object{
			{"type", "CentralParameterProvider"}
		, {"userCropParameters", userCropParameters.to_json()}
		, {"userEnvironmentParameters", userEnvironmentParameters.to_json()}
		, {"userSoilMoistureParameters", userSoilMoistureParameters.to_json()}
		, {"userSoilTemperatureParameters", userSoilTemperatureParameters.to_json()}
		, {"userSoilTransportParameters", userSoilTransportParameters.to_json()}
		, {"userSoilOrganicParameters", userSoilOrganicParameters.to_json()}
		, {"simulationParameters", simulationParameters.to_json()}
		, {"siteParameters", siteParameters.to_json()}
		//, {"groundwaterInformation", groundwaterInformation.to_json()}
		//, {"writeOutputFiles", writeOutputFiles()}
	};
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
