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
#include "tools/algorithms.h"
#include "monica-parameters.h"
#include "monica.h"
#include "tools/debug.h"
#include "soil/conversion.h"
#include "soil/soil.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Soil;
using namespace Tools;
using namespace Climate;

// Some profile definitions for eva2

namespace
{
  /*!
 * @todo Claas bitte ausfüllen
 * @param name
 * @return
 */
  pair<FertiliserType, int> hermesFertiliserName2monicaFertiliserId(const string& name)
  {
    if (name == "KN")
      return make_pair(mineral, 7); //0.00 1.00 0.00 01.00 M Kaliumnitrat (Einh : kg N / ha)
    if (name == "KAS")
      return make_pair(mineral, 1); //1.00 0.00 0.00 01.00 M Kalkammonsalpeter (Einh : kg N / ha)
    if (name == "UR")
      return make_pair(mineral, 8); //1.00 0.00 0.00 01.00   M Harnstoff
    if (name == "AHL")
      return make_pair(mineral, 10); //1.00 0.00 0.00 01.00   M Ammoniumharnstoffloesung
    if (name == "UAN")
      return make_pair(mineral, 9); //1.00 0.00 0.00 01.00   M Urea ammonium nitrate solution
    if (name == "AS")
      return make_pair(mineral, 3); //1.00 0.00 0.00 01.00   M Ammoniumsulfat (Einh: kg N/ha)
    if (name == "DAP")
      return make_pair(mineral, 2); //1.00 0.00 0.00 01.00   M Diammoniumphosphat (Einh: kg N/ha)
    if (name == "SG")
      return make_pair(organic, 7); //0.67 0.00 1.00 06.70   O Schweineguelle (Einh: kg FM/ha)
	if (name == "SU")
	  return make_pair(organic, 8); //0.67 0.00 1.00 06.70   O Schweineurin (Einh: kg FM/ha)
	if (name == "RG1")
      return make_pair(organic, 3); //0.43 0.00 1.00 02.40   O Rinderguelle (Einh: kg FM/ha)
    if (name == "RG2")
      return make_pair(organic, 3); //0.43 0.00 1.00 01.80   O Rinderguelle (Einh: kg FM/ha)
    if (name == "RG3")
      return make_pair(organic, 3); //0.43 0.00 1.00 03.40   O Rinderguelle (Einh: kg FM/ha)
    if (name == "RG4")
      return make_pair(organic, 3); //0.43 0.00 1.00 03.70   O Rinderguelle (Einh: kg FM/ha)
    if (name == "RG5")
      return make_pair(organic, 3); //0.43 0.00 1.00 03.30   O Rinderguelle (Einh: kg FM/ha)
    if (name == "SM")
      return make_pair(organic, 1); //0.15 0.20 0.80 00.60   O Stallmist (Einh: kg FM/ha)
    if (name == "ST1")
      return make_pair(organic, 1); //0.07 0.10 0.90 00.48   O Stallmist (Einh: kg FM/ha)
    if (name == "ST2")
      return make_pair(organic, 1); //0.07 0.10 0.90 00.63   O Stallmist (Einh: kg FM/ha)
    if (name == "ST3")
      return make_pair(organic, 1); //0.07 0.10 0.90 00.82   O Stallmist (Einh: kg FM/ha)
    if (name == "RM1")
      return make_pair(organic, 2); //0.15 0.20 0.80 00.60   O Stallmist (Einh: kg FM/ha)
    if (name == "FM")
      return make_pair(organic, 1); //0.65 0.80 0.20 01.00   O Stallmist (Einh: kg FM/ha)
    if (name == "LM")
      return make_pair(organic, 3); //0.85 0.80 0.20 01.00   O Jauche (Einh: kg FM/ha)
    if (name == "H")
      return make_pair(mineral, 8); //01.00 1.00 0.00 0.00 1.00 0.15 kg N/ha 	M Harnstoff
    if (name == "NPK")
      return make_pair(mineral, 5); //01.00 1.00 0.00 0.00 0.00 0.10 kg N/ha 	M NPK Mineraldünger
    if (name == "ALZ")
      return make_pair(mineral, 8); //01.00 1.00 0.00 0.00 1.00 0.12 kg N/ha 	M Alzon
    if (name == "AZU")
      return make_pair(mineral, 1); //01.00 1.00 0.00 0.00 1.00 0.12 kg N/ha 	M Ansul
    if (name == "NIT")
      return make_pair(mineral, 5); //01.00 1.00 0.00 0.00 0.00 0.10 kg N/ha 	M Nitrophoska
    if (name == "SSA")
      return make_pair(mineral, 3); //01.00 1.00 0.00 0.00 1.00 0.10 kg N/ha 	M schwefelsaures Ammoniak
    if (name == "RG")
      return make_pair(organic, 3); //04.70 0.43 0.00 1.00 1.00 0.40 m3 / ha 	O Rindergülle (Einh: kg FM/ha)
    if (name == "RM")
      return make_pair(organic, 1); //00.60 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Rinderfestmist (Einh: kg FM/ha)
	if (name == "RM")
	  return make_pair(organic, 2); //00.60 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Rindermist (Einh: kg FM/ha)
	if (name == "RSG")
      return make_pair(organic, 3); //05.70 0.55 0.00 1.00 1.00 0.40 m3 / ha 	O Rinder/Schweinegülle (Einh: kg FM/ha)
    if (name == "SM")
      return make_pair(organic, 6); //00.76 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Schweinemist (Einh: kg FM/ha)
	if (name == "SSM")
	  return make_pair(organic, 5); //00.76 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Schweinefestmist (Einh: kg FM/ha)
	if (name == "HG")
      return make_pair(organic, 12); //10.70 0.68 0.00 1.00 1.00 0.40 m3 / ha 	O Hühnergülle (Einh: kg FM/ha)
    if (name == "HFM")
      return make_pair(organic, 11); //02.30 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Hähnchentrockenmist (Einh: kg FM/ha)
    if (name == "HM")
      return make_pair(organic, 11); //02.80 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Hühnermist (Einh: kg FM/ha)
    if (name == "CK")
      return make_pair(mineral, 1); //00.30 0.00 1.00 0.00 0.00 0.00 dt / ha 	M Carbokalk
    if (name == "KSL")
      return make_pair(organic, 16); //01.00 0.25 0.20 0.80 0.00 0.10 dt / ha 	O Klärschlamm (Einh: kg FM/ha)
    if (name == "BAK")
      return make_pair(organic, 15); //01.63 0.00 0.05 0.60 0.00 0.00 dt / ha 	O Bioabfallkompst (Einh: kg FM/ha)
    if (name == "MST")
      return make_pair(organic, 21); // Maize straw
    if (name == "WST")
      return make_pair(organic, 19); // Wheat straw
    if (name == "SST")
      return make_pair(organic, 23); // Soybean straw
    if (name == "WEE")
      return make_pair(organic, 22); // Weeds
    if (name == "YP3")
      return make_pair(mineral, 13); //01.00 0.43 0.57 0.00 1.00 1.00 kg N/ha 	M Yara Pellon Y3
	if (name == "ASH")
	  return make_pair(organic, 24); // Ashes from burnt forest understorey

    cout << "Error: Cannot find fertiliser " << name << " in hermes fertiliser map. Aborting..." << endl;
    exit(-1);
    return make_pair(mineral, -1);
  }

} // namespace

//------------------------------------------------------------------------------

/**
 * Returns the result vector of a special output. Python swig is
 * not able to wrap stl-maps correctly. Stl-vectors are working
 * properly so this function has been implemented to use the results
 * in wrapped python code.
 *
 * @param id ResultId of output
 * @return Vector of result values
 */
std::vector<double>
Result::getResultsById(int id)
{
  // test if crop results are requested
  if (id == primaryYield || id == secondaryYield || id == sumIrrigation ||
      id == sumFertiliser || id == biomassNContent || id == sumTotalNUptake ||
      id == cropHeight || id == cropname || id == sumETaPerCrop || sumTraPerCrop ||
      id == primaryYieldTM || id == secondaryYieldTM || id == daysWithCrop || id == aboveBiomassNContent ||
      id == NStress || id == WaterStress || id == HeatStress || id == OxygenStress || id == aboveGroundBiomass || 
      id == anthesisDay || id == maturityDay || id == harvestDay )
  {
    vector<double> result_vector;
    int size = pvrs.size();
    for (int i=0; i<size; i++)
    {
      PVResult crop_result = pvrs.at(i);
      result_vector.push_back(crop_result.pvResults[(ResultId)id]);
    }
    return result_vector;
  }

  return generalResults[(ResultId)id];
}


const vector<ResultId>& Monica::cropResultIds()
{
  static ResultId ids[] =
  {
    primaryYield, secondaryYield, sumFertiliser,
    sumIrrigation, anthesisDay, maturityDay, harvestDay//, sumMineralisation
  };
  static vector<ResultId> v(ids, ids + 6);//5);

  return v;
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

const vector<ResultId>& Monica::monthlyResultIds()
{
  static ResultId ids[] =
  {
    avg10cmMonthlyAvgCorg, avg30cmMonthlyAvgCorg,
    mean90cmMonthlyAvgWaterContent,
    monthlySumGroundWaterRecharge, monthlySumNLeaching
  };
  static vector<ResultId> v(ids, ids + 5);

  return v;
}


//------------------------------------------------------------------------------

const vector<int>& Monica::CCGermanyResultIds()
{
  static ResultId ids[] =
  {
    primaryYield,                   // done
    yearlySumGroundWaterRecharge,
    yearlySumNLeaching
  };

  static vector<int> v(ids, ids+3);

  return v;
}

//------------------------------------------------------------------------------

const vector<int>& Monica::eva2CropResultIds()
{
  static ResultId ids[] =
  {
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
    OxygenStress
  };
  static vector<int> v(ids, ids + 12);

  return v;
}


//------------------------------------------------------------------------------

const vector<int>& Monica::eva2MonthlyResultIds()
{
  static ResultId ids[] =
  {
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
    monthlySoilMoistureL18

  };
  static vector<int> v(ids, ids + 27);

  return v;
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
	default: ;
	}
	return ResultIdInfo("", "");
}

//------------------------------------------------------------------------------

string WorkStep::toString() const
{
  ostringstream s;
  s << "date: " << date().toString();
  return s.str();
}

//------------------------------------------------------------------------------

void Seed::apply(MonicaModel* model)
{
  debug() << "seeding crop: " << _crop->toString() << " at: " << date().toString() << endl;
  model->seedCrop(_crop);
}

string Seed::toString() const
{
  ostringstream s;
  s << "seeding at: " << date().toString() << " crop: " << _crop->toString();
  return s.str();
}



//------------------------------------------------------------------------------

void Harvest::apply(MonicaModel* model)
{
  if(model->cropGrowth())
  {
    if ((_method == "total") || (_method == "fruitHarvest") || (_method == "cutting"))
		{
			debug() << "harvesting crop: " << _crop->toString() << " at: " << date().toString() << endl;
			if (model->currentCrop() == _crop)
			{
				if (model->cropGrowth()) {
					_crop->setHarvestYields
						(model->cropGrowth()->get_FreshPrimaryCropYield() /
						100.0, model->cropGrowth()->get_FreshSecondaryCropYield() / 100.0);
					_crop->setHarvestYieldsTM
						(model->cropGrowth()->get_PrimaryCropYield() / 100.0,
						model->cropGrowth()->get_SecondaryCropYield() / 100.0);

					_crop->setYieldNContent(model->cropGrowth()->get_PrimaryYieldNContent(),
						model->cropGrowth()->get_SecondaryYieldNContent());
					_crop->setSumTotalNUptake(model->cropGrowth()->get_SumTotalNUptake());
					_crop->setCropHeight(model->cropGrowth()->get_CropHeight());
					_crop->setAccumulatedETa(model->cropGrowth()->get_AccumulatedETa());
					_crop->setAccumulatedTranspiration(model->cropGrowth()->get_AccumulatedTranspiration());
					_crop->setAnthesisDay(model->cropGrowth()->getAnthesisDay());
					_crop->setMaturityDay(model->cropGrowth()->getMaturityDay());
				}

				//store results for this crop
				_cropResult->pvResults[primaryYield] = _crop->primaryYield();
				_cropResult->pvResults[secondaryYield] = _crop->secondaryYield();
				_cropResult->pvResults[primaryYieldTM] = _crop->primaryYieldTM();
				_cropResult->pvResults[secondaryYieldTM] = _crop->secondaryYieldTM();
				_cropResult->pvResults[sumIrrigation] = _crop->appliedIrrigationWater();
				_cropResult->pvResults[biomassNContent] = _crop->primaryYieldN();
				_cropResult->pvResults[aboveBiomassNContent] = _crop->aboveGroundBiomasseN();
				_cropResult->pvResults[aboveGroundBiomass] = _crop->aboveGroundBiomass();
				_cropResult->pvResults[daysWithCrop] = model->daysWithCrop();
				_cropResult->pvResults[sumTotalNUptake] = _crop->sumTotalNUptake();
				_cropResult->pvResults[cropHeight] = _crop->cropHeight();
				_cropResult->pvResults[sumETaPerCrop] = _crop->get_AccumulatedETa();
				_cropResult->pvResults[sumTraPerCrop] = _crop->get_AccumulatedTranspiration();
				_cropResult->pvResults[cropname] = _crop->id();
				_cropResult->pvResults[NStress] = model->getAccumulatedNStress();
				_cropResult->pvResults[WaterStress] = model->getAccumulatedWaterStress();
				_cropResult->pvResults[HeatStress] = model->getAccumulatedHeatStress();
				_cropResult->pvResults[OxygenStress] = model->getAccumulatedOxygenStress();
				_cropResult->pvResults[anthesisDay] = _crop->getAnthesisDay();
        _cropResult->pvResults[maturityDay] = _crop->getMaturityDay();
        _cropResult->pvResults[harvestDay] = date().julianDay();

        if (_method == "total")
          model->harvestCurrentCrop(_exported);
        else if (_method == "fruitHarvest")
          model->fruitHarvestCurrentCrop(_percentage, _exported);
        else if (_method == "cutting")
          model->cuttingCurrentCrop(_percentage, _exported);
      }
			else
			{
				debug() << "Crop: " << model->currentCrop()->toString()
					<< " to be harvested isn't actual crop of this Harvesting action: "
					<< _crop->toString() << endl;
			}
		}
		else if (_method == "leafPruning")
		{
			debug() << "pruning leaves of: " << _crop->toString() << " at: " << date().toString() << endl;
			if (model->currentCrop() == _crop)
			{
				model->leafPruningCurrentCrop(_percentage, _exported);
			}
			else
			{
				debug() << "Crop: " << model->currentCrop()->toString()
					<< " to be pruned isn't actual crop of this harvesting action: "
					<< _crop->toString() << endl;
			}
		}
		else if (_method == "tipPruning")
		{
			debug() << "pruning tips of: " << _crop->toString() << " at: " << date().toString() << endl;
			if (model->currentCrop() == _crop)
			{
				model->tipPruningCurrentCrop(_percentage, _exported);
			}
			else
			{
				debug() << "Crop: " << model->currentCrop()->toString()
					<< " to be pruned isn't actual crop of this harvesting action: "
					<< _crop->toString() << endl;
			}
		}
		else if (_method == "shootPruning")
		{
			debug() << "pruning shoots of: " << _crop->toString() << " at: " << date().toString() << endl;
			if (model->currentCrop() == _crop)
			{
				model->shootPruningCurrentCrop(_percentage, _exported);
			}
			else
			{
				debug() << "Crop: " << model->currentCrop()->toString()
					<< " to be pruned isn't actual crop of this harvesting action: "
					<< _crop->toString() << endl;
			}
		}
  } else {
      debug() << "Cannot harvest crop because there is not one anymore" << endl;
      debug() << "Maybe automatic harvest trigger was already activated so that the ";
      debug() << "crop was already harvested. This must be the fallback harvest application ";
      debug() << "that is not necessary anymore and should be ignored" << endl;
  }
}

string Harvest::toString() const
{
  ostringstream s;
  s << "harvesting at: " << date().toString() << " crop: " << _crop->toString();
  return s.str();
}

//------------------------------------------------------------------------------


// ----------------------------------------------------------------------------
void Cutting::apply(MonicaModel* model)
{
  debug() << "Cutting crop: " << _crop->toString() << " at: " << date().toString() << endl;

  if (model->currentCrop() == _crop)
  {
    if (model->cropGrowth()) {
      _crop->setHarvestYields
          (model->cropGrowth()->get_FreshPrimaryCropYield() /
           100.0, model->cropGrowth()->get_FreshSecondaryCropYield() / 100.0);

      _crop->setHarvestYieldsTM
                (model->cropGrowth()->get_PrimaryCropYield() / 100.0,
                 model->cropGrowth()->get_SecondaryCropYield() / 100.0);
    }

    _crop->setYieldNContent(model->cropGrowth()->get_PrimaryYieldNContent(),
                            model->cropGrowth()->get_SecondaryYieldNContent());

    _crop->setSumTotalNUptake(model->cropGrowth()->get_SumTotalNUptake());
    _crop->setCropHeight(model->cropGrowth()->get_CropHeight());
	

    if (model->cropGrowth()) {
        model->cropGrowth()->applyCutting();
    }


  }
}

string Cutting::toString() const
{
  ostringstream s;
  s << "Cutting at: " << date().toString() << " crop: " << _crop->toString();
  return s.str();
}

//------------------------------------------------------------------------------

string NMinCropParameters::toString() const
{
  ostringstream s;
  s << "samplingDepth: " << samplingDepth << " nTarget: " << nTarget << " nTarget40: " << nTarget30;
  return s.str();
}

//------------------------------------------------------------------------------

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

string NMinUserParameters::toString() const
{
  ostringstream s;
  s << "min: " << min << " max: " << max << " delay: " << delayInDays << " days";
  return s.str();
}

//------------------------------------------------------------------------------

void MineralFertiliserApplication::apply(MonicaModel* model)
{
  debug() << toString() << endl;
  model->applyMineralFertiliser(partition(), amount());
}

string MineralFertiliserApplication::toString() const
{
  ostringstream s;
  s << "applying mineral fertiliser at: " << date().toString() << " amount: " << amount() << " partition: "
      << partition().toString();
  return s.str();
}

//------------------------------------------------------------------------------

OrganicFertiliserApplication::OrganicFertiliserApplication(const Tools::Date& at,
                             const OrganicMatterParameters* params,
                             double amount,
                             bool incorp)
  : WorkStep(at),
    _params(params),
    _amount(amount),
    _incorporation(incorp)
{}

OrganicFertiliserApplication::OrganicFertiliserApplication(json11::Json j)
  : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value())),
    _paramsPtr(std::make_shared<OrganicMatterParameters>(j["parameters"])),
    _params(_paramsPtr.get()),
    _amount(j["amount"].number_value()),
    _incorporation(j["incorporation"].bool_value())
{}

json11::Json OrganicFertiliserApplication::to_json() const
{
  auto p = _params ? _params->to_json() : json11::Json();
  return json11::Json::object {
    {"type", "OrganicFertiliserApplication"},
    {"date", date().toIsoDateString()},
    {"amount", _amount},
    {"parameters", p},
    {"incorporation", _incorporation}};
}

void OrganicFertiliserApplication::apply(MonicaModel* model)
{
  debug() << toString() << endl;
  model->applyOrganicFertiliser(_params, _amount, _incorporation);
}

string OrganicFertiliserApplication::toString() const
{
  ostringstream s;
  s << "applying organic fertiliser at: " << date().toString() << " amount: " << amount() << "\tN percentage: " << _params->vo_NConcentration << "\tN amount: " << amount() * _params->vo_NConcentration; // << "parameters: " << endl;
  return s.str();
}

//------------------------------------------------------------------------------

void TillageApplication::apply(MonicaModel* model)
{
  debug() << toString() << endl;
  model->applyTillage(_depth);
}

string TillageApplication::toString() const
{
  ostringstream s;
  s << "applying tillage at: " << date().toString() << " depth: " << depth();
  return s.str();
}

//------------------------------------------------------------------------------

string IrrigationParameters::toString() const
{
  ostringstream s;
  s << "nitrateConcentration: " << nitrateConcentration << " sulfateConcentration: " << sulfateConcentration;
  return s.str();
}

string AutomaticIrrigationParameters::toString() const
{
  ostringstream s;
  s << "amount: " << amount << " treshold: " << treshold << " " << IrrigationParameters::toString();
  return s.str();

}

void IrrigationApplication::apply(MonicaModel* model)
{
  //cout << toString() << endl;
  model->applyIrrigation(amount(), nitrateConcentration());

}

string IrrigationApplication::toString() const
{
  ostringstream s;
  s << "applying irrigation at: " << date().toString() << " amount: " << amount() << " nitrateConcentration: "
      << nitrateConcentration() << " sulfateConcentration: " << sulfateConcentration();
  return s.str();
}


//------------------------------------------------------------------------------

/**
 * @brief Returns a short summary with information about automatic yield parameter configuration.
 * @brief String
 */
string AutomaticHarvestParameters::toString() const
{
	ostringstream s;
	if (_harvestTime == AutomaticHarvestTime::maturity) {
		s << "Automatic harvestTime: Maturity ";
	}

	return s.str();
}


//------------------------------------------------------------------------------


void MeasuredGroundwaterTableInformation::readInGroundwaterInformation(std::string path)
{
  ifstream ifs(path.c_str(), ios::in);
   if (!ifs.is_open()) {
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

double MeasuredGroundwaterTableInformation::getGroundwaterInformation(Tools::Date gwDate)
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

ProductionProcess::ProductionProcess(const std::string& name, CropPtr crop)
  : _name(name),
    _crop(crop),
    _cropResult(new PVResult())
{
  debug() << "ProductionProcess: " << name.c_str() << endl;
  _cropResult->id = _crop->id();

  if ((crop->seedDate() != Date(1, 1, 1951)) && (crop->seedDate() != Date(0, 0, 0)))
    addApplication(Seed(crop->seedDate(), crop));
	
  if ((crop->harvestDate() != Date(1,1,1951)) && (crop->harvestDate() != Date(0,0,0)) )
  {
    debug() << "crop->harvestDate(): " << crop->harvestDate().toString().c_str() << endl;
    addApplication(Harvest(crop->harvestDate(), crop, _cropResult));
  }

  for(Date cd : crop->getCuttingDates())
	{
    debug() << "Add cutting date: " << cd.toString() << endl;
    addApplication(Cutting(cd, crop));
	}
}

/**
 * @brief Copy constructor
 * @param new_pp
 */
/*
ProductionProcess::ProductionProcess(const ProductionProcess& other)
{
  _name = other._name;
  _crop = CropPtr(new Crop(*(other._crop.get())));
  _cropResult = PVResultPtr(new PVResult(*(other._cropResult.get())));

  _worksteps = other._worksteps;
}
*/

ProductionProcess ProductionProcess::deepCloneAndClearWorksteps() const
{
  ProductionProcess clone(name(), CropPtr(new Crop(*(crop().get()))));
  clone._cropResult = PVResultPtr(new PVResult(*(_cropResult.get())));
  return clone;
}

void ProductionProcess::apply(const Date& date, MonicaModel* model) const
{
//  typedef multimap<Date, WSPtr>::const_iterator CI;
//  pair<CI, CI> p = _worksteps.equal_range(date);
//  if (p.first != p.second)
//  {
//    while (p.first != p.second)
//    {
//      p.first->second->apply(model);
//      p.first++;
//    }
//  }
  auto p = _worksteps.equal_range(date);
  while (p.first != p.second)
  {
    p.first->second->apply(model);
    p.first++;
  }
}

Date ProductionProcess::nextDate(const Date& date) const
{
  //typedef multimap<Date, WSPtr>::const_iterator CI;
  auto ci = _worksteps.upper_bound(date);
  return ci != _worksteps.end() ? ci->first : Date();
}

Date ProductionProcess::start() const
{
  if (_worksteps.empty())
    return Date();
  return _worksteps.begin()->first;
}

Date ProductionProcess::end() const
{
  if (_worksteps.empty())
    return Date();
  return _worksteps.rbegin()->first;
}

std::string ProductionProcess::toString() const
{
  ostringstream s;

	s << "name: " << name() << " start: " << start().toString()
      << " end: " << end().toString() << endl;
  s << "worksteps:" << endl;
  typedef multimap<Date, WSPtr>::const_iterator CI;
  for (CI ci = _worksteps.begin(); ci != _worksteps.end(); ci++)
  {
    s << "at: " << ci->first.toString()
        << " what: " << ci->second->toString() << endl;
  }
  return s.str();
}

//------------------------------------------------------------------------------

//helper for parsing dates

struct DMY
{
  int d, m, y;
  Date toDate(bool useLeapYears = true) const
  {
    return Date(d, m, y, useLeapYears);
  }
};

// to read HERMES two digit date format in management files
struct ParseDate
{
  DMY operator()(const string & d)
  {
    DMY r;
	int length = d.size();

	if (length == 6) {
		// old HERMES format ddmmyy
		r.d = atoi(d.substr(0, 2).c_str());
		r.m = atoi(d.substr(2, 2).c_str());
		r.y = atoi(d.substr(4, 2).c_str());
		r.y = r.y <= 72 ? 2000 + r.y : 1900 + r.y;

	}
	else if (length == 8) {
		
		//ddmmyyyy
		r.d = atoi(d.substr(0, 2).c_str());
		r.m = atoi(d.substr(2, 2).c_str());
		r.y = atoi(d.substr(4, 4).c_str());

	}
	else {
		// other unexpected length
		cout << "ERROR - Cannot parse date \"" << d.c_str() << "\"" << endl;
		cout << "Should be of format DDMMYY or DDMMYYYY" << endl;		
		cout << "Aborting simulation now!" << endl;
		exit(-1);
	}
	
    return r;
  }
} parseDate;

//----------------------------------------------------------------------------

vector<ProductionProcess>
    Monica::cropRotationFromHermesFile(const string& pathToFile, bool useAutomaticHarvestTrigger, AutomaticHarvestParameters autoHarvestParams)
{
  vector<ProductionProcess> ff;

  ifstream ifs(pathToFile.c_str(), ios::binary);
  if (! ifs.good()) {
    cerr << "Could not open file " << pathToFile.c_str() << " . Aborting now!" << endl;
    exit(1);
  }

  string s;

  //skip first line
  getline(ifs, s);

  while (getline(ifs, s))
  {
    if (trim(s) == "end")
      break;

    istringstream ss(s);
    string crp;
    string sowingDate, harvestDate, tillageDate;
    double exp, tillage_depth;
    int t;
    ss >> t >> crp >> sowingDate >> harvestDate >> tillageDate >> exp  >> tillage_depth;

    Date sd = parseDate(sowingDate).toDate(true);    
	Date td = parseDate(tillageDate).toDate(true);
	Date hd;
    



    // tst if dates are valid
    if (!sd.isValid() || !td.isValid())
    {
      debug() << "Error - Invalid sowing or tillage date in \"" << pathToFile.c_str() << "\"" << endl;
      debug() << "Line: " << s.c_str() << endl;
      debug() << "Aborting simulation now!" << endl;
      exit(-1);
    }
    
    //create crop
    CropPtr crop = hermesCropId2Crop(crp);
    crop->setCropParameters(getCropParametersFromMonicaDB(crop->id()));
    crop->setResidueParameters(getResidueParametersFromMonicaDB(crop->id()));
    
    if (! useAutomaticHarvestTrigger) {
        // Do not use automatic harvest trigger
        // Adds harvest date from crop rotation file
		hd = parseDate(harvestDate).toDate(true);
		
		if (!hd.isValid()) {
			debug() << "Error - Invalid harvest date in \"" << pathToFile.c_str() << "\"" << endl;
			debug() << "Line: " << s.c_str() << endl;
			debug() << "Aborting simulation now!" << endl;
			exit(-1);
		}
	} else {
        
        debug() << "Activate automatic Harvest Trigger" << endl;  
        
        // use harvest trigger
        // change latest harvest date to crop specific fallback harvest doy        
		autoHarvestParams.setLatestHarvestDOY(crop->cropParameters()->pc_LatestHarvestDoy);
        crop->activateAutomaticHarvestTrigger(autoHarvestParams);
        
        int harvest_year = sd.year();
        if (crp == "WW" || crp=="SW" || crp=="WG" || crp=="WR" || crp=="WR_GD" || crp=="SB" ||
            crp=="WC" || crp=="WTR") 
        {
            // increment harvest year based on sowing year for winter crops
            // to determine harvest year for automatic harvest trigger
            harvest_year++;                
        } 
        
        debug() << "harvest_year:\t" << harvest_year << endl;
        
        // ###################################################################
        // # Important notes for the automatic harvest trigger (by XS)
        // ###################################################################        
        // @TODO: Change work flow of automatic harvest trigger
        // ###################################################################
        // Automatic harvest trigger works as follows:
        // If activated the latest harvest doy for the crop is automatically  used as
        // harvest date. A harvest application is added as usual when creating
        // the production process (constructor).
        // If the harvest trigger is activated during simulation (main loop in monica.cpp)
        // a new harvest application is created and directly applied without adding
        // it to the list of applications in the production process.
        // Because the fallback harvest application was already added as a harvest event
        // in the production process during creation of the PP, a crop may be harvested 
        // two times. But the apply method of the harvest application tests before
        // doing anything if there is a valid crop pointer. If not, nothing is done.
        // So if a harvest application is called a second time nothing happens ...
        // ###################################################################
        
        // set harvest date to latest crop specific fallback harvest date
        hd = Date::julianDate(crop->cropParameters()->pc_LatestHarvestDoy, harvest_year, true);
        
    }

    
    crop->setSeedAndHarvestDate(sd, hd);
    


    ProductionProcess pp(crp, crop);
    pp.addApplication(TillageApplication(td, (tillage_depth/100.0) ));
    //cout << "production-process: " << pp.toString() << endl;

    ff.push_back(pp);
  }

  return ff;
}

/**
 * @todo Micha/Xenia: Überprüfen, ob YearIndex rauskommen kann.
 */
DataAccessor Monica::climateDataFromHermesFiles(const std::string& pathToFile,
                                                int fromYear, int toYear,
                                                const CentralParameterProvider& cpp,
                                                bool useLeapYears,
                                                double latitude)
{
  DataAccessor da(Date(1, 1, fromYear, useLeapYears),
                  Date(31, 12, toYear, useLeapYears));

  vector<double> _tmin;
  vector<double> _tavg;
  vector<double> _tmax;
  vector<double> _globrad;
  vector<double> _relhumid;
  vector<double> _wind;
  vector<double> _precip;
  vector<double> _sunhours;

  Date date = Date(1, 1, fromYear, useLeapYears);

  for (int y = fromYear; y <= toYear; y++)
  {

    ostringstream yss;
    yss << y;
    string ys = yss.str();
    ostringstream oss;
    oss << pathToFile << ys.substr(1, 3);
    debug() << "File: " << oss.str().c_str() << endl;
    ifstream ifs(oss.str().c_str(), ios::binary);
    if (! ifs.good()) {
      cerr << "Could not open file " << oss.str().c_str() << " . Aborting now!" << endl;
      exit(1);
    }
    string s;

    //skip first line(s)
    getline(ifs, s);
    getline(ifs, s);
    getline(ifs, s);

    int daysCount = 0;
    int allowedDays = Date(31, 12, y, useLeapYears).dayOfYear();
    //    cout << "tavg\t" << "tmin\t" << "tmax\t" << "wind\t"
    debug() << "allowedDays: " << allowedDays << " " << y<< "\t" << useLeapYears << "\tlatitude:\t" << latitude << endl;
    //<< "sunhours\t" << "globrad\t" << "precip\t" << "ti\t" << "relhumid\n";
    while (getline(ifs, s))
    {
      //if(trim(s) == "end") break;

      //Tp_av Tpmin Tpmax T_s10 T_s20 vappd wind sundu radia prec jday RF
      double td;
      int ti;
      double tmin, tmax, tavg, wind, sunhours, globrad, precip, relhumid;
      istringstream ss(s);

      ss >> tavg >> tmin >> tmax >> td >> td >> td >> wind
          >> sunhours >> globrad >> precip >> ti >> relhumid;

			// test if globrad or sunhours should be used
			if(globrad >=0.0) 
			{
				// use globrad
				// HERMES weather files deliver global radiation as [J cm-2]
				// Here, we push back [MJ m-2 d-1]
				double globradMJpm2pd = globrad * 100.0 * 100.0 / 1000000.0;
				_globrad.push_back(globradMJpm2pd);
			} 
			else if(sunhours >= 0.0) 
			{
				// invalid globrad use sunhours
				// convert sunhours into globrad
				// debug() << "Invalid globrad - use sunhours instead" << endl;
				_globrad.push_back(sunshine2globalRadiation(date.dayOfYear(), sunhours, latitude, true));    
				_sunhours.push_back(sunhours);
			} 
			else
			{
				// error case
				debug() << "Error: No global radiation or sunhours specified for day " << date.toString().c_str() << endl;
				debug() << "Aborting now ..." << endl;
				exit(-1);
			}

			if (relhumid>=0.0) {
			    _relhumid.push_back(relhumid);
			}
			        
      // precipitation correction by Richter values
      precip*=cpp.getPrecipCorrectionValue(date.month()-1);

      //      cout << tavg << "\t"<< tmin << "\t" << tmax << "\t" << wind
      //<< "\t" << sunhours <<"\t" << globrad <<"\t" << precip <<"\t" << ti <<"\t" << relhumid << endl;

      _tavg.push_back(tavg);
      _tmin.push_back(tmin);
      _tmax.push_back(tmax);
      _wind.push_back(wind);
			_precip.push_back(precip);

			
      daysCount++;
      date++;
    }

    if (daysCount != allowedDays)
    {
      debug() << "Wrong number of days in " << oss.str().c_str() << " ." << " Found " << daysCount << " days but should have been "
          << allowedDays << " days. Aborting." << endl;
      exit(1);
    }
  }

  //int days = (toYearIndex - fromYearIndex + 1) * 365;
  //assert(int(_tmin.size()) == days);

  da.addClimateData(tmin, _tmin);
  da.addClimateData(tmax, _tmax);
  da.addClimateData(tavg, _tavg);
	da.addClimateData(globrad, _globrad);
	da.addClimateData(wind, _wind);
	da.addClimateData(precip, _precip);

	if(!_sunhours.empty())
		da.addClimateData(sunhours, _sunhours);

	if (!_relhumid.empty()) {
	    da.addClimateData(relhumid, _relhumid);
	}

  return da;
}

//----------------------------------------------------------------------------

/**
 * @brief Constructor
 *
 * Parameter initiliazation
 */
CropParameters::CropParameters() :
    pc_NumberOfDevelopmentalStages(0),
    pc_NumberOfOrgans(0),
    pc_CarboxylationPathway(0),
    pc_DefaultRadiationUseEfficiency(0),
    pc_PartBiologicalNFixation(0),
    pc_InitialKcFactor(0),
    pc_LuxuryNCoeff(0),
    pc_MaxAssimilationRate(0),
    pc_MaxCropHeight(0),
    pc_CropHeightP1(0),
    pc_CropHeightP2(0),
    pc_MinimumNConcentration(0),
    pc_MinimumTemperatureForAssimilation(0),
    pc_NConcentrationAbovegroundBiomass(0),
    pc_NConcentrationB0(0),
    pc_NConcentrationPN(0),
    pc_NConcentrationRoot(0),
    pc_ResidueNRatio(0),
    pc_DevelopmentAccelerationByNitrogenStress(0),
    pc_CuttingDelayDays(0),
    pc_FieldConditionModifier(1.0),
	pc_AssimilateReallocation(0.0),
	pc_LT50cultivar(0.0),
	pc_LatestHarvestDoy(-1)
{}

/**
 * @brief
 */
void CropParameters::resizeStageOrganVectors()
{
  pc_AssimilatePartitioningCoeff.resize(pc_NumberOfDevelopmentalStages,
                                        std::vector<double>(pc_NumberOfOrgans));
  pc_OrganSenescenceRate.resize(pc_NumberOfDevelopmentalStages,
                              std::vector<double>(pc_NumberOfOrgans));
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

//------------------------------------------------------------------------------

/**
 * @brief Returns data structure for crop parameters with values from DB
 *
 * A datastructure for crop parameters is created, initialized with
 * database values and returned. This structure will be initialized only
 * once. Similar to a singleton pattern.
 *
 * @param cropId
 *
 * @return Reference to crop parameters
 */
const CropParameters* Monica::getCropParametersFromMonicaDB(int cropId)
{
  static mutex lockable;

  static bool initialized = false;
  typedef std::shared_ptr<CropParameters> CPPtr;
  typedef map<int, CPPtr> CPS;

  static CPS cpss;

  // only initialize once
  if (!initialized)
  {
    lock_guard<mutex> lock(lockable);

    //test if after waiting for the lock the other thread
    //already initialized the whole thing
    if (!initialized)
    {

      DB *con = newConnection("monica");
      DBRow row;
      std::string text_request = "select id, name, perennial, max_assimilation_rate, "
          "carboxylation_pathway, minimum_temperature_for_assimilation, "
          "crop_specific_max_rooting_depth, min_n_content, "
          "n_content_pn, n_content_b0, "
          "n_content_above_ground_biomass, n_content_root, initial_kc_factor, "
          "development_acceleration_by_nitrogen_stress, fixing_n, "
          "luxury_n_coeff, max_crop_height, residue_n_ratio, "
          "sampling_depth, target_n_sampling_depth, target_n30, "
          "default_radiation_use_efficiency, crop_height_P1, crop_height_P2, "
          "stage_at_max_height, max_stem_diameter, stage_at_max_diameter, "
          "heat_sum_irrigation_start, heat_sum_irrigation_end, "
          "max_N_uptake_p, root_distribution_p, plant_density, "
          "root_growth_lag, min_temperature_root_growth, initial_rooting_depth, "
          "root_penetration_rate, root_form_factor, specific_root_length, "
          "stage_after_cut, crit_temperature_heat_stress, "
          "lim_temperature_heat_stress, begin_sensitive_phase_heat_stress, "
          "end_sensitive_phase_heat_stress, drought_impact_on_fertility_factor, "
          "cutting_delay_days, field_condition_modifier, assimilate_reallocation, "
					"LT50cultivar, frost_hardening, frost_dehardening, "
					"low_temperature_exposure, respiratory_stress, latest_harvest_doy from crop";
      con->select(text_request.c_str());

      debug () << text_request.c_str() << endl;
      while (!(row = con->getRow()).empty())
      {

        int i = 0;
        int id = satoi(row[i++]);

        debug() << "Reading in crop Parameters for: " << id << endl;
        CPS::iterator cpsi = cpss.find(id);
        CPPtr cps;

        if (cpsi == cpss.end())
          cpss.insert(make_pair(id, cps = std::shared_ptr<CropParameters>(new CropParameters)));
        else
          cps = cpsi->second;

        cps->pc_CropName = row[i++].c_str();
        cps->pc_Perennial = satob(row[i++]);
        cps->pc_MaxAssimilationRate = satof(row[i++]);
        cps->pc_CarboxylationPathway = satoi(row[i++]);
        cps->pc_MinimumTemperatureForAssimilation = satof(row[i++]);
        cps->pc_CropSpecificMaxRootingDepth = satof(row[i++]);
        cps->pc_MinimumNConcentration = satof(row[i++]);
        cps->pc_NConcentrationPN = satof(row[i++]);
        cps->pc_NConcentrationB0 = satof(row[i++]);
        cps->pc_NConcentrationAbovegroundBiomass = satof(row[i++]);
        cps->pc_NConcentrationRoot = satof(row[i++]);
        cps->pc_InitialKcFactor = satof(row[i++]);
        cps->pc_DevelopmentAccelerationByNitrogenStress = satoi(row[i++]);
        cps->pc_PartBiologicalNFixation = satof(row[i++]);
        cps->pc_LuxuryNCoeff = satof(row[i++]);
        cps->pc_MaxCropHeight = satof(row[i++]);
        cps->pc_ResidueNRatio = satof(row[i++]);
        cps->pc_SamplingDepth = satof(row[i++]);
        cps->pc_TargetNSamplingDepth = satof(row[i++]);
        cps->pc_TargetN30 = satof(row[i++]);
        cps->pc_DefaultRadiationUseEfficiency = satof(row[i++]);
        cps->pc_CropHeightP1 = satof(row[i++]);
        cps->pc_CropHeightP2 = satof(row[i++]);
        cps->pc_StageAtMaxHeight = satof(row[i++]);
        cps->pc_MaxCropDiameter = satof(row[i++]);
        cps->pc_StageAtMaxDiameter = satof(row[i++]);
        cps->pc_HeatSumIrrigationStart = satof(row[i++]);
        cps->pc_HeatSumIrrigationEnd = satof(row[i++]);
        cps->pc_MaxNUptakeParam = satof(row[i++]);
        cps->pc_RootDistributionParam = satof(row[i++]);
        cps->pc_PlantDensity = satof(row[i++]);
        cps->pc_RootGrowthLag = satof(row[i++]);
        cps->pc_MinimumTemperatureRootGrowth = satof(row[i++]);
        cps->pc_InitialRootingDepth = satof(row[i++]);
        cps->pc_RootPenetrationRate = satof(row[i++]);
        cps->pc_RootFormFactor = satof(row[i++]);
        cps->pc_SpecificRootLength = satof(row[i++]);
        cps->pc_StageAfterCut = satoi(row[i++]);
        cps->pc_CriticalTemperatureHeatStress = satof(row[i++]);
        cps->pc_LimitingTemperatureHeatStress = satof(row[i++]);
        cps->pc_BeginSensitivePhaseHeatStress = satof(row[i++]);
        cps->pc_EndSensitivePhaseHeatStress = satof(row[i++]);
        cps->pc_DroughtImpactOnFertilityFactor = satof(row[i++]);
        cps->pc_CuttingDelayDays = satoi(row[i++]);
        cps->pc_FieldConditionModifier = satof(row[i++]);
        cps->pc_AssimilateReallocation = satof(row[i++]);
        cps->pc_LT50cultivar = satof(row[i++]);
        cps->pc_FrostHardening = satof(row[i++]);
        cps->pc_FrostDehardening = satof(row[i++]);
        cps->pc_LowTemperatureExposure = satof(row[i++]);
        cps->pc_RespiratoryStress = satof(row[i++]);
        cps->pc_LatestHarvestDoy = satoi(row[i++]);

      }
      std::string req2 ="select o.crop_id, o.id, o.initial_organ_biomass, "
                        "o.organ_maintainance_respiration, o.is_above_ground, "
                        "o.organ_growth_respiration, o.is_storage_organ "
                        "from organ as o inner join crop as c on c.id = o.crop_id "
                        "order by o.crop_id, c.id";
      con->select(req2.c_str());

      debug() << req2.c_str() << endl;

      while (!(row = con->getRow()).empty())
      {

        int cropId = satoi(row[0]);
//        debug() << "Organ for crop: " << cropId << endl;
        auto cps = cpss[cropId];

        cps->pc_NumberOfOrgans++;
        cps->pc_InitialOrganBiomass.push_back(satof(row[2]));
        cps->pc_OrganMaintenanceRespiration.push_back(satof(row[3]));
        cps->pc_AbovegroundOrgan.push_back(satoi(row[4]) == 1);
        cps->pc_OrganGrowthRespiration.push_back(satof(row[5]));
        cps->pc_StorageOrgan.push_back(satoi(row[6]));
      }

      std::string req4 = "select crop_id, id, stage_temperature_sum, "
          "base_temperature, opt_temperature, vernalisation_requirement, "
          "day_length_requirement, base_day_length, "
          "drought_stress_threshold, critical_oxygen_content, "
          "specific_leaf_area, stage_max_root_n_content, "
          "stage_kc_factor "
          "from dev_stage "
          "order by crop_id, id";

      con->select(req4.c_str());
      debug() << req4.c_str() << endl;
      while (!(row = con->getRow()).empty())
      {
        int cropId = satoi(row[0]);
        auto cps = cpss[cropId];
        cps->pc_NumberOfDevelopmentalStages++;
        cps->pc_StageTemperatureSum.push_back(satof(row[2]));
        cps->pc_BaseTemperature.push_back(satof(row[3]));
        cps->pc_OptimumTemperature.push_back(satof(row[4]));
        cps->pc_VernalisationRequirement.push_back(satof(row[5]));
        cps->pc_DaylengthRequirement.push_back(satof(row[6]));
        cps->pc_BaseDaylength.push_back(satof(row[7]));
        cps->pc_DroughtStressThreshold.push_back(satof(row[8]));
        cps->pc_CriticalOxygenContent.push_back(satof(row[9]));
        cps->pc_SpecificLeafArea.push_back(satof(row[10]));
        cps->pc_StageMaxRootNConcentration.push_back(satof(row[11]));
        cps->pc_StageKcFactor.push_back(satof(row[12]));
      }

      for(CPS::value_type vt : cpss)
      {
        vt.second->resizeStageOrganVectors();
      }
      //for (CPS::iterator it = cpss.begin(); it != cpss.end(); it++)
      //  it->second->resizeStageOrganVectors();

      std::string req3 = "select crop_id, organ_id, dev_stage_id, "
                        "ods_dependent_param_id, value "
                        "from crop2ods_dependent_param "
                        "order by crop_id, ods_dependent_param_id, dev_stage_id, organ_id";
      con->select(req3.c_str());
      debug() << req3.c_str() << endl;

      while (!(row = con->getRow()).empty())
      {

        int cropId = satoi(row[0]);
//        debug() << "ods_dependent_param " << cropId << "\t" << satoi(row[3]) <<"\t" << satoi(row[2]) <<"\t" << satoi(row[1])<< endl;
        auto cps = cpss[cropId];
        vector<vector<double> >& sov = satoi(row[3]) == 1
                                       ? cps->pc_AssimilatePartitioningCoeff
                                         : cps->pc_OrganSenescenceRate;
        sov[satoi(row[2]) - 1][satoi(row[1]) - 1] = satof(row[4]);
      }

      con->select("SELECT crop_id, organ_id, is_primary, percentage, dry_matter FROM yield_parts");
      debug() << "SELECT crop_id, organ_id, is_primary, percentage, dry_matter FROM yield_parts" << endl;
      while (!(row = con->getRow()).empty())
      {
        int cropId = satoi(row[0]);
        int organId = satoi(row[1]);
        bool isPrimary = satoi(row[2]) == 1;
        double percentage = satof(row[3]) / 100.0;
        double yieldDryMatter = satof(row[4]);

        auto cps = cpss[cropId];

         // normal case, uses yield partitioning from crop database
         if (isPrimary) {
//            cout << cropId<< " Add primary organ: " << organId << endl;
            cps->pc_OrganIdsForPrimaryYield.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));

         } else {
//            cout << cropId << " Add secondary organ: " << organId << endl;
            cps->pc_OrganIdsForSecondaryYield.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));
         }

      }

      // get cutting parts if there are some data available
      con->select("SELECT crop_id, organ_id, is_primary, percentage, dry_matter FROM cutting_parts");
      while (!(row = con->getRow()).empty())
      {
        int cropId = satoi(row[0]);
        int organId = satoi(row[1]);
        //bool isPrimary = satoi(row[2]) == 1;
        double percentage = satof(row[3]) / 100.0;
        double yieldDryMatter = satof(row[4]);

        auto cps = cpss[cropId];
        cps->pc_OrganIdsForCutting.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));
        //if (cropId!=18) {
        //    // do not add cutting part organ id for sudan gras because they are already added
        //    cps->pc_OrganIdsForPrimaryYield.push_back(Monica::YieldComponent(organId, percentage, yieldDryMatter));
        //}
      }


      delete con;
      initialized = true;

      /*
       for(CPS::const_iterator it = cpss.begin(); it != cpss.end(); it++)
       cout << it->second->toString();
       //*/
    }
  }

  static CropParameters nothing;

  CPS::const_iterator ci = cpss.find(cropId);

  debug() << "Find crop parameter: " << cropId << endl;
  return ci != cpss.end() ? ci->second.get() : &nothing;
}

//------------------------------------------------------------------------------

/**
 * @brief Constructor
 * @param ps_LayerThickness
 * @param ps_ProfileDepth
 * @param ps_MaxMineralisationDepth
 * @param ps_NitrogenResponseOn
 * @param ps_WaterDeficitResponseOn
 */
//GeneralParameters::GeneralParameters(double ps_LayerThickness,
//                                     double ps_ProfileDepth,
//									 double ps_MaximumMineralisationDepth,
//									 bool pc_NitrogenResponseOn,
//					                 bool pc_WaterDeficitResponseOn,
//									 bool pc_EmergenceFloodingControlOn,
//									 bool pc_EmergenceMoistureControlOn) :
//ps_LayerThickness(int(ps_ProfileDepth / ps_LayerThickness), ps_LayerThickness),
//ps_ProfileDepth(ps_ProfileDepth),
//ps_MaxMineralisationDepth(ps_MaximumMineralisationDepth),
//pc_NitrogenResponseOn(pc_NitrogenResponseOn),
//pc_WaterDeficitResponseOn(pc_WaterDeficitResponseOn),
//pc_EmergenceFloodingControlOn(pc_EmergenceFloodingControlOn),
//pc_EmergenceMoistureControlOn(pc_EmergenceMoistureControlOn)
//{}

GeneralParameters::GeneralParameters(double layerThickness)
  : ps_LayerThickness(int(ps_ProfileDepth / layerThickness), layerThickness)
{}

GeneralParameters::GeneralParameters(json11::Json j)
  : ps_ProfileDepth(j["ProfileDepth"].number_value()),
    ps_MaxMineralisationDepth(j["MaxMineralisationDepth"].number_value()),
    pc_NitrogenResponseOn(j["NitrogenResponseOn"].bool_value()),
    pc_WaterDeficitResponseOn(j["WaterDeficitResponseOn"].bool_value()),
    pc_EmergenceFloodingControlOn(j["EmergenceFloodingControlOn"].bool_value()),
    pc_EmergenceMoistureControlOn(j["EmergenceMoistureControlOn"].bool_value()),
    useNMinMineralFertilisingMethod(j["useNMinMineralFertilisingMethod"].bool_value()),
    nMinFertiliserPartition(j["nMinFertiliserPartition"]),
    nMinUserParams(j["nMinUserParams"]),
    atmosphericCO2(j["atmosphericCO2"].number_value()),
    useAutomaticIrrigation(j["useAutomaticIrrigation"].bool_value()),
    autoIrrigationParams(j["autoIrrigationParams"]),
    useSecondaryYields(j["useSecondaryYields"].bool_value()),
    windSpeedHeight(j["windSpeedHeight"].number_value()),
    albedo(j["albedo"].number_value()),
    groundwaterInformation(j["groundwaterInformation"]),
    pathToOutputDir(j["pathToOutputDir"].string_value())
{
  auto lts = j["LayerThickness"].array_items();
  for(auto lt : lts)
    ps_LayerThickness.push_back(lt.number_value());
}

json11::Json GeneralParameters::to_json() const
{
  json11::Json::array lts;
  for(auto lt : ps_LayerThickness)
    lts.push_back(lt);
  return json11::Json::object {
    {"type", "GeneralParameters"},
    {"ProfileDepth", ps_ProfileDepth},
    {"MaxMineralisationDepth", ps_MaxMineralisationDepth},
    {"NitrogenResponseOn", pc_NitrogenResponseOn},
    {"WaterDeficitResponseOn", pc_WaterDeficitResponseOn},
    {"EmergenceFloodingControlOn", pc_EmergenceFloodingControlOn},
    {"EmergenceMoistureControlOn", pc_EmergenceMoistureControlOn},
    {"LayerThickness", lts},
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
    vs_GroundwaterDepth(j["_GroundwaterDepth"].number_value()),
    vs_Soil_CN_Ratio(j["Soil_CN_Ratio"].number_value()),
    vs_DrainageCoeff(j["vs_DrainageCoeff"].number_value()),
    vq_NDeposition(j["vq_NDeposition"].number_value()),
    vs_MaxEffectiveRootingDepth(j["vs_MaxEffectiveRootingDepth"].number_value())
{}

json11::Json SiteParameters::to_json() const
{
  return json11::Json::object {
    {"type", "SiteParameters"},
    {"Latitude", vs_Latitude},
    {"Slope", vs_Slope},
    {"HeightNN", vs_HeightNN},
    {"GroundwaterDepth", vs_GroundwaterDepth},
    {"Soil_CN_Ratio", vs_Soil_CN_Ratio},
    {"DrainageCoeff", vs_DrainageCoeff},
    {"NDeposition", vq_NDeposition},
    {"MaxEffectiveRootingDepth", vs_MaxEffectiveRootingDepth}};
}

/**
 * @brief Serializes site parameters into a string.
 * @return String containing size parameters
 */
string SiteParameters::toString() const
{
  ostringstream s;
  s << "vs_Latitude: " << vs_Latitude << " vs_Slope: " << vs_Slope << " vs_HeightNN: " << vs_HeightNN
      << " vs_DepthGroundwaterTable: " << vs_GroundwaterDepth << " vs_Soil_CN_Ratio: " << vs_Soil_CN_Ratio
      << "vs_DrainageCoeff: " << vs_DrainageCoeff << " vq_NDeposition: " << vq_NDeposition
      << " vs_MaxEffectiveRootingDepth: " << vs_MaxEffectiveRootingDepth
      << endl;
  return s.str();
}

//------------------------------------------------------------------------------

Crop::Crop(const std::string& name)
  : _name(name)
{}

Crop::Crop(CropId id,
           const std::string& name,
           const CropParameters* cps,
           const OrganicMatterParameters* rps,
           double crossCropAdaptionFactor)
  : _id(id),
    _name(name),
    _cropParams(cps),
    _residueParams(rps),
    _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(CropId id,
           const std::string& name,
           const Tools::Date& seedDate,
           const Tools::Date& harvestDate,
           const CropParameters* cps,
           const OrganicMatterParameters* rps,
           double crossCropAdaptionFactor)
  : _id(id),
    _name(name),
    _seedDate(seedDate),
    _harvestDate(harvestDate),
    _cropParams(cps),
    _residueParams(rps),
    _crossCropAdaptionFactor(crossCropAdaptionFactor)
{}

Crop::Crop(json11::Json j)
  : _id(j["id"].int_value()),
    _name(j["name"].string_value()),
    _seedDate(Tools::Date::fromIsoDateString(j["seedDate"].string_value())),
    _harvestDate(Tools::Date::fromIsoDateString(j["havestDate"].string_value()))
{
  if(_id > -1)
  {
    _cropParams = getCropParametersFromMonicaDB(_id);
    _residueParams = getResidueParametersFromMonicaDB(_id);
  }

  auto cds = j["cuttingDates"].array_items();
  for(auto cd : cds)
   _cuttingDates.push_back(Tools::Date::fromIsoDateString(cd.string_value()));
}

json11::Json Crop::to_json() const
{
  json11::Json::array cds;
  for(auto cd : _cuttingDates)
    cds.push_back(cd.toIsoDateString());
  return json11::Json::object {
    {"type", "Crop"},
    {"id", _id},
    {"name", _name},
    {"seedDate", _seedDate.toIsoDateString()},
    {"harvestDate", _harvestDate.toIsoDateString()},
    {"cuttingDates", cds},
    //        {"automaticHarvest", _automaticHarvest}
  };
}

//		Crop::Crop(const Crop& new_crop)
//		{
//			_id = new_crop._id;
//			_name  = new_crop._name;
//			_seedDate = new_crop._seedDate;
//			_harvestDate = new_crop._harvestDate;
//			_cropParams = new_crop._cropParams;
//			_perennialCropParams = new_crop._perennialCropParams;
//			_residueParams = new_crop._residueParams;
//			_primaryYield = new_crop._primaryYield;
//			_secondaryYield = new_crop._secondaryYield;
//			_primaryYieldTM = new_crop._primaryYieldTM;
//			_secondaryYieldTM = new_crop._secondaryYieldTM;
//			_primaryYieldN = new_crop._primaryYieldN;
//			_secondaryYieldN = new_crop._secondaryYieldN;
//			_sumTotalNUptake = new_crop._sumTotalNUptake;
//			_appliedAmountIrrigation = new_crop._appliedAmountIrrigation;
//			_crossCropAdaptionFactor = new_crop._crossCropAdaptionFactor;
//			_cropHeight = new_crop._cropHeight;
//			eva2_typeUsage = new_crop.eva2_typeUsage;
//			_anthesisDay = new_crop._anthesisDay;
//			_maturityDay = new_crop._maturityDay;
//			_automaticHarvest = new_crop._automaticHarvest;
//			_automaticHarvestParams = new_crop._automaticHarvestParams;
//		}

string Crop::toString(bool detailed) const
{
  ostringstream s;
  s << "id: " << id() << " name: " << name() << " seedDate: " << seedDate().toString() << " harvestDate: "
      << harvestDate().toString();
  if (detailed)
  {
    s << endl << "CropParameters: " << endl << cropParameters()->toString() << endl << "ResidueParameters: " << endl
        << residueParameters()->toString() << endl;
  }
  return s.str();
}

void Crop::writeCropParameters(std::string path)
{
    ofstream parameter_output_file;
    parameter_output_file.open((path + "/crop_parameters-" + _name.c_str() + ".txt").c_str());
    if (parameter_output_file.fail()){
        debug() << "Could not write file\"" << (path + "crop_parameters-" + _name.c_str() + ".txt").c_str() << "\"" << endl;
        return;
  }

  parameter_output_file << _cropParams->toString().c_str();

  parameter_output_file.close();

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
    {"name", name},
    {"Carbamid", vo_Carbamid},
    {"NH4", vo_NH4},
    {"NO3", vo_NO3}};
}

/**
 * @brief Serializes all object information into a string.
 * @return String with parameter information.
 */
string MineralFertiliserParameters::toString() const
{
  ostringstream s;
  s << "name: " << name << " carbamid: " << vo_Carbamid
      << " NH4: " << vo_NH4 << " NO3: " << vo_NO3;
  return s.str();
}

/**
 * @brief Reads mineral fertiliser parameters from monica DB
 * @param id of the fertiliser
 * @return mineral fertiliser parameters value object with values from database
 */
MineralFertiliserParameters
    Monica::getMineralFertiliserParametersFromMonicaDB(int id)
{
  static mutex lockable;
  static bool initialized = false;
  static map<int, MineralFertiliserParameters> m;

  if (!initialized)
  {
    lock_guard<mutex> lock(lockable);

    if (!initialized)
    {
      DB *con = newConnection("monica");
      DBRow row;
      con->select("select id, name, no3, nh4, carbamid from mineral_fertilisers");
      while (!(row = con->getRow()).empty())
      {
        int id = satoi(row[0]);
        string name = row[1];
        double no3 = satof(row[2]);
        double nh4 = satof(row[3]);
        double carbamid = satof(row[4]);

        m.insert(make_pair(id, MineralFertiliserParameters(name, carbamid, no3, nh4)));
      }
      delete con;

      initialized = true;
    }
  }

  map<int, MineralFertiliserParameters>::const_iterator ci = m.find(id);

  return ci != m.end() ? ci->second : MineralFertiliserParameters();
}


std::vector<ProductionProcess>
Monica::attachFertiliserSA(std::vector<ProductionProcess> cropRotation, const std::string pathToFertiliserFile)
{
  attachFertiliserApplicationsToCropRotation(cropRotation, pathToFertiliserFile);
  return cropRotation;
}

void
Monica::attachFertiliserApplicationsToCropRotation(std::vector<ProductionProcess>& cr,
					      const std::string& pathToFile)
{
  ifstream ifs(pathToFile.c_str(), ios::binary);
  string s;

  std::vector<ProductionProcess>::iterator it = cr.begin();
  if (it == cr.end())
    return;

  //skip first line
  getline(ifs, s);

  Date currentEnd = it->end();
  while (getline(ifs, s))
  {
    if (trim(s) == "end")
      break;

    //Schlag_ID  N  FRT   Date
    double sid;
    double n;
    string frt;
    string sfdate;
    bool incorp;
    istringstream ss(s);
    ss >> sid >> n >> frt >> sfdate >> incorp;

    //get data parsed and to use leap years if the crop rotation uses them
    Date fdate = parseDate(sfdate).toDate(it->crop()->seedDate().useLeapYears());

    if (!fdate.isValid())
    {
      debug() << "Error - Invalid date in \"" << pathToFile.c_str() << "\"" << endl;
      debug() << "Line: " << s.c_str() << endl;
      debug() << "Aborting simulation now!" << endl;
      exit(-1);
    }

    //cout << "PP start: " << it->start().toString()
    //<< " PP end: " << it->end().toString() << endl;
    //cout << "fdate: " << fdate.toString()
    //<< " currentEnd: " << currentEnd.toString() << endl;

    //if the currently read fertiliser date is after the current end
    //of the crop, move as long through the crop rotation as
    //we find an end date that lies after the currently read fertiliser date
    while (fdate > currentEnd)
    {
      //move to next crop and possibly exit at the end
      it++;
      if (it == cr.end())
        break;

      currentEnd = it->end();

      //cout << "new PP start: " << it->start().toString()
      //<< " new PP end: " << it->end().toString() << endl;
      //cout << "new currentEnd: " << currentEnd.toString() << endl;

    }

    //which type and id is the current fertiliser
    pair<FertiliserType, int> fertTypeAndId =
        hermesFertiliserName2monicaFertiliserId(frt);
    switch (fertTypeAndId.first)
    {
    case mineral:
      {
        //create mineral fertiliser application
        const MineralFertiliserParameters& mfp = getMineralFertiliserParametersFromMonicaDB(fertTypeAndId.second);
        it->addApplication(MineralFertiliserApplication(fdate, mfp, n));
        break;
      }
    case organic:
      {
        //create organic fertiliser application
        OrganicMatterParameters* omp = getOrganicFertiliserParametersFromMonicaDB(fertTypeAndId.second);
		//omp->vo_NConcentration = 100.0;
        it->addApplication(OrganicFertiliserApplication(fdate, omp, n, incorp));
        break;
      }
    case undefined:
      {
        break;
      }
    }

    //cout << "----------------------------------" << endl;
  }

}

//------------------------------------------------------------------------------

void
Monica::attachIrrigationApplicationsToCropRotation(std::vector<ProductionProcess>& cr,
                                               const std::string& pathToFile)
{
  ifstream ifs(pathToFile.c_str(), ios::in);
  if (!ifs.is_open()) {
      return;
  }
  string s;

  std::vector<ProductionProcess>::iterator it = cr.begin();
  if (it == cr.end())
    return;

  //skip first line
  getline(ifs, s);

  Date currentEnd = it->end();
  while (getline(ifs, s))
  {
    if (trim(s) == "end")
      break;

    //Field_ID  mm SCc IrrDat NCc
    double fid;
    int mm;
    double scc; //sulfate concentration [mg dm-3]
    string irrDate;
    double ncc; //nitrate concentration [mg dm-3]
    istringstream ss(s);
    ss >> fid >> mm >> scc >> irrDate >> ncc;

    //get data parsed and to use leap years if the crop rotation uses them
    Date idate = parseDate(irrDate).toDate(it->crop()->seedDate().useLeapYears());
    if (!idate.isValid())
    {
      debug() << "Error - Invalid date in \"" << pathToFile.c_str() << "\"" << endl;
      debug() << "Line: " << s.c_str() << endl;
      debug() << "Aborting simulation now!" << endl;
      exit(-1);
    }

    //cout << "PP start: " << it->start().toString()
    //<< " PP end: " << it->end().toString() << endl;
    //cout << "irrigationDate: " << idate.toString()
    //<< " currentEnd: " << currentEnd.toString() << endl;

    //if the currently read irrigation date is after the current end
    //of the crop, move as long through the crop rotation as
    //we find an end date that lies after the currently read irrigation date
    while (idate > currentEnd)
    {
      //move to next crop and possibly exit at the end
      it++;
      if (it == cr.end())
        break;

      currentEnd = it->end();

      //cout << "new PP start: " << it->start().toString()
      //<< " new PP end: " << it->end().toString() << endl;
      //cout << "new currentEnd: " << currentEnd.toString() << endl;
    }

    //finally add the application to the current crops list
    it->addApplication(IrrigationApplication(idate, mm, IrrigationParameters(ncc, scc)));

    //cout << "----------------------------------" << endl;
  }
}

//------------------------------------------------------------------------------

/**
 * @brief Default constructor
 */
OrganicMatterParameters::OrganicMatterParameters() :
    name(""),
    vo_AOM_DryMatterContent(0.0),
    vo_AOM_NH4Content(0.0),
    vo_AOM_NO3Content(0.0),
    vo_AOM_CarbamidContent(0.0),
    vo_AOM_SlowDecCoeffStandard(0.0),
    vo_AOM_FastDecCoeffStandard(0.0),
    vo_PartAOM_to_AOM_Slow(0.0),
    vo_PartAOM_to_AOM_Fast(0.0),
    vo_CN_Ratio_AOM_Slow(0.0),
    vo_CN_Ratio_AOM_Fast(0.0),
    vo_PartAOM_Slow_to_SMB_Slow(0.0),
    vo_PartAOM_Slow_to_SMB_Fast(0.0),
    vo_NConcentration(0.0)
{}

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


OrganicMatterParameters::OrganicMatterParameters(const OrganicMatterParameters& omp)
{
  this->vo_AOM_DryMatterContent = omp.vo_AOM_DryMatterContent;
  this->vo_AOM_NH4Content = omp.vo_AOM_NH4Content;
  this->vo_AOM_NO3Content = omp.vo_AOM_NO3Content;
  this->vo_AOM_CarbamidContent = omp.vo_AOM_CarbamidContent;

  this->vo_AOM_SlowDecCoeffStandard = omp.vo_AOM_SlowDecCoeffStandard;
  this->vo_AOM_FastDecCoeffStandard = omp.vo_AOM_FastDecCoeffStandard;

  this->vo_PartAOM_to_AOM_Slow = omp.vo_PartAOM_to_AOM_Slow;
  this->vo_PartAOM_to_AOM_Fast = omp.vo_PartAOM_to_AOM_Fast;

  this->vo_CN_Ratio_AOM_Slow = omp.vo_CN_Ratio_AOM_Slow;
  this->vo_CN_Ratio_AOM_Fast = omp.vo_CN_Ratio_AOM_Fast;

  this->vo_PartAOM_Slow_to_SMB_Slow = omp.vo_PartAOM_Slow_to_SMB_Slow;
  this->vo_PartAOM_Slow_to_SMB_Fast = omp.vo_PartAOM_Slow_to_SMB_Fast;

  this->vo_NConcentration = omp.vo_NConcentration;

}

/**
 * @brief Serializes all object information into a string.
 * @return String with parameter information.
 */
string OrganicMatterParameters::toString() const
{
  ostringstream s;
  s << "Name: " << name << endl
      << "vo_NConcentration: " << vo_NConcentration << endl
      << "vo_DryMatter: " << vo_AOM_DryMatterContent << endl
      << "vo_NH4: " << vo_AOM_NH4Content << endl
      << "vo_NO3: " << vo_AOM_NO3Content << endl
      << "vo_NH2: " << vo_AOM_CarbamidContent << endl
      << "vo_kSlow: " << vo_AOM_SlowDecCoeffStandard << endl
      << "vo_kFast: " << vo_AOM_FastDecCoeffStandard << endl
      << "vo_PartSlow: " << vo_PartAOM_to_AOM_Slow << endl
      << "vo_PartFast: " << vo_PartAOM_to_AOM_Fast << endl
      << "vo_CNSlow: " << vo_CN_Ratio_AOM_Slow << endl
      << "vo_CNFast: " << vo_CN_Ratio_AOM_Fast << endl
      << "vo_SMBSlow: " << vo_PartAOM_Slow_to_SMB_Slow << endl
      << "vo_SMBFast: " << vo_PartAOM_Slow_to_SMB_Fast << endl;
  return s.str();
}

/**
 * @brief Reads organic fertiliser parameters from monica DB
 * @param organ_fert_id ID of fertiliser
 * @return organic fertiliser parameters with values from database
 */
OrganicMatterParameters*
Monica::getOrganicFertiliserParametersFromMonicaDB(int id)
{
  static mutex lockable;
  static bool initialized = false;
  typedef map<int, OMPPtr> Map;
  static Map m;

  if (!initialized)
  {
    lock_guard<mutex> lock(lockable);

    if (!initialized)
    {
      DB *con = newConnection("monica");
      DBRow row;
      con->select("select om_Type, dm, nh4_n, no3_n, nh2_n, k_slow, k_fast, part_s, "
                 "part_f, cn_s, cn_f, smb_s, smb_f, id "
                 "from organic_fertiliser");
      while (!(row = con->getRow()).empty())
      {
        auto omp = OMPPtr(new OMP);
        omp->name = row[0];
        omp->vo_AOM_DryMatterContent = satof(row[1]);
        omp->vo_AOM_NH4Content = satof(row[2]);
        omp->vo_AOM_NO3Content = satof(row[3]);
        omp->vo_AOM_CarbamidContent = satof(row[4]);
        omp->vo_AOM_SlowDecCoeffStandard = satof(row[5]);
        omp->vo_AOM_FastDecCoeffStandard = satof(row[6]);
        omp->vo_PartAOM_to_AOM_Slow = satof(row[7]);
        omp->vo_PartAOM_to_AOM_Fast = satof(row[8]);
        omp->vo_CN_Ratio_AOM_Slow = satof(row[9]);
        omp->vo_CN_Ratio_AOM_Fast = satof(row[10]);
        omp->vo_PartAOM_Slow_to_SMB_Slow = satof(row[11]);
        omp->vo_PartAOM_Slow_to_SMB_Fast = satof(row[12]);
        int id = satoi(row[13]);

        m.insert(make_pair(id, omp));
      }
      delete con;

      initialized = true;
    }
  }

  static OrganicMatterParameters nothing;

  Map::const_iterator ci = m.find(id);
  return ci != m.end() ? ci->second.get() : &nothing;
}

/*
 ResidueParameters::ResidueParameters()
 : vo_AOM_DryMatterContent(0.289),
 vo_AOM_NH4Content(0.007),
 vo_AOM_NO3Content(0.0),
 vo_AOM_CarbamidContent(0.0),
 vo_AOM_SlowDecCoeffStandard(2.0e-4),
 vo_AOM_FastDecCoeffStandard(2.0e-3),
 vo_PartAOM_to_AOM_Slow(0.72),
 vo_PartAOM_to_AOM_Fast(0.18),
 vo_CN_Ratio_AOM_Slow(100.0),
 vo_CN_Ratio_AOM_Fast(7.3),
 vo_PartAOM_Slow_to_SMB_Slow(0.0),
 vo_PartAOM_Slow_to_SMB_Fast(1.0) { }
 */

/**
 * @brief Reads residue parameters from monica DB
 * @param crop_id ID of crop
 * @return Residues parameters with values from database
 */
const OrganicMatterParameters*
    Monica::getResidueParametersFromMonicaDB(int cropId)
{
  static mutex lockable;
  static bool initialized = false;
  typedef map<int, OMPPtr> Map;
  static Map m;

  if (!initialized)
  {
    lock_guard<mutex> lock(lockable);

    if (!initialized)
    {
      DB *con = newConnection("monica");
      DBRow row;
      con->select("select residue_type, dm, nh4, no3, nh2, k_slow, k_fast, part_s, "
                 "part_f, cn_s, cn_f, smb_s, smb_f, crop_id "
                 "from residue_table");
      while (!(row = con->getRow()).empty())
      {
        auto omp = OMPPtr(new OMP);
        omp->name = row[0];
        omp->vo_AOM_DryMatterContent = satoi(row[1]);
        omp->vo_AOM_NH4Content = satof(row[2]);
        omp->vo_AOM_NO3Content = satof(row[3]);
        omp->vo_AOM_CarbamidContent = satof(row[4]);
        omp->vo_AOM_SlowDecCoeffStandard = satof(row[5]);
        omp->vo_AOM_FastDecCoeffStandard = satof(row[6]);
        omp->vo_PartAOM_to_AOM_Slow = satof(row[7]);
        omp->vo_PartAOM_to_AOM_Fast = satof(row[8]);
        omp->vo_CN_Ratio_AOM_Slow = satof(row[9]);
        omp->vo_CN_Ratio_AOM_Fast = satof(row[10]);
        omp->vo_PartAOM_Slow_to_SMB_Slow = satof(row[11]);
        omp->vo_PartAOM_Slow_to_SMB_Fast = satof(row[12]);
        int id = satoi(row[13]);

        m.insert(make_pair(id, omp));
      }
      delete con;

      initialized = true;
    }
  }

  static OrganicMatterParameters nothing;

  Map::const_iterator ci = m.find(cropId);
  return ci != m.end() ? ci->second.get() : &nothing;
}

//------------------------------------------------------------------------------

/**
 * @brief Constructor
 */
CentralParameterProvider::CentralParameterProvider()
{
  for (int i=0; i<MONTH; i++) {
    precipCorrectionValues[i] = 1.0;
  }
  writeOutputFiles = false;

}

/**
 * Copy constructor
 * @param
 * @return
 */
CentralParameterProvider::
    CentralParameterProvider(const CentralParameterProvider& cpp)
{

  userCropParameters = cpp.userCropParameters;
  userEnvironmentParameters = cpp.userEnvironmentParameters;
  userSoilMoistureParameters = cpp.userSoilMoistureParameters;
  userSoilTemperatureParameters = cpp.userSoilTemperatureParameters;
  userSoilTransportParameters = cpp.userSoilTransportParameters;
	userSoilOrganicParameters = cpp.userSoilOrganicParameters;
	sensitivityAnalysisParameters = cpp.sensitivityAnalysisParameters;
  capillaryRiseRates = cpp.capillaryRiseRates;
	userInitValues = cpp.userInitValues;

  for (int i=0; i<12; i++)
    precipCorrectionValues[i] = cpp.precipCorrectionValues[i];
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

  if (month<12) {
    return precipCorrectionValues[month];
  }
  cerr << "Requested correction value for precipitation for an invalid month.\nMust be in range of 0<=value<12." << endl;
  return 1.0;
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

CentralParameterProvider Monica::readUserParameterFromDatabase(int type)
{
  static mutex lockable;

  static bool initialized = false;

  static CentralParameterProvider centralParameterProvider;

  if (!initialized)
  {
    lock_guard<mutex> lock(lockable);

    if (!initialized)
    {
      debug() << "DB Conncection user parameters" << endl;
      DB *con = newConnection("monica");
      DBRow row;
      switch (type)
      {
      case Env::MODE_HERMES:
        con->select("select name, value_hermes from user_parameter");
        break;
      case Env::MODE_EVA2:
        con->select("select name, value_eva2 from user_parameter");
        break;
      case Env::MODE_MACSUR_SCALING:
        con->select("select name, value_macsur_scaling from user_parameter");
        break;
      default:
        con->select("select name, value_hermes from user_parameter");
        break;
      }

      UserCropParameters& user_crops =
          centralParameterProvider.userCropParameters;
      UserEnvironmentParameters& user_env =
          centralParameterProvider.userEnvironmentParameters;
      UserSoilMoistureParameters& user_soil_moisture =
          centralParameterProvider.userSoilMoistureParameters;
      UserSoilTemperatureParameters& user_soil_temperature =
          centralParameterProvider.userSoilTemperatureParameters;
      UserSoilTransportParameters& user_soil_transport =
          centralParameterProvider.userSoilTransportParameters;
      UserSoilOrganicParameters& user_soil_organic =
          centralParameterProvider.userSoilOrganicParameters;
      UserInitialValues& user_init_values =
          centralParameterProvider.userInitValues;

      while (!(row = con->getRow()).empty())
      {
        std::string name = row[0];
        if (name == "tortuosity")
          user_crops.pc_Tortuosity = satof(row[1]);
        else if (name == "canopy_reflection_coefficient")
          user_crops.pc_CanopyReflectionCoefficient = satof(row[1]);
        else if (name == "reference_max_assimilation_rate")
          user_crops.pc_ReferenceMaxAssimilationRate = satof(row[1]);
        else if (name == "reference_leaf_area_index")
          user_crops.pc_ReferenceLeafAreaIndex = satof(row[1]);
        else if (name == "maintenance_respiration_parameter_2")
          user_crops.pc_MaintenanceRespirationParameter2 = satof(row[1]);
        else if (name == "maintenance_respiration_parameter_1")
          user_crops.pc_MaintenanceRespirationParameter1 = satof(row[1]);
        else if (name == "minimum_n_concentration_root")
          user_crops.pc_MinimumNConcentrationRoot = satof(row[1]);
        else if (name == "minimum_available_n")
          user_crops.pc_MinimumAvailableN = satof(row[1]);
        else if (name == "reference_albedo")
          user_crops.pc_ReferenceAlbedo = satof(row[1]);
        else if (name == "stomata_conductance_alpha")
          user_crops.pc_StomataConductanceAlpha = satof(row[1]);
        else if (name == "saturation_beta")
          user_crops.pc_SaturationBeta = satof(row[1]);
        else if (name == "growth_respiration_redux")
          user_crops.pc_GrowthRespirationRedux = satof(row[1]);
        else if (name == "max_crop_n_demand")
          user_crops.pc_MaxCropNDemand = satof(row[1]);
        else if (name == "growth_respiration_parameter_2")
          user_crops.pc_GrowthRespirationParameter2 = satof(row[1]);
        else if (name == "growth_respiration_parameter_1")
          user_crops.pc_GrowthRespirationParameter1 = satof(row[1]);
        else if (name == "use_automatic_irrigation")
          user_env.p_UseAutomaticIrrigation = satoi(row[1]) == 1;
        else if (name == "use_nmin_mineral_fertilising_method")
          user_env.p_UseNMinMineralFertilisingMethod = satoi(row[1]) == 1;
        else if (name == "layer_thickness")
          user_env.p_LayerThickness = satof(row[1]);
        else if (name == "number_of_layers")
          user_env.p_NumberOfLayers = satoi(row[1]);
        else if (name == "start_pv_index")
          user_env.p_StartPVIndex = satoi(row[1]);
		else if (name == "albedo")
          user_env.p_Albedo = satof(row[1]);
        else if (name == "athmospheric_co2")
          user_env.p_AthmosphericCO2 = satof(row[1]);
        else if (name == "wind_speed_height")
          user_env.p_WindSpeedHeight = satof(row[1]);
        else if (name == "use_secondary_yields")
          user_env.p_UseSecondaryYields = satoi(row[1]) == 1;
        else if (name == "julian_day_automatic_fertilising")
          user_env.p_JulianDayAutomaticFertilising = satoi(row[1]);
        else if (name == "critical_moisture_depth")
          user_soil_moisture.pm_CriticalMoistureDepth = satof(row[1]);
        else if (name == "saturated_hydraulic_conductivity")
          user_soil_moisture.pm_SaturatedHydraulicConductivity = satof(row[1]);
        else if (name == "surface_roughness")
          user_soil_moisture.pm_SurfaceRoughness = satof(row[1]);
        else if (name == "hydraulic_conductivity_redux")
          user_soil_moisture.pm_HydraulicConductivityRedux = satof(row[1]);
        else if (name == "snow_accumulation_treshold_temperature")
          user_soil_moisture.pm_SnowAccumulationTresholdTemperature = satof(row[1]);
        else if (name == "kc_factor")
          user_soil_moisture.pm_KcFactor = satof(row[1]);
        else if (name == "time_step")
          user_env.p_timeStep = satof(row[1]);
        else if (name == "temperature_limit_for_liquid_water")
          user_soil_moisture.pm_TemperatureLimitForLiquidWater = satof(row[1]);
        else if (name == "correction_snow")
          user_soil_moisture.pm_CorrectionSnow = satof(row[1]);
        else if (name == "correction_rain")
          user_soil_moisture.pm_CorrectionRain = satof(row[1]);
        else if (name == "snow_max_additional_density")
          user_soil_moisture.pm_SnowMaxAdditionalDensity = satof(row[1]);
        else if (name == "new_snow_density_min")
          user_soil_moisture.pm_NewSnowDensityMin = satof(row[1]);
        else if (name == "snow_retention_capacity_min")
          user_soil_moisture.pm_SnowRetentionCapacityMin = satof(row[1]);
        else if (name == "refreeze_parameter_2")
          user_soil_moisture.pm_RefreezeParameter2 = satof(row[1]);
        else if (name == "refreeze_parameter_1")
          user_soil_moisture.pm_RefreezeParameter1 = satof(row[1]);
        else if (name == "refreeze_temperature")
          user_soil_moisture.pm_RefreezeTemperature = satof(row[1]);
        else if (name == "snowmelt_temperature")
          user_soil_moisture.pm_SnowMeltTemperature = satof(row[1]);
        else if (name == "snow_packing")
          user_soil_moisture.pm_SnowPacking = satof(row[1]);
        else if (name == "snow_retention_capacity_max")
          user_soil_moisture.pm_SnowRetentionCapacityMax = satof(row[1]);
        else if (name == "evaporation_zeta")
          user_soil_moisture.pm_EvaporationZeta = satof(row[1]);
        else if (name == "xsa_critical_soil_moisture")
          user_soil_moisture.pm_XSACriticalSoilMoisture = satof(row[1]);
        else if (name == "maximum_evaporation_impact_depth")
          user_soil_moisture.pm_MaximumEvaporationImpactDepth = satof(row[1]);
        else if (name == "ntau")
          user_soil_temperature.pt_NTau = satof(row[1]);
        else if (name == "initial_surface_temperature")
          user_soil_temperature.pt_InitialSurfaceTemperature = satof(row[1]);
        else if (name == "base_temperature")
          user_soil_temperature.pt_BaseTemperature = satof(row[1]);
        else if (name == "quartz_raw_density")
          user_soil_temperature.pt_QuartzRawDensity = satof(row[1]);
        else if (name == "density_air")
          user_soil_temperature.pt_DensityAir = satof(row[1]);
        else if (name == "density_water")
          user_soil_temperature.pt_DensityWater = satof(row[1]);
        else if (name == "specific_heat_capacity_air")
          user_soil_temperature.pt_SpecificHeatCapacityAir = satof(row[1]);
        else if (name == "specific_heat_capacity_quartz")
          user_soil_temperature.pt_SpecificHeatCapacityQuartz = satof(row[1]);
        else if (name == "specific_heat_capacity_water")
          user_soil_temperature.pt_SpecificHeatCapacityWater = satof(row[1]);
        else if (name == "soil_albedo")
          user_soil_temperature.pt_SoilAlbedo = satof(row[1]);
        else if (name == "dispersion_length")
          user_soil_transport.pq_DispersionLength = satof(row[1]);
        else if (name == "AD")
          user_soil_transport.pq_AD = satof(row[1]);
        else if (name == "diffusion_coefficient_standard")
          user_soil_transport.pq_DiffusionCoefficientStandard = satof(row[1]);
        else if (name == "leaching_depth")
          user_env.p_LeachingDepth = satof(row[1]);
        else if (name == "groundwater_discharge")
          user_soil_moisture.pm_GroundwaterDischarge = satof(row[1]);
        else if (name == "density_humus")
          user_soil_temperature.pt_DensityHumus = satof(row[1]);
        else if (name == "specific_heat_capacity_humus")
          user_soil_temperature.pt_SpecificHeatCapacityHumus = satof(row[1]);
        else if (name == "max_percolation_rate")
          user_soil_moisture.pm_MaxPercolationRate = satof(row[1]);
				else if (name == "max_groundwater_depth")
          user_env.p_MaxGroundwaterDepth = satof(row[1]);
        else if (name == "min_groundwater_depth")
          user_env.p_MinGroundwaterDepth = satof(row[1]);
        else if (name == "min_groundwater_depth_month")
          user_env.p_MinGroundwaterDepthMonth = satoi(row[1]);
		else if (name == "SOM_SlowDecCoeffStandard")
			user_soil_organic.po_SOM_SlowDecCoeffStandard = satof(row[1]);
		else if (name == "SOM_FastDecCoeffStandard")
			user_soil_organic.po_SOM_FastDecCoeffStandard = satof(row[1]);
		else if (name == "SMB_SlowMaintRateStandard")
			user_soil_organic.po_SMB_SlowMaintRateStandard = satof(row[1]);
		else if (name == "SMB_FastMaintRateStandard")
			user_soil_organic.po_SMB_FastMaintRateStandard = satof(row[1]);
		else if (name == "SMB_SlowDeathRateStandard")
			user_soil_organic.po_SMB_SlowDeathRateStandard = satof(row[1]);
		else if (name == "SMB_FastDeathRateStandard")
			user_soil_organic.po_SMB_FastDeathRateStandard = satof(row[1]);
		else if (name == "SMB_UtilizationEfficiency")
			user_soil_organic.po_SMB_UtilizationEfficiency = satof(row[1]);
		else if (name == "SOM_SlowUtilizationEfficiency")
			user_soil_organic.po_SOM_SlowUtilizationEfficiency = satof(row[1]);
		else if (name == "SOM_FastUtilizationEfficiency")
			user_soil_organic.po_SOM_FastUtilizationEfficiency = satof(row[1]);
		else if (name == "AOM_SlowUtilizationEfficiency")
			user_soil_organic.po_AOM_SlowUtilizationEfficiency = satof(row[1]);
		else if (name == "AOM_FastUtilizationEfficiency")
			user_soil_organic.po_AOM_FastUtilizationEfficiency = satof(row[1]);
		else if (name == "AOM_FastMaxC_to_N")
			user_soil_organic.po_AOM_FastMaxC_to_N = satof(row[1]);
		else if (name == "PartSOM_Fast_to_SOM_Slow")
			user_soil_organic.po_PartSOM_Fast_to_SOM_Slow = satof(row[1]);
		else if (name == "PartSMB_Slow_to_SOM_Fast")
			user_soil_organic.po_PartSMB_Slow_to_SOM_Fast = satof(row[1]);
		else if (name == "PartSMB_Fast_to_SOM_Fast")
			user_soil_organic.po_PartSMB_Fast_to_SOM_Fast = satof(row[1]);
		else if (name == "PartSOM_to_SMB_Slow")
			user_soil_organic.po_PartSOM_to_SMB_Slow = satof(row[1]);
		else if (name == "PartSOM_to_SMB_Fast")
			user_soil_organic.po_PartSOM_to_SMB_Fast = satof(row[1]);
		else if (name == "CN_Ratio_SMB")
			user_soil_organic.po_CN_Ratio_SMB = satof(row[1]);
		else if (name == "LimitClayEffect")
			user_soil_organic.po_LimitClayEffect = satof(row[1]);
		else if (name == "AmmoniaOxidationRateCoeffStandard")
			user_soil_organic.po_AmmoniaOxidationRateCoeffStandard = satof(row[1]);
		else if (name == "NitriteOxidationRateCoeffStandard")
			user_soil_organic.po_NitriteOxidationRateCoeffStandard = satof(row[1]);
		else if (name == "TransportRateCoeff")
			user_soil_organic.po_TransportRateCoeff = satof(row[1]);
		else if (name == "SpecAnaerobDenitrification")
			user_soil_organic.po_SpecAnaerobDenitrification = satof(row[1]);
		else if (name == "ImmobilisationRateCoeffNO3")
			user_soil_organic.po_ImmobilisationRateCoeffNO3 = satof(row[1]);
		else if (name == "ImmobilisationRateCoeffNH4")
			user_soil_organic.po_ImmobilisationRateCoeffNH4 = satof(row[1]);
		else if (name == "Denit1")
			user_soil_organic.po_Denit1 = satof(row[1]);
		else if (name == "Denit2")
			user_soil_organic.po_Denit2 = satof(row[1]);
		else if (name == "Denit3")
			user_soil_organic.po_Denit3 = satof(row[1]);
		else if (name == "HydrolysisKM")
			user_soil_organic.po_HydrolysisKM = satof(row[1]);
		else if (name == "ActivationEnergy")
			user_soil_organic.po_ActivationEnergy = satof(row[1]);
		else if (name == "HydrolysisP1")
			user_soil_organic.po_HydrolysisP1 = satof(row[1]);
		else if (name == "HydrolysisP2")
			user_soil_organic.po_HydrolysisP2 = satof(row[1]);
		else if (name == "AtmosphericResistance")
			user_soil_organic.po_AtmosphericResistance = satof(row[1]);
		else if (name == "N2OProductionRate")
			user_soil_organic.po_N2OProductionRate = satof(row[1]);
		else if (name == "Inhibitor_NH3")
			user_soil_organic.po_Inhibitor_NH3 = satof(row[1]);
      }
      delete con;
      initialized = true;
#ifdef RUN_MACSUR_SCALING
      initialized = false;
#endif

      centralParameterProvider.capillaryRiseRates = readCapillaryRiseRates();
    }
  }

  return centralParameterProvider;
}

//----------------------------------------------------------------------------

//RPSCDRes Monica::readPrincipalSoilCharacteristicData(string soilType, double rawDensity)
//{
//	static mutex lockable;
//	typedef map<int, RPSCDRes> M1;
//	typedef map<string, M1> M2;
//	static M2 m;
//	static bool initialized = false;
//	if(!initialized)
//	{
//		lock_guard<mutex> lock(lockable);

//		if(!initialized)
//		{
//			DBPtr con(newConnection("monica"));

//			string query("select soil_type, soil_raw_density*10, "
//									 "air_capacity, field_capacity, n_field_capacity "
//									 "from soil_characteristic_data "
//									 "where air_capacity != 0 and field_capacity != 0 and n_field_capacity != 0 "
//									 "order by soil_type, soil_raw_density");
//			con->select(query.c_str());

//			debug() << endl << query.c_str() << endl;
//			DBRow row;
//			while(!(row = con->getRow()).empty())
//			{
//				double ac = satof(row[2]);
//				double fc = satof(row[3]);
//				double nfc = satof(row[4]);

//				RPSCDRes r(true);
//				r.sat = ac + fc;
//				r.fc = fc;
//				r.pwp = fc - nfc;

//				m[row[0]][satoi(row[1])] = r;
//			}

//			initialized = true;
//		}
//	}

//	auto ci = m.find(soilType);
//	if(ci != m.end())
//	{
//		int rd10 = int(rawDensity*10);
//		int delta = rd10 < 15 ? 2 : -2;

//		M1::const_iterator ci2;
//		//if we didn't find values for a given raw density, e.g. 1.1 (= 11)
//		//we try to find the closest next one (up (1.1) or down (1.9))
//		while((ci2 = ci->second.find(rd10)) == ci->second.end() &&
//					(11 <= rd10 && rd10 <= 19))
//			rd10 += delta;

//		return ci2 != ci->second.end() ? ci2->second : RPSCDRes();
//	}

//	return RPSCDRes();
//}

//RPSCDRes Monica::readSoilCharacteristicModifier(string soilType, double organicMatter)
//{
//	static mutex lockable;
//	typedef map<int, RPSCDRes> M1;
//	typedef map<string, M1> M2;
//	static M2 m;
//	static bool initialized = false;
//	if(!initialized)
//	{
//		lock_guard<mutex> lock(lockable);

//		if(!initialized)
//		{
//			DBPtr con(newConnection("monica"));

//			string query("select soil_type, organic_matter*10, "
//									 "air_capacity, field_capacity, n_field_capacity "
//									 "from soil_aggregation_values "
//									 "order by soil_type, organic_matter");
//			con->select(query.c_str());

//			debug() << endl << query.c_str() << endl;
//			DBRow row;
//			while(!(row = con->getRow()).empty())
//			{
//				double ac = satof(row[2]);
//				double fc = satof(row[3]);
//				double nfc = satof(row[4]);

//				RPSCDRes r(true);
//				r.sat = ac + fc;
//				r.fc = fc;
//				r.pwp = fc - nfc;

//				m[row[0]][satoi(row[1])] = r;
//			}

//			initialized = true;
//		}
//	}

//	auto ci = m.find(soilType);
//	if(ci != m.end())
//	{
//		auto ci2 = ci->second.find(int(organicMatter*10));
//		return ci2 != ci->second.end() ? ci2->second : RPSCDRes();
//	}

//	return RPSCDRes();
//}


//namespace
//{
//  struct X
//  {
//    double sat, fc, pwp;

//		static int makeInt(double value) { return int(Tools::round(value, 1)*10); }
//  };

//	void readXSoilCharacteristicY(std::string key1, double key2,
//                                double &sat, double &fc, double &pwp,
//                                string query)
//  {
//    static mutex lockable;
//    typedef map<int, X> M1;
//    typedef map<string, M1> M2;
//    typedef map<string, M2> M3;
//    static M3 m;
//    static bool initialized = false;
//    if(!initialized)
//    {
//      lock_guard<mutex> lock(lockable);

//      if(!initialized)
//      {
//        // read soil characteristic like air-, field- and n-field-capacity from monica database
//        DBPtr con(newConnection("monica"));
//        con->select(query.c_str());
//        debug() << endl << query.c_str() << endl;
//        DBRow row;
//        while (!(row = con->getRow()).empty())
//        {
//          double ac = satof(row[2]);
//          double fc = satof(row[3]);
//          double nfc = satof(row[4]);

//          int r = X::makeInt(satof(row[1]));
//          X& x = m[query][row[0]][r];
//          x.sat = ac + fc;
//          x.fc = fc;
//          x.pwp = fc - nfc;
//        }
				
//				initialized = true;
//      }
//    }

//    M3::const_iterator qci = m.find(query);
//    if(qci != m.end())
//    {
//      const M2& m2 = qci->second;
//      M2::const_iterator ci = m2.find(key1);
////      debug () <<"key1 " << key1.c_str() << endl;
//      if(ci != m2.end())
//      {
//        const M1& m1 = ci->second;
//        M1::const_iterator ci2 = m1.find(X::makeInt(key2));
////        debug () <<"key2 " << key2 << endl;
//        if(ci2 != m1.end())
//        {
//          const X& x = ci2->second;
//          sat = x.sat;
//          fc = x.fc;
//          pwp = x.pwp;
//          return;
//        }
//      }
//    }

//    sat = 0;
//    fc = 0;
//    pwp = 0;
//  }
//}

//void Monica::readPrincipalSoilCharacteristicData(std::string soil_type,
//                                                 double raw_density,
//                                                 double &sat, double &fc,
//                                                 double &pwp)
//{
//  static const string query =
//      "select soil_type, soil_raw_density, air_capacity, "
//      "field_capacity, n_field_capacity "
//      "from soil_characteristic_data";

//  return readXSoilCharacteristicY(soil_type, raw_density, sat, fc, pwp, query);
//}

//void Monica::readSoilCharacteristicModifier(std::string soil_type,
//                                            double organic_matter,
//                                            double &sat, double &fc,
//                                            double &pwp)
//{
//  static const string query =
//      "select soil_type, organic_matter, air_capacity, "
//      "field_capacity, n_field_capacity "
//      "from soil_aggregation_values";

//  return readXSoilCharacteristicY(soil_type, organic_matter, sat, fc, pwp,
//                                  query);
//}

/**
 * Simple output of climate data stored in given data accessor.
 * @param climateData Data structure that stores climate data.
 */
void
    Monica::testClimateData(Climate::DataAccessor &climateData)
{
  for (int i = 0, size = climateData.noOfStepsPossible(); i < size; i++)
  {
    double tmin = climateData.dataForTimestep(Climate::tmin, i);
    double tavg = climateData.dataForTimestep(Climate::tavg, i);
    double tmax = climateData.dataForTimestep(Climate::tmax, i);
    double precip = climateData.dataForTimestep(Climate::precip, i);
    double wind = climateData.dataForTimestep(Climate::wind, i);
    double globrad = climateData.dataForTimestep(Climate::globrad, i);
    double relhumid = climateData.dataForTimestep(Climate::relhumid, i);
    double sunhours = climateData.dataForTimestep(Climate::sunhours, i);
    debug() << "day: " << i << " tmin: " << tmin << " tavg: " << tavg
        << " tmax: " << tmax << " precip: " << precip
        << " wind: " << wind << " globrad: " << globrad
        << " relhumid: " << relhumid << " sunhours: " << sunhours
        << endl;
  }
}

Monica::CropPtr Monica::hermesCropId2Crop(const string& hermesCropId)
{
	if(hermesCropId == "WW")
		return CropPtr(new Crop(1, hermesCropId)); // Winter wheat
	if(hermesCropId == "SW")
		return CropPtr(new Crop(25, hermesCropId)); // Spring wheat
	if(hermesCropId == "WG")
		return CropPtr(new Crop(2, hermesCropId)); // Winter barley
	if(hermesCropId == "SG")
		return CropPtr(new Crop(4, hermesCropId)); // Spring barley
	if(hermesCropId == "WR")
		return CropPtr(new Crop(3, hermesCropId)); // Winter rye
  if(hermesCropId == "WR_GD")
    return CropPtr(new Crop(51, hermesCropId)); // Silage winter rye
	if(hermesCropId == "SR")
		return CropPtr(new Crop(20, hermesCropId)); // Spring rye
	if(hermesCropId == "OAT")
		return CropPtr(new Crop(22, hermesCropId)); // Oats
	if(hermesCropId == "ZR")
		return CropPtr(new Crop(10, hermesCropId)); // Sugar beet
	if(hermesCropId == "SM")
		return CropPtr(new Crop(7, hermesCropId)); // Silage maize
	if(hermesCropId == "GM")
		return CropPtr(new Crop(5, hermesCropId)); // Grain maize
	if(hermesCropId == "GMB")
		return CropPtr(new Crop(6, hermesCropId)); // Grain maize Brazil (Pioneer)
	if(hermesCropId == "MEP")
		return CropPtr(new Crop(8, hermesCropId)); // Early potato
	if(hermesCropId == "MLP")
		return CropPtr(new Crop(8, hermesCropId)); // Late potato
	if(hermesCropId == "WC")
		return CropPtr(new Crop(9, hermesCropId)); // Winter canola
	if(hermesCropId == "SC")
		return CropPtr(new Crop(9, hermesCropId)); // Spring canola
	if(hermesCropId == "MU")
		return CropPtr(new Crop(11, hermesCropId)); // Mustard
	if(hermesCropId == "PH")
		return CropPtr(new Crop(12, hermesCropId)); // Phacelia
	if(hermesCropId == "CLV")
		return CropPtr(new Crop(13, hermesCropId)); // Kleegras
	if(hermesCropId == "LZG")
		return CropPtr(new Crop(14, hermesCropId)); // Luzerne-Gras
	if(hermesCropId == "WDG")
		return CropPtr(new Crop(16, hermesCropId)); // Weidelgras
	if(hermesCropId == "FP")
		return CropPtr(new Crop(24, hermesCropId)); // Field pea
	if(hermesCropId == "OR")
		return CropPtr(new Crop(17, hermesCropId)); // Oil raddish
	if(hermesCropId == "SDG")
		return CropPtr(new Crop(18, hermesCropId)); // Sudan grass
	if(hermesCropId == "WTR")
		return CropPtr(new Crop(19, hermesCropId)); // Winter triticale
	if(hermesCropId == "STR")
		return CropPtr(new Crop(23, hermesCropId)); // Spring triticale
	if(hermesCropId == "SOR")
		return CropPtr(new Crop(21, hermesCropId)); // Sorghum
	if(hermesCropId == "SX0")
		return CropPtr(new Crop(28, hermesCropId)); // Soy bean maturity group 000
	if(hermesCropId == "S00")
		return CropPtr(new Crop(29, hermesCropId)); // Soy bean maturity group 00
	if(hermesCropId == "S0X")
		return CropPtr(new Crop(30, hermesCropId)); // Soy bean maturity group 0
	if(hermesCropId == "S01")
		return CropPtr(new Crop(31, hermesCropId)); // Soy bean maturity group I
	if(hermesCropId == "S02")
		return CropPtr(new Crop(32, hermesCropId)); // Soy bean maturity group II
	if(hermesCropId == "S03")
		return CropPtr(new Crop(33, hermesCropId)); // Soy bean maturity group III
	if(hermesCropId == "S04")
		return CropPtr(new Crop(34, hermesCropId)); // Soy bean maturity group IV
	if(hermesCropId == "S05")
		return CropPtr(new Crop(35, hermesCropId)); // Soy bean maturity group V
	if(hermesCropId == "S06")
		return CropPtr(new Crop(36, hermesCropId)); // Soy bean maturity group VI
	if(hermesCropId == "S07")
		return CropPtr(new Crop(37, hermesCropId)); // Soy bean maturity group VII
	if(hermesCropId == "S08")
		return CropPtr(new Crop(38, hermesCropId)); // Soy bean maturity group VIII
	if(hermesCropId == "S09")
		return CropPtr(new Crop(39, hermesCropId)); // Soy bean maturity group IX
	if(hermesCropId == "S10")
		return CropPtr(new Crop(40, hermesCropId)); // Soy bean maturity group X
	if(hermesCropId == "S11")
		return CropPtr(new Crop(41, hermesCropId)); // Soy bean maturity group XI
	if(hermesCropId == "S12")
		return CropPtr(new Crop(42, hermesCropId)); // Soy bean maturity group XII
	if(hermesCropId == "COS")
		return CropPtr(new Crop(43, hermesCropId)); // Cotton short
	if(hermesCropId == "COM")
		return CropPtr(new Crop(44, hermesCropId)); // Cotton medium
	if(hermesCropId == "COL")
		return CropPtr(new Crop(45, hermesCropId)); // Cotton long
	if (hermesCropId == "EMM")
		return CropPtr(new Crop(47, hermesCropId)); // Emmer 3000 b.c.
	if (hermesCropId == "EIN")
		return CropPtr(new Crop(48, hermesCropId)); // Einkorn 3000 b.c.
	if (hermesCropId == "COB")
		return CropPtr(new Crop(46, hermesCropId)); // Cotton medium Brazil
	if (hermesCropId == "SC")
		return CropPtr(new Crop(49, hermesCropId)); // Sugar cane transplant	
	if (hermesCropId == "SCT")
	  return CropPtr(new Crop(50, hermesCropId)); // Sugar cane ratoon
	if (hermesCropId == "DUW")
		return CropPtr(new Crop(52, hermesCropId)); // Durum wheat
	if (hermesCropId == "FTO")
		return CropPtr(new Crop(53, hermesCropId)); // Field tomato
	if (hermesCropId == "BR")
		return CropPtr(new Crop(hermesCropId));


	return CropPtr();
}



//const CapillaryRiseRates& Monica::readCapillaryRiseRates()
//{
//	static mutex lockable;
//	static bool initialized = false;
//  static CapillaryRiseRates cap_rates;

//	if (!initialized)
//  {
//    lock_guard<mutex> lock(lockable);

//    if(!initialized)
//		{

//      static const string query =
//          "select soil_type, distance, capillary_rate "
//          "from capillary_rise_rate";

//      // read capillary rise rates from database
//      DB *con = newConnection("monica");
//      con->select(query.c_str());

//      DBRow row;
//      while (!(row = con->getRow()).empty())
//      {
//        string soil_type = row[0];
//        int distance = satoi(row[1]);
//        double rate = satof(row[2]);
//        cap_rates.addRate(soil_type, distance, rate);
//      }

//			delete con;

//			initialized = true;
//    }
//	}

//	return cap_rates;
//}

/*
void
Monica::Crop::applyCutting()
{
  const CropParameters* cps = this->cropParameters();


}
*/


const map<int, string>& Monica::availableMonicaCrops()
{
  static mutex lockable;
  static map<int, string> m;
  static bool initialized = false;
  if(!initialized)
  {
    lock_guard<mutex> lock(lockable);

    if(!initialized)
    {
      DBPtr con(newConnection("monica"));

      string query("select id, name "
                   "from crop "
                   "order by id");
      con->select(query.c_str());

      DBRow row;
      while(!(row = con->getRow()).empty())
        m[satoi(row[0])] = capitalize(row[1]);

      initialized = true;
    }
  }

  return m;
}
