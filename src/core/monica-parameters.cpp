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

// Some profile definitions for eva2

namespace
{
  /*!
 * @todo Claas bitte ausfüllen
 * @param name
 * @return
 */
  

} // namespace

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
        if (model->cropGrowth())
        {
          _crop->setHarvestYields(model->cropGrowth()->get_FreshPrimaryCropYield() /
                                  100.0, model->cropGrowth()->get_FreshSecondaryCropYield() / 100.0);
          _crop->setHarvestYieldsTM(model->cropGrowth()->get_PrimaryCropYield() / 100.0,
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
				_cropResult->pvResults[soilMoist0_90cmAtHarvest] = model->mean90cmWaterContent();
				_cropResult->pvResults[corg0_30cmAtHarvest] = model->avgCorg(0.3);
				_cropResult->pvResults[nmin0_90cmAtHarvest] = model->sumNmin(0.9);

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

ProductionProcess::ProductionProcess(const std::string& name, CropPtr crop)
  : _name(name),
    _crop(crop),
    _cropResult(new PVResult(crop->id()))
{
  debug() << "ProductionProcess: " << name.c_str() << endl;

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

ProductionProcess ProductionProcess::deepCloneAndClearWorksteps() const
{
  ProductionProcess clone(name(), CropPtr(new Crop(*(crop().get()))));
  clone._cropResult = PVResultPtr(new PVResult(*(_cropResult.get())));
  return clone;
}

void ProductionProcess::apply(const Date& date, MonicaModel* model) const
{
  auto p = _worksteps.equal_range(date);
  while (p.first != p.second)
  {
    p.first->second->apply(model);
    p.first++;
  }
}

Date ProductionProcess::nextDate(const Date& date) const
{
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

//----------------------------------------------------------------------------

CropParameters::CropParameters(json11::Json j)
  : pc_CropName(j["CropName"].string_value()),
    pc_Perennial(j["Perennial"].bool_value()),
    pc_NumberOfDevelopmentalStages(j["NumberOfDevelopmentalStages"].int_value()),
    pc_NumberOfOrgans(j["NumberOfOrgans"].int_value()),
    pc_CarboxylationPathway(j["CarboxylationPathway"].int_value()),
    pc_DefaultRadiationUseEfficiency(j["DefaultRadiationUseEfficiency"].number_value()),
    pc_PartBiologicalNFixation(j["PartBiologicalNFixation"].number_value()),
    pc_InitialKcFactor(j["InitialKcFactor"].number_value()),
    pc_LuxuryNCoeff(j["LuxuryNCoeff"].number_value()),
    pc_MaxAssimilationRate(j["MaxAssimilationRate"].number_value()),
    pc_MaxCropDiameter(j["MaxCropDiameter"].number_value()),
    pc_MaxCropHeight(j["MaxCropHeight"].number_value()),
    pc_CropHeightP1(j["CropHeightP1"].number_value()),
    pc_CropHeightP2(j["CropHeightP2"].number_value()),
    pc_StageAtMaxHeight(j["StageAtMaxHeight"].number_value()),
    pc_StageAtMaxDiameter(j["StageAtMaxDiameter"].number_value()),
    pc_MinimumNConcentration(j["MinimumNConcentration"].number_value()),
    pc_MinimumTemperatureForAssimilation(j["MinimumTemperatureForAssimilation"].number_value()),
    pc_NConcentrationAbovegroundBiomass(j["NConcentrationAbovegroundBiomass"].number_value()),
    pc_NConcentrationB0(j["NConcentrationB0"].number_value()),
    pc_NConcentrationPN(j["NConcentrationPN"].number_value()),
    pc_NConcentrationRoot(j["NConcentrationRoot"].number_value()),
    pc_ResidueNRatio(j["ResidueNRatio"].number_value()),
    pc_DevelopmentAccelerationByNitrogenStress(j["DevelopmentAccelerationByNitrogenStress"].int_value()),
    pc_FieldConditionModifier(j["FieldConditionModifier"].number_value()),
    pc_AssimilateReallocation(j["AssimilateReallocation"].number_value()),
    pc_LT50cultivar(j["LT50cultivar"].number_value()),
    pc_FrostHardening(j["FrostHardening"].number_value()),
    pc_FrostDehardening(j["FrostDehardening"].number_value()),
    pc_LowTemperatureExposure(j["LowTemperatureExposure"].number_value()),
    pc_RespiratoryStress(j["RespiratoryStress"].number_value()),
    pc_LatestHarvestDoy(j["LatestHarvestDoy"].int_value()),
//    std::vector<std::vector<double>> pc_AssimilatePartitioningCoeff,
//    std::vector<std::vector<double>> pc_OrganSenescenceRate,
    pc_BaseDaylength(toDoubleVector(j["BaseDaylength"])),
    pc_BaseTemperature(toDoubleVector(j["BaseTemperature"])),
    pc_OptimumTemperature(toDoubleVector(j["OptimumTemperature"])),
    pc_DaylengthRequirement(toDoubleVector(j["DaylengthRequirement"])),
    pc_DroughtStressThreshold(toDoubleVector(j["DroughtStressThreshold"])),
    pc_OrganMaintenanceRespiration(toDoubleVector(j["OrganMaintenanceRespiration"])),
    pc_OrganGrowthRespiration(toDoubleVector(j["OrganGrowthRespiration"])),
    pc_SpecificLeafArea(toDoubleVector(j["SpecificLeafArea"])),
    pc_StageMaxRootNConcentration(toDoubleVector(j["StageMaxRootNConcentration"])),
    pc_StageKcFactor(toDoubleVector(j["StageKcFactor"])),
    pc_StageTemperatureSum(toDoubleVector(j["StageTemperatureSum"])),
    pc_VernalisationRequirement(toDoubleVector(j["VernalisationRequirement"])),
    pc_InitialOrganBiomass(toDoubleVector(j["InitialOrganBiomass"])),
    pc_CriticalOxygenContent(toDoubleVector(j["CriticalOxygenContent"])),
    pc_CropSpecificMaxRootingDepth(j["CropSpecificMaxRootingDepth"].number_value()),
    pc_AbovegroundOrgan(toIntVector(j["AbovegroundOrgan"])),
    pc_StorageOrgan(toIntVector(j["StorageOrgan"])),
    pc_SamplingDepth(j["SamplingDepth"].number_value()),
    pc_TargetNSamplingDepth(j["TargetNSamplingDepth"].number_value()),
    pc_TargetN30(j["TargetN30"].number_value()),
    pc_HeatSumIrrigationStart(j["HeatSumIrrigationStart"].number_value()),
    pc_HeatSumIrrigationEnd(j["HeatSumIrrigationEnd"].number_value()),
    pc_MaxNUptakeParam(j["MaxNUptakeParam"].number_value()),
    pc_RootDistributionParam(j["RootDistributionParam"].number_value()),
    pc_PlantDensity(j["PlantDensity"].number_value()),
    pc_RootGrowthLag(j["RootGrowthLag"].number_value()),
    pc_MinimumTemperatureRootGrowth(j["MinimumTemperatureRootGrowth"].number_value()),
    pc_InitialRootingDepth(j["InitialRootingDepth"].number_value()),
    pc_RootPenetrationRate(j["RootPenetrationRate"].number_value()),
    pc_RootFormFactor(j["RootFormFactor"].number_value()),
    pc_SpecificRootLength(j["SpecificRootLength"].number_value()),
    pc_StageAfterCut(j["StageAfterCut"].int_value()),
    pc_CriticalTemperatureHeatStress(j["CriticalTemperatureHeatStress"].number_value()),
    pc_LimitingTemperatureHeatStress(j["LimitingTemperatureHeatStress"].number_value()),
    pc_BeginSensitivePhaseHeatStress(j["BeginSensitivePhaseHeatStress"].number_value()),
    pc_EndSensitivePhaseHeatStress(j["EndSensitivePhaseHeatStress"].number_value()),
    pc_CuttingDelayDays(j["CuttingDelayDays"].int_value()),
    pc_DroughtImpactOnFertilityFactor(j["DroughtImpactOnFertilityFactor"].number_value()),
    pc_OrganIdsForPrimaryYield(toVector<YieldComponent>(j["OrganIdsForPrimaryYield"])),
    pc_OrganIdsForSecondaryYield(toVector<YieldComponent>(j["OrganIdsForSecondaryYield"])),
    pc_OrganIdsForCutting(toVector<YieldComponent>(j["OrganIdsForCutting"]))
{
  for(auto js : j["AssimilatePartitioningCoeff"].array_items())
    pc_AssimilatePartitioningCoeff.push_back(toDoubleVector(js));

  for(auto js : j["OrganSenescenceRate"].array_items())
    pc_OrganSenescenceRate.push_back(toDoubleVector(js));
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
    ps_LayerThickness(toDoubleVector(j["LayerThickness"])),
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
  //  auto lts = j["LayerThickness"].array_items();
  //  for(auto lt : lts)
  //    ps_LayerThickness.push_back(lt.number_value());
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
		{ "type", "MineralFertiliserParameters" },
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


//-----------------------------------------------------------------------------------------

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


