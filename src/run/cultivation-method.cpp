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

#include "cultivation-method.h"

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
  : WorkStep(at)
  , _crop(crop)
{}

Seed::Seed(json11::Json j)
  : WorkStep(j)
  , _crop(std::make_shared<Crop>(j["crop"]))
{}

json11::Json Seed::to_json(bool includeFullCropParameters) const
{
  return json11::Json::object {
    {"type", "Seed"},
    {"date", date().toIsoDateString()},
    {"crop", _crop->to_json(includeFullCropParameters)}};
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
  : WorkStep(at)
  ,  _crop(crop)
  , _cropResult(cropResult)
  , _method(method)
{}

Harvest::Harvest(json11::Json j)
  : WorkStep(j)
  , _crop(std::make_shared<Crop>(j["crop"]))
  , _cropResult(new PVResult(_crop->id()))
  , _method(j["method"].string_value())
  , _percentage(j["percentage"].number_value())
  , _exported(j["exported"].bool_value())
{}

json11::Json Harvest::to_json(bool includeFullCropParameters) const
{
  return json11::Json::object {
    {"type", "Harvest"},
    {"date", date().toIsoDateString()},
    {"crop", _crop->to_json(includeFullCropParameters)},
    {"method", _method},
    {"percentage", _percentage},
    {"exported", _exported}};
}

void Harvest::apply(MonicaModel* model)
{
  if(model->cropGrowth())
  {
    auto crop = model->currentCrop();
    _cropResult->id = crop->id();

    if ((_method == "total") || (_method == "fruitHarvest") || (_method == "cutting"))
    {
      debug() << "harvesting crop: " << crop->toString() << " at: " << date().toString() << endl;
      crop->setHarvestYields(model->cropGrowth()->get_FreshPrimaryCropYield() /
                             100.0, model->cropGrowth()->get_FreshSecondaryCropYield() / 100.0);
      crop->setHarvestYieldsTM(model->cropGrowth()->get_PrimaryCropYield() / 100.0,
                               model->cropGrowth()->get_SecondaryCropYield() / 100.0);

      crop->setYieldNContent(model->cropGrowth()->get_PrimaryYieldNContent(),
                             model->cropGrowth()->get_SecondaryYieldNContent());
      crop->setSumTotalNUptake(model->cropGrowth()->get_SumTotalNUptake());
      crop->setCropHeight(model->cropGrowth()->get_CropHeight());
      crop->setAccumulatedETa(model->cropGrowth()->get_AccumulatedETa());
      crop->setAccumulatedTranspiration(model->cropGrowth()->get_AccumulatedTranspiration());
      crop->setAnthesisDay(model->cropGrowth()->getAnthesisDay());
      crop->setMaturityDay(model->cropGrowth()->getMaturityDay());

      //store results for this crop
      _cropResult->pvResults[primaryYield] = crop->primaryYield();
      _cropResult->pvResults[secondaryYield] = crop->secondaryYield();
      _cropResult->pvResults[primaryYieldTM] = crop->primaryYieldTM();
      _cropResult->pvResults[secondaryYieldTM] = crop->secondaryYieldTM();
      _cropResult->pvResults[sumIrrigation] = crop->appliedIrrigationWater();
      _cropResult->pvResults[biomassNContent] = crop->primaryYieldN();
      _cropResult->pvResults[aboveBiomassNContent] = crop->aboveGroundBiomasseN();
      _cropResult->pvResults[aboveGroundBiomass] = crop->aboveGroundBiomass();
      _cropResult->pvResults[daysWithCrop] = model->daysWithCrop();
      _cropResult->pvResults[sumTotalNUptake] = crop->sumTotalNUptake();
      _cropResult->pvResults[cropHeight] = crop->cropHeight();
      _cropResult->pvResults[sumETaPerCrop] = crop->get_AccumulatedETa();
      _cropResult->pvResults[sumTraPerCrop] = crop->get_AccumulatedTranspiration();
      _cropResult->pvResults[cropname] = crop->id();
      _cropResult->pvResults[NStress] = model->getAccumulatedNStress();
      _cropResult->pvResults[WaterStress] = model->getAccumulatedWaterStress();
      _cropResult->pvResults[HeatStress] = model->getAccumulatedHeatStress();
      _cropResult->pvResults[OxygenStress] = model->getAccumulatedOxygenStress();
      _cropResult->pvResults[anthesisDay] = crop->getAnthesisDay();
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
    else if (_method == "leafPruning")
    {
      debug() << "pruning leaves of: " << crop->toString() << " at: " << date().toString() << endl;
      model->leafPruningCurrentCrop(_percentage, _exported);
    }
    else if (_method == "tipPruning")
    {
      debug() << "pruning tips of: " << crop->toString() << " at: " << date().toString() << endl;
      model->tipPruningCurrentCrop(_percentage, _exported);
    }
    else if (_method == "shootPruning")
    {
      debug() << "pruning shoots of: " << crop->toString() << " at: " << date().toString() << endl;
      model->shootPruningCurrentCrop(_percentage, _exported);
    }
  }
  else
  {
    debug() << "Cannot harvest crop because there is not one anymore" << endl;
    debug() << "Maybe automatic harvest trigger was already activated so that the ";
    debug() << "crop was already harvested. This must be the fallback harvest application ";
    debug() << "that is not necessary anymore and should be ignored" << endl;
  }
}

//------------------------------------------------------------------------------

Cutting::Cutting(const Tools::Date& at)
  : WorkStep(at)
{}

Cutting::Cutting(json11::Json j)
  : WorkStep(j)
{}

json11::Json Cutting::to_json() const
{
  return json11::Json::object {
    {"type", "Cutting"},
    {"date", date().toIsoDateString()}};
}

void Cutting::apply(MonicaModel* model)
{

  assert(model->currentCrop() && model->cropGrowth());

  auto crop = model->currentCrop();

  debug() << "Cutting crop: " << crop->toString() << " at: " << date().toString() << endl;

  crop->setHarvestYields(model->cropGrowth()->get_FreshPrimaryCropYield() / 100.0,
                         model->cropGrowth()->get_FreshSecondaryCropYield() / 100.0);

  crop->setHarvestYieldsTM(model->cropGrowth()->get_PrimaryCropYield() / 100.0,
                           model->cropGrowth()->get_SecondaryCropYield() / 100.0);

  crop->setYieldNContent(model->cropGrowth()->get_PrimaryYieldNContent(),
                         model->cropGrowth()->get_SecondaryYieldNContent());

  crop->setSumTotalNUptake(model->cropGrowth()->get_SumTotalNUptake());
  crop->setCropHeight(model->cropGrowth()->get_CropHeight());

  model->cropGrowth()->applyCutting();
}

//------------------------------------------------------------------------------

MineralFertiliserApplication::MineralFertiliserApplication(const Tools::Date& at,
                                                           MineralFertiliserParameters partition,
                                                           double amount)
  : WorkStep(at)
  , _partition(partition)
  , _amount(amount)
{}

MineralFertiliserApplication::MineralFertiliserApplication(json11::Json j)
  : WorkStep(j)
  , _partition(j["partition"])
  , _amount(double_value(j, "amount"))
{}

json11::Json MineralFertiliserApplication::to_json() const
{
  return json11::Json::object {
    {"type", "MineralFertiliserApplication"},
    {"date", date().toIsoDateString()},
    {"amount", _amount},
    {"partition", _partition}};
}

void MineralFertiliserApplication::apply(MonicaModel* model)
{
  debug() << toString() << endl;
  model->applyMineralFertiliser(partition(), amount());
}

//------------------------------------------------------------------------------

OrganicFertiliserApplication::OrganicFertiliserApplication(const Tools::Date& at,
                                                           OrganicMatterParametersPtr params,
                                                           double amount,
                                                           bool incorp)
  : WorkStep(at)
  , _params(params)
  , _amount(amount)
  , _incorporation(incorp)
{}

OrganicFertiliserApplication::OrganicFertiliserApplication(json11::Json j)
  : WorkStep(Tools::Date::fromIsoDateString(j["date"].string_value()))
  , _params(make_shared<OrganicMatterParameters>(j["parameters"]))
  , _amount(j["amount"].number_value())
  , _incorporation(j["incorporation"].bool_value())
{}

json11::Json OrganicFertiliserApplication::to_json() const
{
  return json11::Json::object {
    {"type", "OrganicFertiliserApplication"},
    {"date", date().toIsoDateString()},
    {"amount", _amount},
    {"parameters", _params->to_json()},
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
    _depth(double_value(j, "depth"))
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
    _amount(double_value(j, "amount")),
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

WSPtr Monica::makeWorkstep(json11::Json j)
{
  string type = string_value(j["type"]);
  if(type == "Seed")
    return make_shared<Seed>(j);
  else if(type == "Harvest")
    return make_shared<Harvest>(j);
  else if(type == "Cutting")
    return make_shared<Cutting>(j);
  else if(type == "MineralFertiliserApplication")
    return make_shared<MineralFertiliserApplication>(j);
  else if(type == "OrganicFertiliserApplication")
    return make_shared<OrganicFertiliserApplication>(j);
  else if(type == "TillageApplication")
    return make_shared<TillageApplication>(j);
  else if(type == "IrrigationApplication")
    return make_shared<IrrigationApplication>(j);
}

CultivationMethod::CultivationMethod(const string& name)
  : _name(name)
{}

CultivationMethod::CultivationMethod(CropPtr crop, const std::string& name)
  : _name(name.empty() ? crop->speciesName() + "/" + crop->cultivarName() : name),
    _crop(crop),
    _cropResult(new PVResult(crop->id()))
{
  debug() << "ProductionProcess: " << name.c_str() << endl;

  if(crop->seedDate().isValid())
    addApplication(Seed(crop->seedDate(), _crop));
	
  if(crop->harvestDate().isValid())
  {
    debug() << "crop->harvestDate(): " << crop->harvestDate().toString().c_str() << endl;
    addApplication(Harvest(crop->harvestDate(), _crop, _cropResult));
  }

  for(Date cd : crop->getCuttingDates())
	{
    debug() << "Add cutting date: " << cd.toString() << endl;
    addApplication(Cutting(cd));
	}
}

CultivationMethod::CultivationMethod(json11::Json j)
  : _customId(int_value(j, "customId"))
  , _name(string_value(j, "name"))
  , _crop(new Crop(j["crop"]))
  , _irrigateCrop(bool_value(j, "irrigateCrop"))
{
  for(auto ws : j["worksteps"].array_items())
    insert(make_pair(Date::fromIsoDateString(string_value(ws[0])), makeWorkstep(ws[1])));
}

json11::Json CultivationMethod::to_json() const
{
  auto wss = J11Array();
  for(auto d2ws : *this)
    wss.push_back(J11Array {d2ws.first.toIsoDateString(), d2ws.second->to_json()});

  return J11Object {
    {"type", "ProductionProcess"},
    {"customId", _customId},
    {"name", _name},
    {"crop", _crop->to_json()},
    {"irrigateCrop", _irrigateCrop},
    {"worksteps", wss}};
}

void CultivationMethod::apply(const Date& date, MonicaModel* model) const
{
  auto p = equal_range(date);
  while (p.first != p.second)
  {
    p.first->second->apply(model);
    p.first++;
  }
}

Date CultivationMethod::nextDate(const Date& date) const
{
  auto ci = upper_bound(date);
  return ci != end() ? ci->first : Date();
}

Date CultivationMethod::startDate() const
{
  if(empty())
    return Date();
  return begin()->first;
}

Date CultivationMethod::endDate() const
{
  if(empty())
    return Date();
  return rbegin()->first;
}

std::string CultivationMethod::toString() const
{
  ostringstream s;
  s << "name: " << name()
    << " start: " << startDate().toString()
    << " end: " << endDate().toString() << endl;
  s << "worksteps:" << endl;
  for(auto p : *this)
    s << "at: " << p.first.toString()
      << " what: " << p.second->toString() << endl;
  return s.str();
}

//----------------------------------------------------------------------------
