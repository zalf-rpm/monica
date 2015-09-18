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
#include "../core/monica-parameters.h"
#include "../core/monica.h"
#include "tools/debug.h"
#include "soil/conversion.h"
#include "soil/soil.h"
#include "../io/database-io.h"

#include "production-process.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Soil;
using namespace Tools;
using namespace Climate;

//----------------------------------------------------------------------------

WorkStep::WorkStep(const Tools::Date& d)
  : _date(d)
{}

WorkStep::WorkStep(json11::Json j)
  : _date(Date::fromIsoDateString(string_value(j, "date")))
{}

json11::Json WorkStep::to_json() const
{
  return json11::Json::object {
    {"type", "WorkStep"},
    {"date", date().toIsoDateString()},};
}

//------------------------------------------------------------------------------

Seed::Seed(const Tools::Date& at, CropPtr crop)
  : WorkStep(at),
    _crop(crop)
{}

Seed::Seed(json11::Json j)
  : WorkStep(j),
    _crop(std::make_shared<Crop>(j["crop"]))
{}

json11::Json Seed::to_json() const
{
  return json11::Json::object {
    {"type", "Seed"},
    {"date", date().toIsoDateString()},
    {"crop", *_crop}};
}

void Seed::apply(MonicaModel* model)
{
  debug() << "seeding crop: " << _crop->toString() << " at: " << date().toString() << endl;
  model->seedCrop(_crop);
}

//------------------------------------------------------------------------------

Harvest::Harvest(const Tools::Date& at,
                 CropPtr crop,
                 PVResultPtr cropResult,
                 std::string method)
  : WorkStep(at),
    _crop(crop),
    _cropResult(cropResult),
    _method(method)
{}

Harvest::Harvest(json11::Json j,
                 CropPtr crop)
  : WorkStep(j),
    _crop(crop),
    _cropResult(new PVResult(crop->id())),
    _method(j["method"].string_value()),
    _percentage(j["percentage"].number_value()),
    _exported(j["exported"].bool_value())
{}

json11::Json Harvest::to_json() const
{
  return json11::Json::object {
    {"type", "Harvest"},
    {"date", date().toIsoDateString()},
    {"method", _method},
    {"percentage", _percentage},
    {"exported", _exported}};
}


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

//------------------------------------------------------------------------------

Cutting::Cutting(const Tools::Date& at,
                 CropPtr crop)
  : WorkStep(at),
    _crop(crop)
{}

Cutting::Cutting(json11::Json j,
                 CropPtr crop)
  : WorkStep(j),
    _crop(crop)
{}

json11::Json Cutting::to_json() const
{
  return json11::Json::object {
    {"type", "Cutting"},
    {"date", date().toIsoDateString()}};
}

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

//------------------------------------------------------------------------------

MineralFertiliserApplication::MineralFertiliserApplication(const Tools::Date& at,
                                                           MineralFertiliserParameters partition,
                                                           double amount)
  : WorkStep(at),
    _partition(partition),
    _amount(amount)
{}

MineralFertiliserApplication::MineralFertiliserApplication(json11::Json j)
  : WorkStep(j),
    _partition(j["parameters"]),
    _amount(number_value(j, "amount"))
{}

json11::Json MineralFertiliserApplication::to_json() const
{
  return json11::Json::object {
    {"type", "MineralFertiliserApplication"},
    {"date", date().toIsoDateString()},
    {"amount", _amount},
    {"parameters", _partition}};
}

void MineralFertiliserApplication::apply(MonicaModel* model)
{
  debug() << toString() << endl;
  model->applyMineralFertiliser(partition(), amount());
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

//------------------------------------------------------------------------------

TillageApplication::TillageApplication(const Tools::Date& at,
                                       double depth)
  : WorkStep(at),
    _depth(depth)
{}

TillageApplication::TillageApplication(json11::Json j)
  : WorkStep(j),
    _depth(number_value(j, "depth"))
{}

json11::Json TillageApplication::to_json() const
{
  return json11::Json::object {
    {"type", "TillageApplication"},
    {"date", date().toIsoDateString()},
    {"depth", _depth}};
}

void TillageApplication::apply(MonicaModel* model)
{
  debug() << toString() << endl;
  model->applyTillage(_depth);
}

//------------------------------------------------------------------------------

IrrigationApplication::IrrigationApplication(const Tools::Date& at,
                      double amount,
                      IrrigationParameters params)
  : WorkStep(at),
    _amount(amount),
    _params(params)
{}

IrrigationApplication::IrrigationApplication(json11::Json j)
  : WorkStep(j),
    _amount(number_value(j, "amount")),
    _params(j["parameters"])
{}

json11::Json IrrigationApplication::to_json() const
{
  return json11::Json::object {
    {"type", "IrrigationApplication"},
    {"date", date().toIsoDateString()},
    {"amount", _amount},
    {"parameters", _params}};
}

void IrrigationApplication::apply(MonicaModel* model)
{
  //cout << toString() << endl;
  model->applyIrrigation(amount(), nitrateConcentration());
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



