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

	set_iso_date_value(_earliestDate, j, "earliest-date");
	set_iso_date_value(_latestDate, j, "latest-date");
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
	{
		_crop->setSeedDate(date());
		if(j["is-winter-crop"].is_bool())
			_crop->setIsWinterCrop(j["is-winter-crop"].bool_value());
	}

	return res;
}

json11::Json AutomaticSowing::to_json(bool includeFullCropParameters) const
{
	return json11::Json::object
	{{"type", type()}
	,{"earliest-date", J11Array{_earliestDate.toIsoDateString(), "", "earliest sowing date"}}
	,{"latest-date", J11Array{_latestDate.toIsoDateString(), "", "latest sowing date"}}
	,{"min-temp", J11Array{_minTempThreshold, "°C", "minimal air temperature for sowing (T >= thresh && avg T in Twindow >= thresh)"}}
	,{"days-in-temp-window", J11Array{_daysInTempWindow, "d", "days to be used for sliding window of min-temp"}}
	,{"min-%-asw", J11Array{_minPercentASW, "%", "minimal soil-moisture in percent of available soil-water"}}
	,{"max-%-asw", J11Array{_maxPercentASW, "%", "maximal soil-moisture in percent of available soil-water"}}
	,{"max-3d-precip-sum", J11Array{_max3dayPrecipSum, "mm", "sum of precipitation in the last three days (including current day)"}}
	,{"max-curr-day-precip", J11Array{_maxCurrentDayPrecipSum, "mm", "max precipitation allowed at current day"}}
	,{"temp-sum-above-base-temp", J11Array{_tempSumAboveBaseTemp, "°C", "temperature sum above T-base needed"}}
	,{"base-temp", J11Array{_baseTemp, "°C", "base temperature above which temp-sum-above-base-temp is counted"}}
	,{"is-winter-crop", _crop ? _crop->isWinterCrop() : json11::Json()}
	,{"crop", _crop ? _crop->to_json(includeFullCropParameters) : json11::Json()}
	};
}

bool isSoilMoistureOk(MonicaModel* model, 
											double minPercentASW, 
											double maxPercentASW)
{
	bool soilMoistureOk = false;
	double pwp = model->soilColumn().at(0).vs_PermanentWiltingPoint();
	double sm = max(0.0, model->soilColumn().at(0).get_Vs_SoilMoisture_m3() - pwp);
	double asw = model->soilColumn().at(0).vs_FieldCapacity() - pwp;
	double currentPercentASW = sm / asw * 100.0;
	soilMoistureOk = minPercentASW <= currentPercentASW && currentPercentASW <= maxPercentASW;

	return soilMoistureOk;
}

bool isPrecipitationOk(const std::vector<std::map<Climate::ACD, double>>& climateData, 
											 double max3dayPrecipSum, 
											 double maxCurrentDayPrecipSum)
{
	bool precipOk = false;
	double psum3d = accumulate(climateData.rbegin(), climateData.rbegin() + 3, 0.0,
														 [](double acc, const map<ACD, double>& d)
	{
		auto it = d.find(Climate::precip);
		return acc + (it == d.end() ? 0 : it->second);
	});
	double currentp = climateData.back().at(Climate::precip);
	precipOk = psum3d <= max3dayPrecipSum && currentp <= maxCurrentDayPrecipSum;

	return precipOk;
}

void AutomaticSowing::apply(MonicaModel* model)
{
	if(_cropSeeded)
		return;

	auto currentDate = model->currentStepDate();

	auto seed = [&]()
	{
		model->seedCrop(_crop);
		_crop->setSeedDate(currentDate);
		model->addEvent("AutomaticSowing");
		model->addEvent("automatic-sowing");
		_cropSeeded = true;
		debug() << "automatically sowing crop: " << _crop->toString() << " at: " << currentDate.toString() << endl;
	};

	auto earliestDate = _earliestDate.isRelativeDate() ? _earliestDate.toAbsoluteDate(currentDate.year()) : _earliestDate;
	auto latestDate = _latestDate.isRelativeDate() ? _latestDate.toAbsoluteDate(currentDate.year()) : _latestDate;

	if(!_inSowingRange && currentDate < earliestDate)
		return;
	else
		_inSowingRange = true;

	if(_inSowingRange && currentDate >= latestDate)
	{
		seed();
		_inSowingRange = false;
		return;
	}

	const auto& cd = model->climateData();
	auto currentCd = cd.back();
	
	auto avg = [&](Climate::ACD acd)
	{
		return accumulate(cd.rbegin(), cd.rbegin() + _daysInTempWindow, 0.0,
											[acd](double acc, const map<ACD, double>& d)
		{
			auto it = d.find(acd);
			return acc + (it == d.end() ? 0 : it->second);
		}) / min(int(cd.size()), _daysInTempWindow);
	};

	//check temperature
	bool Tok = false;
	if(_crop->isWinterCrop())
	{
		double avgTavg = avg(Climate::tavg);
		Tok = avgTavg <= _minTempThreshold;
	}
	else
	{
		double avgTmin = avg(Climate::tmin);
		bool avgTminOk = avgTmin >= _minTempThreshold;
		bool TminOk = currentCd[Climate::tmin] >= _minTempThreshold;
		Tok = avgTminOk && TminOk;
	}
	
	if(!Tok)
		return;

	//check soil moisture
	if(!isSoilMoistureOk(model, _minPercentASW, _maxPercentASW))
		return;

	//check precipitation
	if(!isPrecipitationOk(cd, _max3dayPrecipSum, _maxCurrentDayPrecipSum))
		return;

	//check temperature sum
	double baseTemp = _baseTemp;
	double tempSum = accumulate(cd.begin(), cd.end(), 0.0,
															[baseTemp](double acc, const map<ACD, double>& d)
	{
		auto it = d.find(Climate::tavg);
		return acc + (it == d.end() ? 0 : max(0.0, it->second - baseTemp));
	});
	if(tempSum < _tempSumAboveBaseTemp)
		return;
	
	seed();
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

AutomaticHarvesting::AutomaticHarvesting()
	: Harvest()
	, _harvestTime("maturity")
{}

AutomaticHarvesting::AutomaticHarvesting(CropPtr crop,
																				 std::string harvestTime,
																				 Date latestHarvest,
																				 std::string method)
	: Harvest(Date(), crop, method)
	, _harvestTime(harvestTime)
	, _latestDate(latestHarvest)
{}

AutomaticHarvesting::AutomaticHarvesting(json11::Json j)
	: Harvest(j)
	, _harvestTime("maturity")
{
	merge(j);
}

Errors AutomaticHarvesting::merge(json11::Json j)
{
	Errors res = Harvest::merge(j);

	set_iso_date_value(_latestDate, j, "latest-date");
	set_double_value(_minPercentASW, j, "min-%-asw");
	set_double_value(_maxPercentASW, j, "max-%-asw");
	set_double_value(_max3dayPrecipSum, j, "max-3d-precip");
	set_double_value(_maxCurrentDayPrecipSum, j, "max-curr-day-precip");
	set_string_value(_harvestTime, j, "harvest-time");

	return res;
}

json11::Json AutomaticHarvesting::to_json(bool includeFullCropParameters) const
{
	auto o = Harvest::to_json(includeFullCropParameters).object_items();
	o["type"] = type();
	o["latest-date"] = J11Array{_latestDate.toIsoDateString(), "", "latest harvesting date"};
	o["min-%-asw"] = J11Array{_minPercentASW, "%", "minimal soil-moisture in percent of available soil-water"};
	o["max-%-asw"], J11Array{_maxPercentASW, "%", "maximal soil-moisture in percent of available soil-water"};
	o["max-3d-precip-sum"], J11Array{_max3dayPrecipSum, "mm", "sum of precipitation in the last three days (including current day)"};
	o["max-curr-day-precip"], J11Array{_maxCurrentDayPrecipSum, "mm", "max precipitation allowed at current day"};
	o["harvest-time"] = _harvestTime;
	return o;
}

void AutomaticHarvesting::apply(MonicaModel* model)
{
	//no crop or already harvested
	if(!model->cropGrowth() 
		 || _cropHarvested)
		return;

	//check for maturity
	bool isMaturityReached = _harvestTime == "maturity" && model->cropGrowth()->maturityReached();
	if(!isMaturityReached)
		return;
	
	auto currentDate = model->currentStepDate();

	auto harvest = [&]()
	{
		Harvest::apply(model);
		model->addEvent("AutomaticHarvesting");
		model->addEvent("automatic-harvesting");
		model->addEvent("harvesting");
		_cropHarvested = true;
		debug() << "automatically harvesting crop: " << crop()->toString() << " at: " << currentDate.toString() << endl;
	};

	//make dates comparable
	auto sd = model->currentCrop()->seedDate();
	auto seedDate = sd.isRelativeDate() ? sd.toAbsoluteDate(currentDate.year()) : sd;
	auto latestDate = _latestDate.isRelativeDate() ? _latestDate.toAbsoluteDate(currentDate.year()) : _latestDate;
	
	//check if user forgot to add one year to winter crop's latest seed date, if the date was relative
	if(seedDate > latestDate)
		latestDate.addYears(1);

	//harvest after or at latested date
	if(currentDate >= latestDate)
	{
		harvest();
		return;
	}
	
	//check soil moisture
	if(!isSoilMoistureOk(model, _minPercentASW, _maxPercentASW))
		return;

	//check precipitation
	if(!isPrecipitationOk(model->climateData(), _max3dayPrecipSum, _maxCurrentDayPrecipSum))
		return;

	harvest();
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

NDemandMineralFertiliserApplication::
NDemandMineralFertiliserApplication(int stage, 
																		double depth,
																		MineralFertiliserParameters partition,
																		double amount)
	: MineralFertiliserApplication(Date(), partition, amount)
	, _depth(depth)
	, _stage(stage)
{}

NDemandMineralFertiliserApplication::NDemandMineralFertiliserApplication(json11::Json j)
{
	merge(j);
}

Errors NDemandMineralFertiliserApplication::merge(json11::Json j)
{
	Errors res = MineralFertiliserApplication::merge(j);
	set_double_value(_depth, j, "depth");
	set_int_value(_stage, j, "stage");
	return res;
}

json11::Json NDemandMineralFertiliserApplication::to_json() const
{
	auto o = MineralFertiliserApplication::to_json().object_items();
	o["type"] = type();
	o["depth"] = J11Array{_depth, "m", "depth of Nmin measurement"};
	o["stage"] = J11Array{_stage, "", "if this development stage is entered, the fertilizer will be applied"};
	return o;
}

void NDemandMineralFertiliserApplication::apply(MonicaModel* model)
{
	auto cg = model->cropGrowth();
	auto cc = model->currentCrop();
	if(_appliedFertilizer 
		 || !cg 
		 || !cc)
		return;

	auto cps = model->currentCrop()->cropParameters();
	
	auto applyFertilizer = [&](double amount)
	{
		debug() << toString() << endl;

		/*
		const NMinUserParameters& ups = model->simPs.p_NMinUserParams;
		return model->soilColumnNC().applyMineralFertiliserViaNMinMethod(partition(),
																																		 _depth < 0.01 ? cps->speciesParams.pc_SamplingDepth : _depth,
																																		 cps->speciesParams.pc_TargetNSamplingDepth,
																																		 cps->speciesParams.pc_TargetN30,
																																		 ups.min,
																																		 ups.max,
																																		 ups.delayInDays);
	  */
		model->applyMineralFertiliser(partition(), amount);
		_appliedFertilizer = true;
		model->addEvent("NDemandMineralFertiliserApplication");
		model->addEvent("N-demand-mineral-fertilizing");
	};
	

	//apply at seeding time
	if(_stage == 0)
	{
		applyFertilizer(amount());
		return;
	}

	auto currStage = model->cropGrowth()->get_DevelopmentalStage() + 1;
	if(currStage == _stage)
	{



	}



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
	else if(type == "AutomaticSowing")
		return make_shared<AutomaticSowing>(j);
	else if(type == "Harvest")
		return make_shared<Harvest>(j);
	else if(type == "AutomaticHarvesting")
		return make_shared<AutomaticHarvesting>(j);
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
				if((_name.empty() || _name == "Fallow") && _crop)
				{
					_name = _crop->id();
				}
			}
		}
		else if(wsType == "AutomaticSowing")
		{
			if(AutomaticSowing* seed = dynamic_cast<AutomaticSowing*>(ws.get()))
			{
				_crop = seed->crop();
				if((_name.empty() || _name == "Fallow") && _crop)
				{
					_name = _crop->id();
				}
			}
		}
		else if(wsType == "Harvest" 
						|| wsType == "AutomaticHarvesting")
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
}

void CultivationMethod::apply(MonicaModel* model) const
{
	//check if dynamic worksteps can be applied
	for(auto wsp : applicationsAt(Date()))
			wsp->apply(model);
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
	auto it = begin();
	while(it != end() && !it->first.isValid())
		it++;
	return it->first;
}

Date CultivationMethod::endDate() const
{
	if(empty())
		return Date();
	auto it = rbegin();
	while(it != rend() && !it->first.isValid())
		it++;
	return it->first;
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

void CultivationMethod::reset()
{
	for(auto p : *this)
		p.second->reset();
}
