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
#include "../core/monica-model.h"
#include "tools/debug.h"
#include "soil/conversion.h"
#include "soil/soil.h"
#include "../io/database-io.h"
#include "../io/build-output.h"

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

void WorkStep::apply(MonicaModel* model)
{
	model->addEvent("WorkStep");
	model->addEvent("workstep");
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
		{"crop", _crop ? _crop->to_json(includeFullCropParameters) : json11::Json()}};
}

void Seed::apply(MonicaModel* model)
{
	debug() << "seeding crop: " << _crop->toString() << " at: " << date().toString() << endl;
	model->seedCrop(_crop);
	model->addEvent("Seed");
	model->addEvent("seeding");
}

//------------------------------------------------------------------------------

AutomaticSowing::AutomaticSowing(const Tools::Date& at, CropPtr crop)
	: WorkStep()
	, _crop(crop)
{
	if(_crop)
		_crop->setSeedDate(at);
}

AutomaticSowing::AutomaticSowing(json11::Json j)
{
	merge(j);
}

Errors AutomaticSowing::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);

	set_iso_date_value(_minDate, j, "min-date");
	set_iso_date_value(_maxDate, j, "max-date");
	set_double_value(_minTempThreshold, j, "min-temp");
	set_int_value(_daysInTempWindow, j, "days-in-temp-window");
	set_double_value(_minPercentASW, j, "min-%-asw");
	set_double_value(_maxPercentASW, j, "max-%-asw");
	set_double_value(_max3dayPrecipSum, j, "max-3d-precip");
	set_double_value(_maxCurrentDayPrecipSum, j, "max-curr-day-precip");
	set_double_value(_tempSumAboveBaseTemp, j, "temp-sum-above-base-temp");
	set_double_value(_baseTemp, j, "base-temp");

	set_shared_ptr_value(_crop, j, "crop");
	if(_crop)
		_crop->setSeedDate(date());

	return res;
}

json11::Json AutomaticSowing::to_json(bool includeFullCropParameters) const
{
	return json11::Json::object
	{{"type", type()}
	,{"min-date", J11Array{_minDate.toIsoDateString(), "", "earliest sowing date"}}
	,{"max-date", J11Array{_maxDate.toIsoDateString(), "", "latest sowing date"}}
	,{"min-temp", J11Array{_minTempThreshold, "°C", "minimal air temperature for sowing (T >= thresh && avg T in Twindow >= thresh)"}}
	,{"days-in-temp-window", J11Array{_daysInTempWindow, "d", "days to be used for sliding window of min-temp"}}
	,{"min-%-asw", J11Array{_minPercentASW, "%", "minimal soil-moisture in percent of available soil-water"}}
	,{"max-%-asw", J11Array{_maxPercentASW, "%", "maximal soil-moisture in percent of available soil-water"}}
	,{"max-3d-precip-sum", J11Array{_max3dayPrecipSum, "mm", "sum of precipitation in the last three days (including current day)"}}
	,{"max-curr-day-precip", J11Array{_maxCurrentDayPrecipSum, "mm", "max precipitation allowed at current day"}}
	,{"temp-sum-above-base-temp", J11Array{_tempSumAboveBaseTemp, "°C", "temperature sum above T-base needed"}}
	,{"base-temp", J11Array{_baseTemp, "°C", "base temperature above which temp-sum-above-base-temp is counted"}}
	,{"crop", _crop ? _crop->to_json(includeFullCropParameters) : json11::Json()}
	};
}

void AutomaticSowing::apply(MonicaModel* model)
{
	debug() << "automatically sowing crop: " << _crop->toString() << " at: " << date().toString() << endl;

	auto currentDate = model->currentStepDate();
	if(currentDate < _minDate)
		return;

	if(currentDate == _maxDate)
	{
		model->seedCrop(_crop);
		return;
	}

	auto cd = model->climateData();
	double tavg = accumulate(cd.rbegin(), cd.rbegin() + _daysInTempWindow, 0.0, [](double acc, const map<ACD, double>& d)
	{
		auto it = d.find(Climate::tavg);
		return acc + (it == d.end() ? 0 : it->second);
	}) / min(int(cd.size()), _daysInTempWindow);





	model->seedCrop(_crop);
	model->addEvent("Seed");
	model->addEvent("seeding");
}

//------------------------------------------------------------------------------

Harvest::Harvest()
	: _method("total")
{}

Harvest::Harvest(const Tools::Date& at,
								 CropPtr crop,
								 std::string method)
	: WorkStep(at)
	, _crop(crop)
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

	set_string_value(_method, j, "method");
	set_double_value(_percentage, j, "percentage");
	set_bool_value(_exported, j, "exported");

	return res;
}

json11::Json Harvest::to_json(bool includeFullCropParameters) const
{
	return json11::Json::object
	{{"type", type()}
	,{"date", date().toIsoDateString()}
//,{"crop", _crop ? _crop->to_json(includeFullCropParameters) :json11::Json()}
	,{"method", _method}
	,{"percentage", _percentage}
	,{ "exported", _exported }
	};
}

void Harvest::apply(MonicaModel* model)
{
	if(model->cropGrowth())
	{
		auto crop = model->currentCrop();

		if(_method == "total"
			 || _method == "fruitHarvest"
			 || _method == "cutting")
		{
			debug() << "harvesting crop: " << crop->toString() << " at: " << date().toString() << endl;
			
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
		model->addEvent("Harvest");
		model->addEvent("harvesting");
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

AutomaticHarvest::AutomaticHarvest()
	: Harvest()
{}

AutomaticHarvest::AutomaticHarvest(CropPtr crop,
																	 std::string harvestTime,
																	 int latestHarvestDOY,
																	 std::string method)
	: Harvest(Date(), crop, method)
	, _harvestTime(harvestTime)
	, _latestHarvestDOY(latestHarvestDOY)
{
}

AutomaticHarvest::AutomaticHarvest(json11::Json j)
	: Harvest(j)
{
	merge(j);
}

Errors AutomaticHarvest::merge(json11::Json j)
{
	Errors res = Harvest::merge(j);

	set_string_value(_harvestTime, j, "harvestTime");
	set_int_value(_latestHarvestDOY, j, "latestHarvestDOY");

	return res;
}

json11::Json AutomaticHarvest::to_json(bool includeFullCropParameters) const
{
	auto o = Harvest::to_json(includeFullCropParameters).object_items();
	o["type"] = type();
	o["harvestTime"] = _harvestTime;
	o["latestHarvestDOY"] = _latestHarvestDOY;
	return o;
}

void AutomaticHarvest::apply(MonicaModel* model)
{
	if(model->cropGrowth() 
		 && _harvestTime == "maturity"
		 && (model->cropGrowth()->maturityReached()
				 || _latestHarvestDOY == model->currentStepDate().julianDay()))
	{
		Harvest::apply(model);
		model->addEvent("AutomaticHarvest");
		model->addEvent("automatic-harvesting");
		model->addEvent("harvesting");
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
	//crop->setHarvestYields(model->cropGrowth()->get_FreshPrimaryCropYield() / 100.0,
	//											 model->cropGrowth()->get_FreshSecondaryCropYield() / 100.0);

	//crop->setHarvestYieldsTM(model->cropGrowth()->get_PrimaryCropYield() / 100.0,
	//												 model->cropGrowth()->get_SecondaryCropYield() / 100.0);

	//crop->setYieldNContent(model->cropGrowth()->get_PrimaryYieldNContent(),
	//											 model->cropGrowth()->get_SecondaryYieldNContent());

	//crop->setSumTotalNUptake(model->cropGrowth()->get_SumTotalNUptake());
	//crop->setCropHeight(model->cropGrowth()->get_CropHeight());

	model->cropGrowth()->applyCutting();
	model->addEvent("Cutting");
	model->addEvent("cutting");
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
	model->addEvent("MineralFertiliserApplication");
	model->addEvent("mineral-fertilizing");
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
	model->addEvent("OrganicFertiliserApplication");
	model->addEvent("organic-fertilizing");
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
	model->addEvent("TillageApplication");
	model->addEvent("tillage");
}

//------------------------------------------------------------------------------

SetValue::SetValue(const Tools::Date& at,
									 OId oid,
									 json11::Json value)
	: WorkStep(at)
	, _oid(oid)
	, _value(value)
{}

SetValue::SetValue(json11::Json j)
{
	merge(j);
}

Errors SetValue::merge(json11::Json j)
{
	Errors res = WorkStep::merge(j);

	auto oids = parseOutputIds({j["var"]});
	if(!oids.empty())
		_oid = oids[0];
	else
		return res;

	_value = j["value"];
	if(_value.is_array())
	{
		auto jva = _value.array_items();
		if(!jva.empty())
		{
			//is an expression
			if(jva[0] == "=" && jva.size() == 4)
			{
				auto f = buildPrimitiveCalcExpression(J11Array(jva.begin() + 1, jva.end()));
				_getValue = [=](const MonicaModel* mm){ return f(*mm); };
			}
			else
			{
				auto oids = parseOutputIds({_value});
				if(!oids.empty())
				{
					auto oid = oids[0];
					const auto& ofs = buildOutputTable().ofs;
					auto ofi = ofs.find(oid.id);
					if(ofi != ofs.end())
					{
						auto f = ofi->second;
						_getValue = [=](const MonicaModel* mm){ return f(*mm, oid); };
					}
				}
			}
		}
	}
	else
		_getValue = [=](const MonicaModel*){ return _value; };

	return res;
}

json11::Json SetValue::to_json() const
{
	return json11::Json::object
	{{"type", type()}
	,{"date", date().toIsoDateString()}
	,{"var", _oid.jsonInput}
	,{"value", _value}
	};
}

void SetValue::apply(MonicaModel* model)
{
	if(!_getValue)
		return;

	const auto& setfs = buildOutputTable().setfs;
	auto ci = setfs.find(_oid.id);
	if(ci != setfs.end())
	{
		auto v = _getValue(model);
		ci->second(*model, _oid, v);
	}

	model->addEvent("SetValue");
	model->addEvent("set-value");
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
	model->addEvent("IrrigationApplication");
	model->addEvent("irrigation");
}

//------------------------------------------------------------------------------

WSPtr Monica::makeWorkstep(json11::Json j)
{
	string type = string_value(j["type"]);
	if(type == "Seed")
		return make_shared<Seed>(j);
	else if(type == "Harvest")
		return make_shared<Harvest>(j);
	else if(type == "AutomaticHarvest")
		return make_shared<AutomaticHarvest>(j);
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
	else if(type == "SetValue")
		return make_shared<SetValue>(j);

	return WSPtr();
}

CultivationMethod::CultivationMethod(const string& name)
	: _name(name)
{}

CultivationMethod::CultivationMethod(CropPtr crop,
																		 const std::string& name)
	: _name(name.empty() ? crop->id() : name)
	, _crop(crop)
{
	debug() << "CultivationMethod: " << name.c_str() << endl;

	if(crop->seedDate().isValid())
		addApplication(Seed(crop->seedDate(), _crop));

	if(crop->harvestDate().isValid())
	{
		debug() << "crop->harvestDate(): " << crop->harvestDate().toString().c_str() << endl;
		addApplication(Harvest(crop->harvestDate(), _crop));
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
	set_bool_value(_irrigateCrop, j, "irrigateCrop");

	for(auto wsj : j["worksteps"].array_items())
	{
		auto ws = makeWorkstep(wsj);
		if(!ws)
			continue;
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
				harvest->setCrop(_crop);
				_crop->setHarvestDate(harvest->date());
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

	return J11Object
	{{"type", "CultivationMethod"}
	,{"customId", _customId}
  ,{"name", _name}
  ,{"irrigateCrop", _irrigateCrop}
  ,{"worksteps", wss}
	};
}

void CultivationMethod::apply(const Date& date, 
															MonicaModel* model) const
{
	//apply everything at date
	for(auto wsp : applicationsAt(date))
		wsp->apply(model);

	//check if dynamic worksteps can be applied
	for(auto wsp : applicationsAt(Date()))
	{
		wsp->apply(model);
	}
}

Date CultivationMethod::nextDate(const Date& date) const
{
	auto ci = upper_bound(date);
	return ci != end() ? ci->first : Date();
}

vector<WSPtr> CultivationMethod::applicationsAt(const Date& date) const
{
	vector<WSPtr> apps;
	auto p = equal_range(date);
	while(p.first != p.second)
	{
		apps.push_back(p.first->second);
		p.first++;
	}
	return apps;
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

