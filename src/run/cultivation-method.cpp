/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

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
{
	merge(j);
}

Errors WorkStep::merge(json11::Json j)
{
	set_iso_date_value(_date, j, "date");
	return{};
}

json11::Json WorkStep::to_json() const
{
	return json11::Json::object{
		{"type", type()},
		{"date", date().toIsoDateString()},};
}

//------------------------------------------------------------------------------

Seed::Seed(const Tools::Date& at, CropPtr crop)
	: WorkStep(at)
	, _crop(crop)
{
	if(_crop)
		_crop->setSeedDate(at);
}

Seed::Seed(json11::Json j)
{
	merge(j);
}

Errors Seed::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);
	set_shared_ptr_value(_crop, j, "crop");
	if(_crop)
		_crop->setSeedDate(date());

	return res;
}

json11::Json Seed::to_json(bool includeFullCropParameters) const
{
	return json11::Json::object{
		{"type", type()},
		{"date", date().toIsoDateString()},
		{"crop", _crop->to_json(includeFullCropParameters)}};
}

void Seed::apply(MonicaModel* model)
{
	debug() << "seeding crop: " << _crop->toString() << " at: " << date().toString() << endl;
	model->seedCrop(_crop);
}

//------------------------------------------------------------------------------

Harvest::Harvest()
	: _method("total")
{}

Harvest::Harvest(const Tools::Date& at,
								 CropPtr crop,
								 CMResultPtr cropResult,
								 std::string method)
	: WorkStep(at)
	, _crop(crop)
	, _cropResult(cropResult)
	, _method(method)
{
	if(_crop)
		_crop->setHarvestDate(at);
}

Harvest::Harvest(json11::Json j)
	: _method("total")
{
	merge(j);
}

Errors Harvest::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);
	set_shared_ptr_value(_crop, j, "crop");
	//this is dangerous as this is actually a value object,
	//but one could think later, that it should actually refer to
	//the shared crop (between CultivationMethod, MonicaModel, Seed, Harvest etc.)
	if(_crop)
		_crop->setHarvestDate(date());
	_cropResult = make_shared<CMResult>(_crop ? _crop->id() : "");
	set_string_value(_method, j, "method");
	set_double_value(_percentage, j, "percentage");
	set_bool_value(_exported, j, "exported");

	return res;
}

json11::Json Harvest::to_json(bool includeFullCropParameters) const
{
	return json11::Json::object{
		{"type", type()},
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

		if(_method == "total"
			 || _method == "fruitHarvest"
			 || _method == "cutting")
		{
			debug() << "harvesting crop: " << crop->toString() << " at: " << date().toString() << endl;
			crop->setHarvestYields(model->cropGrowth()->get_FreshPrimaryCropYield() / 100.0,
														 model->cropGrowth()->get_FreshSecondaryCropYield() / 100.0);
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
			_cropResult->results[primaryYield] = crop->primaryYield();
			_cropResult->results[secondaryYield] = crop->secondaryYield();
			_cropResult->results[primaryYieldTM] = crop->primaryYieldTM();
			_cropResult->results[secondaryYieldTM] = crop->secondaryYieldTM();
			_cropResult->results[sumIrrigation] = crop->appliedIrrigationWater();
			_cropResult->results[biomassNContent] = crop->primaryYieldN();
			_cropResult->results[aboveBiomassNContent] = crop->aboveGroundBiomasseN();
			_cropResult->results[aboveGroundBiomass] = crop->aboveGroundBiomass();
			_cropResult->results[daysWithCrop] = model->daysWithCrop();
			_cropResult->results[sumTotalNUptake] = crop->sumTotalNUptake();
			_cropResult->results[cropHeight] = crop->cropHeight();
			_cropResult->results[sumETaPerCrop] = crop->get_AccumulatedETa();
			_cropResult->results[sumTraPerCrop] = crop->get_AccumulatedTranspiration();
			_cropResult->results[cropname] = crop->dbId();
			_cropResult->results[NStress] = model->getAccumulatedNStress();
			_cropResult->results[WaterStress] = model->getAccumulatedWaterStress();
			_cropResult->results[HeatStress] = model->getAccumulatedHeatStress();
			_cropResult->results[OxygenStress] = model->getAccumulatedOxygenStress();
			_cropResult->results[anthesisDay] = crop->getAnthesisDay();
			_cropResult->results[soilMoist0_90cmAtHarvest] = model->mean90cmWaterContent();
			_cropResult->results[corg0_30cmAtHarvest] = model->avgCorg(0.3);
			_cropResult->results[nmin0_90cmAtHarvest] = model->sumNmin(0.9);

			if(_method == "total")
				model->harvestCurrentCrop(_exported);
			else if(_method == "fruitHarvest")
				model->fruitHarvestCurrentCrop(_percentage, _exported);
			else if(_method == "cutting")
				model->cuttingCurrentCrop(_percentage, _exported);
		}
		else if(_method == "leafPruning")
		{
			debug() << "pruning leaves of: " << crop->toString() << " at: " << date().toString() << endl;
			model->leafPruningCurrentCrop(_percentage, _exported);
		}
		else if(_method == "tipPruning")
		{
			debug() << "pruning tips of: " << crop->toString() << " at: " << date().toString() << endl;
			model->tipPruningCurrentCrop(_percentage, _exported);
		}
		else if(_method == "shootPruning")
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
{
	merge(j);
}

Errors Cutting::merge(json11::Json j)
{
	return WorkStep::merge(j);
}

json11::Json Cutting::to_json() const
{
	return json11::Json::object{
		{"type", type()},
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

MineralFertiliserApplication::
MineralFertiliserApplication(const Tools::Date& at,
														 MineralFertiliserParameters partition,
														 double amount)
	: WorkStep(at)
	, _partition(partition)
	, _amount(amount)
{}

MineralFertiliserApplication::MineralFertiliserApplication(json11::Json j)
{
	merge(j);
}

Errors MineralFertiliserApplication::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);
	set_value_obj_value(_partition, j, "partition");
	set_double_value(_amount, j, "amount");
	return res;
}

json11::Json MineralFertiliserApplication::to_json() const
{
	return json11::Json::object{
		{"type", type()},
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

OrganicFertiliserApplication::
OrganicFertiliserApplication(const Tools::Date& at,
														 OrganicMatterParametersPtr params,
														 double amount,
														 bool incorp)
	: WorkStep(at)
	, _params(params)
	, _amount(amount)
	, _incorporation(incorp)
{}

OrganicFertiliserApplication::OrganicFertiliserApplication(json11::Json j)
{
	merge(j);
}

Errors OrganicFertiliserApplication::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);
	set_shared_ptr_value(_params, j, "parameters");
	set_double_value(_amount, j, "amount");
	set_bool_value(_incorporation, j, "incorporation");
	return res;
}

json11::Json OrganicFertiliserApplication::to_json() const
{
	return json11::Json::object{
		{"type", type()},
		{"date", date().toIsoDateString()},
		{"amount", _amount},
		{"parameters", _params ? _params->to_json() : ""},
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
	: WorkStep(at)
	, _depth(depth)
{}

TillageApplication::TillageApplication(json11::Json j)
{
	merge(j);
}

Errors TillageApplication::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);
	set_double_value(_depth, j, "depth");
	return res;
}

json11::Json TillageApplication::to_json() const
{
	return json11::Json::object{
		{"type", type()},
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
	: WorkStep(at)
	, _amount(amount)
	, _params(params)
{}

IrrigationApplication::IrrigationApplication(json11::Json j)
{
	merge(j);
}

Errors IrrigationApplication::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);
	set_double_value(_amount, j, "amount");
	set_value_obj_value(_params, j, "parameters");
	return res;
}

json11::Json IrrigationApplication::to_json() const
{
	return json11::Json::object{
		{"type", type()},
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

	return WSPtr();
}

CultivationMethod::CultivationMethod(const string& name)
	: _name(name)
{}

CultivationMethod::CultivationMethod(CropPtr crop,
																		 const std::string& name)
	: _name(name.empty() ? crop->id() : name)
	, _crop(crop)
	, _cropResult(new CMResult(crop->id()))
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
{
	merge(j);
}

Errors CultivationMethod::merge(json11::Json j)
{
	Errors res;

	set_int_value(_customId, j, "customId");
	set_string_value(_name, j, "name");
	//set_shared_ptr_value(_crop, j, "crop");
	set_bool_value(_irrigateCrop, j, "irrigateCrop");

	for(auto wsj : j["worksteps"].array_items())
	{
		auto ws = makeWorkstep(wsj);
		insert(make_pair(iso_date_value(wsj, "date"), ws));
		string wsType = ws->type();
		if(wsType == "Seed")
		{
			if(Seed* seed = dynamic_cast<Seed*>(ws.get()))
			{
				_crop = seed->crop();
				if(_name.empty() && _crop)
				{
					_name = _crop->id();
				}
			}
		}
		else if(wsType == "Harvest")
		{
			if(Harvest* harvest = dynamic_cast<Harvest*>(ws.get()))
			{
				_cropResult = harvest->cropResult();
			}
		}
	}

	return res;
}

json11::Json CultivationMethod::to_json() const
{
	auto wss = J11Array();
	for(auto d2ws : *this)
		wss.push_back(d2ws.second->to_json());

	return J11Object{
		{"type", "CultivationMethod"},
		{"customId", _customId},
    {"name", _name},
		//    {"crop", _crop->to_json()},
    {"irrigateCrop", _irrigateCrop},
    {"worksteps", wss}};
}

void CultivationMethod::apply(const Date& date, MonicaModel* model) const
{
	auto p = equal_range(date);
	while(p.first != p.second)
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

