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

#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <set>
#include <sstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <thread>
#include <tuple>
#include <limits>

#include "run-monica.h"
#include "tools/debug.h"
#include "climate/climate-common.h"
#include "db/abstract-db-connections.h"
#include "tools/json11-helper.h"
#include "tools/algorithms.h"
#include "../io/build-output.h"

using namespace Monica;
using namespace std;
using namespace Climate;
using namespace Tools;
using namespace Soil;
using namespace json11;

//-----------------------------------------------------------------------------

Env::Env(CentralParameterProvider cpp)
	: params(cpp)
{}

Env::Env(json11::Json j)
{
	merge(j);
}

namespace
{
	template<typename Vector>
	Errors extractAndStore(Json jv, Vector& vec)
	{
		Errors es;
		vec.clear();
		for(Json cmj : jv.array_items())
		{
			typename Vector::value_type v;
			es.append(v.merge(cmj));
			vec.push_back(v);
		}
		return es;
	}
}

Errors Env::merge(json11::Json j)
{
	Errors es;

	es.append(params.merge(j["params"]));

	es.append(da.merge(j["da"]));

	events = j["events"];
	outputs = j["outputs"];

	es.append(extractAndStore(j["cropRotation"], cropRotation));
	
	set_bool_value(debugMode, j, "debugMode");
	
	set_string_value(pathToClimateCSV, j, "pathToClimateCSV");
	csvViaHeaderOptions = j["csvViaHeaderOptions"];

	set_string_value(customId, j, "customId");
	events = j["events"];

	return es;
}

json11::Json Env::to_json() const
{
	J11Array cr;
	for(const auto& c : cropRotation)
		cr.push_back(c.to_json());

	return json11::Json::object{
		 {"type", "Env"}
		,{"params", params.to_json()}
		,{"cropRotation", cr}
		,{"da", da.to_json()}
		,{"debugMode", debugMode}
		,{"pathToClimateCSV", pathToClimateCSV}
		,{"csvViaHeaderOptions", csvViaHeaderOptions}
		,{"customId", customId}
		,{"events", events}
		,{"outputs", outputs}
	};
}

string Env::toString() const
{
	ostringstream s;
	s << " noOfLayers: " << params.simulationParameters.p_NumberOfLayers
		<< " layerThickness: " << params.simulationParameters.p_LayerThickness
		<< endl;
	s << "ClimateData: from: " << da.startDate().toString()
		<< " to: " << da.endDate().toString() << endl;
	s << "Fruchtfolge: " << endl;
	for(const CultivationMethod& cm : cropRotation)
		s << cm.toString() << endl;
	s << "customId: " << customId;
	return s.str();
}

/**
* Interface method for python wrapping, so climate module
* does not need to be wrapped by python.
*
* @param acd
* @param data
*/
void
Env::addOrReplaceClimateData(std::string name, const std::vector<double>& data)
{
	int acd = 0;
	if(name == "tmin")
		acd = tmin;
	else if(name == "tmax")
		acd = tmax;
	else if(name == "tavg")
		acd = tavg;
	else if(name == "precip")
		acd = precip;
	else if(name == "globrad")
		acd = globrad;
	else if(name == "wind")
		acd = wind;
	else if(name == "sunhours")
		acd = sunhours;
	else if(name == "relhumid")
		acd = relhumid;

	da.addOrReplaceClimateData(AvailableClimateData(acd), data);
}

/**
* Returns the result vector of a special output. Python swig is
* not able to wrap stl-maps correctly. Stl-vectors are working
* properly so this function has been implemented to use the results
* in wrapped python code.
*
* @param id ResultId of output
* @return Vector of result values
*/
/*
std::vector<double> Result::getResultsById(int id)
{
	// test if crop results are requested
	if(id == primaryYield || id == secondaryYield || id == sumIrrigation ||
		 id == sumFertiliser || id == biomassNContent || id == sumTotalNUptake ||
		 id == cropHeight || id == cropname || id == sumETaPerCrop || sumTraPerCrop ||
		 id == primaryYieldTM || id == secondaryYieldTM || id == daysWithCrop || id == aboveBiomassNContent ||
		 id == NStress || id == WaterStress || id == HeatStress || id == OxygenStress || id == aboveGroundBiomass ||
		 id == anthesisDay || id == maturityDay || id == harvestDay ||
		 id == soilMoist0_90cmAtHarvest || id == corg0_30cmAtHarvest || id == nmin0_90cmAtHarvest)
	{
		vector<double> result_vector;
		int size = pvrs.size();
		for(int i = 0; i < size; i++)
		{
			CMResult crop_result = pvrs.at(i);
			result_vector.push_back(crop_result.results[(ResultId)id]);
		}
		return result_vector;
	}

	return generalResults[(ResultId)id];
}


std::string Result::toString()
{
	ostringstream s;
	map<ResultId, std::vector<double> >::iterator it;
	// show content:
	for(it = generalResults.begin(); it != generalResults.end(); it++)
	{
		ResultId id = (*it).first;
		std::vector<double> data = (*it).second;
		s << resultIdInfo(id).shortName.c_str() << ":\t" << data.at(data.size() - 1) << endl;
	}

	return s.str();
}
//*/

//--------------------------------------------------------------------------------------

pair<Date, map<Climate::ACD, double>> climateDataForStep(const Climate::DataAccessor& da,
																												 size_t stepNo)
{
	Date startDate = da.startDate();
	Date currentDate = startDate + stepNo;
	double tmin = da.dataForTimestep(Climate::tmin, stepNo);
	double tavg = da.dataForTimestep(Climate::tavg, stepNo);
	double tmax = da.dataForTimestep(Climate::tmax, stepNo);
	double precip = da.dataForTimestep(Climate::precip, stepNo);
	double wind = da.dataForTimestep(Climate::wind, stepNo);
	double globrad = da.dataForTimestep(Climate::globrad, stepNo);

	// test if data for relhumid are available; if not, value is set to -1.0
	double relhumid = da.hasAvailableClimateData(Climate::relhumid)
		? da.dataForTimestep(Climate::relhumid, stepNo)
		: -1.0;

	map<Climate::ACD, double> m
	{{ Climate::tmin, tmin }
	,{ Climate::tavg, tavg }
	,{ Climate::tmax, tmax }
	,{ Climate::precip, precip }
	,{ Climate::wind, wind }
	,{ Climate::globrad, globrad }
	,{ Climate::relhumid, relhumid }
	};
	return make_pair(currentDate, m);
}

void writeDebugInputs(const Env& env, string fileName = "inputs.json")
{
	ofstream pout;
	string pathToFile = fixSystemSeparator(ensureDirExists(env.params.pathToOutputDir()) + "/" + fileName);
	pout.open(pathToFile);
	if(pout.fail())
	{
		cerr << "Error couldn't open file: '" << pathToFile << "'." << endl;
		return;
	}
	pout << env.to_json().dump() << endl;
	pout.flush();
	pout.close();
}


//-----------------------------------------------------------------------------

void storeResults(const vector<OId>& outputIds,
									vector<J11Array>& results,
									const MonicaModel& monica)
{
	const auto& ofs = buildOutputTable().ofs;

	size_t i = 0;
	results.resize(outputIds.size());
	for(auto oid : outputIds)
	{
		auto ofi = ofs.find(oid.id);
		if(ofi != ofs.end())
			ofi->second(monica, results[i], oid);
		++i;
	}
};

//-----------------------------------------------------------------------------

void StoreData::storeResultsIfSpecApplies(const MonicaModel& monica)
{
	auto cd = monica.currentStepDate();

	auto y = cd.year();
	auto m = cd.month();
	auto d = cd.day();

	//check if we are in the start/end range or no year, month, day specified
	if(spec.start.value().year.isValue() && y < spec.start.value().year.value()
		 || spec.end.value().year.isValue() && y > spec.end.value().year.value())
		return;
	if(spec.start.value().month.isValue() && m < spec.start.value().month.value()
		 || spec.end.value().month.isValue() && m > spec.end.value().month.value())
		return;
	if(spec.start.value().day.isValue() && d < spec.start.value().day.value()
		 || spec.end.value().day.isValue() && d > spec.end.value().day.value())
		return;
	
	//at spec takes precedence over range spec, if both would be set
	if(spec.isAt())
	{
		if((spec.at.value().year.isNothing() || y == spec.at.value().year.value())
			 && (spec.at.value().month.isNothing() || m == spec.at.value().month.value())
			 && (spec.at.value().day.isNothing()
					 || d == spec.at.value().day.value()
					 || (spec.at.value().day.isValue() && d < spec.at.value().day.value() && d == cd.daysInMonth())))
		{
			storeResults(outputIds, results, monica);
		}
	}
	//spec.at.isValue() can also mean "xxxx-xx-xx" = daily values
	else if(spec.at.isValue())
	{
		storeResults(outputIds, results, monica);
	}
	else
	{
		//check if we are in the aggregating from/to range
		if((spec.from.value().year.isNothing() || y >= spec.from.value().year.value())
			 && (spec.to.value().year.isNothing() || y <= spec.to.value().year.value()))
		{
			if((spec.from.value().month.isNothing() || m >= spec.from.value().month.value())
				 && (spec.to.value().month.isNothing() || m <= spec.to.value().month.value()))
			{
				if((spec.from.value().day.isNothing() || d >= spec.from.value().day.value())
					 && (spec.to.value().day.isNothing() || d <= spec.to.value().day.value()))
				{
					storeResults(outputIds, intermediateResults, monica);

					//if on last day of range or last day in month (even if month has less than 31 days (= marker for end of month))
					//aggregate intermediate values
					if((spec.to.value().year.isNothing() || y == spec.to.value().year.value())
						 && (spec.to.value().month.isNothing() || m == spec.to.value().month.value())
						 && ((spec.to.value().day.isValue() && d == spec.to.value().day.value())
								 || (spec.to.value().day.isValue() && d < spec.to.value().day.value() && d == cd.daysInMonth())))
					{
						size_t i = 0;
						results.resize(intermediateResults.size());
						for(auto oid : outputIds)
						{
							if(!intermediateResults.empty())
							{
								auto& ivs = intermediateResults.at(i);
								if(ivs.front().is_string())
								{
									switch(oid.timeAggOp)
									{
									case OId::FIRST: results[i].push_back(ivs.front()); break;
									case OId::LAST: results[i].push_back(ivs.back()); break;
									default: results[i].push_back(ivs.front());
									}
								}
								else
									results[i].push_back(applyOIdOP(oid.timeAggOp, ivs));

								intermediateResults[i].clear();
							}
							++i;
						}
					}
				}
			}
		}
	}
}

vector<StoreData> setupStorage(json11::Json event2oids, Date startDate, Date endDate)
{
	map<string, Json> shortcuts = 
	{{"daily", J11Object{{"at", "xxxx-xx-xx"}}}
	,{"monthly", J11Object{{"from", "xxxx-xx-01"}, {"to", "xxxx-xx-31"}}}
	,{"yearly", J11Object{{"from", "xxxx-01-01"}, {"to", "xxxx-12-31"}}}
	,{"run", J11Object{{"from", startDate.toIsoDateString()}, {"to", endDate.toIsoDateString()}}}
	};

	vector<StoreData> storeData;

	auto e2os = event2oids.array_items();
	for(size_t i = 0, size = e2os.size(); i < size; i+=2)
	{
		StoreData sd;
		sd.spec.origSpec = e2os[i];
		Json spec = sd.spec.origSpec;
		if(spec.is_string())
		{
			auto ss = spec.string_value();
			auto ci = shortcuts.find(ss);
			if(ci != shortcuts.end())
				spec = ci->second;
		}
		else if(!spec.is_object())
			continue;
		sd.spec.merge(spec);
		sd.outputIds = parseOutputIds(e2os[i+1].array_items());
		
		storeData.push_back(sd);
	}

	return storeData;
}

Output Monica::runMonica(Env env)
{
	activateDebug = env.debugMode;
	if(activateDebug)
	{
		writeDebugInputs(env, "inputs.json");
	}

	Output out;
	out.customId = env.customId;

	if(env.cropRotation.empty())
	{
		debug() << "Error: Crop rotation is empty!" << endl;
		return out;
	}

	debug() << "starting Monica" << endl;
	debug() << "-----" << endl;

	MonicaModel monica(env.params);

	debug() << "currentDate" << endl;
	Date currentDate = env.da.startDate();
	size_t nods = env.da.noOfStepsPossible();
	debug() << "nods: " << nods << endl;

	size_t currentMonth = currentDate.month();
	unsigned int dim = 0; //day in current month

	//iterator through the production processes
	vector<CultivationMethod>::const_iterator cmci = env.cropRotation.begin();
	//direct handle to current process
	CultivationMethod currentCM = *cmci;
	//are the dates in the production process relative dates
	//or are they absolute as produced by the hermes inputs
	bool useRelativeDates = currentCM.startDate().isRelativeDate();
	//the next application date, either a relative or an absolute date
	//to get the correct applications out of the production processes
	Date nextCMApplicationDate = currentCM.startDate();
	//a definitely absolute next application date to keep track where
	//we are in the list of climate data
	Date nextAbsoluteCMApplicationDate = useRelativeDates
		? nextCMApplicationDate.toAbsoluteDate(currentDate.year())// + 1) // + 1 probably due to use in DSS and have one year to init monica 
		: nextCMApplicationDate;
	debug() << "next app-date: " << nextCMApplicationDate.toString()
		<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
	bool currentCropIsPlanted = false;

	vector<J11Array> intermediateMonthlyResults;
	vector<J11Array> intermediateYearlyResults;
	vector<J11Array> intermediateRunResults;
	vector<J11Array> intermediateCropResults;

	std::vector<OId> dailyOutputIds = parseOutputIds(env.outputs["daily"].array_items());
	std::vector<OId> monthlyOutputIds = parseOutputIds(env.outputs["monthly"].array_items());
	std::vector<OId> yearlyOutputIds = parseOutputIds(env.outputs["yearly"].array_items());
	std::vector<OId> runOutputIds = parseOutputIds(env.outputs["run"].array_items());
	std::vector<OId> cropOutputIds = parseOutputIds(env.outputs["crop"].array_items());
	std::map<Tools::Date, std::vector<OId>> atOutputIds;
	if(env.outputs["at"].is_object())
	{
		for(auto p : env.outputs["at"].object_items())
		{
			Date d = Date::fromIsoDateString(p.first);
			if(d.isValid())
				atOutputIds[d] = parseOutputIds(p.second.array_items());
		}
	}

	vector<StoreData> store = setupStorage(env.events, env.da.startDate(), env.da.endDate());

	//if for some reason there are no applications (no nothing) in the
	//production process: quit
	if(!nextAbsoluteCMApplicationDate.isValid())
	{
		debug() << "start of production-process: " << currentCM.toString()
			<< " is not valid" << endl;
		return out;
	}

	//*
	auto aggregateCropOutput = [&]()
	{
		size_t i = 0;
		auto& vs = out.crop[monica.currentCrop()->id()];
		vs.resize(intermediateCropResults.size());
		for(auto oid : cropOutputIds)
		{
			if(!intermediateCropResults.empty())
			{
				auto& ivs = intermediateCropResults.at(i);
				if(ivs.front().is_string())
				{
					switch(oid.timeAggOp)
					{
					case OId::FIRST: vs[i].push_back(ivs.front()); break;
					case OId::LAST: vs[i].push_back(ivs.back()); break;
					default: vs[i].push_back(ivs.front());
					}
				}
				else
					vs[i].push_back(applyOIdOP(oid.timeAggOp, ivs));
				intermediateCropResults[i].clear();
			}
			++i;
		}
	};
	//*/
	
	//beware: !!!! if there are absolute days used, then there is basically
	//no rotation if the last crop in the crop rotation has changed
	//the loop starts anew but the first crops date has already passed
	//so the crop won't be seeded again or any work applied
	//thus for absolute dates the crop rotation has to be as long as there
	//are climate data !!!!!

	for(unsigned int d = 0; d < nods; ++d, ++currentDate, ++dim)
	{
		debug() << "currentDate: " << currentDate.toString() << endl;
		monica.resetDailyCounter();

		//    if (currentDate.year() == 2012) {
		//        cout << "Reaching problem year :-)" << endl;
		//    }
		// test if monica's crop has been dying in previous step
		// if yes, it will be incorporated into soil
		if(monica.cropGrowth() && monica.cropGrowth()->isDying())
			monica.incorporateCurrentCrop();

		/////////////////////////////////////////////////////////////////
		// AUTOMATIC HARVEST TRIGGER
		/////////////////////////////////////////////////////////////////

		/**
		* @TODO Change passing of automatic trigger parameters when building crop rotation (specka).
		* The automatic harvest trigger is passed globally to the method that reads in crop rotation
		* via hermes files because it cannot be configured crop specific with the HERMES format.
		* The harvest trigger prevents the adding of a harvest application as done in the normal case
		* that uses hard coded harvest data configured via the rotation file.
		*
		* When using the Json format, for each crop individual settings can be specified. The automatic
		* harvest trigger should be one of those options. Don't forget to pass a crop-specific latest
		* harvest date via json parameters too, that is now specified in the sqlite database globally
		* for each crop.
		*/

		// Test if automatic harvest trigger is used
		if(monica.cropGrowth() && currentCM.crop()->useAutomaticHarvestTrigger())
		{
			// Test if crop should be harvested at maturity
			if(currentCM.crop()->getAutomaticHarvestParams().getHarvestTime() == AutomaticHarvestParameters::maturity)
			{

				if(monica.cropGrowth()->maturityReached()
					 || currentCM.crop()->getAutomaticHarvestParams().getLatestHarvestDOY() == currentDate.julianDay())
				{
					debug() << "####################################################" << endl;
					debug() << "AUTOMATIC HARVEST TRIGGER EVENT" << endl;
					debug() << "####################################################" << endl;

					aggregateCropOutput();

					//auto harvestApplication = make_unique<Harvest>(currentDate, currentPP.crop(), currentPP.cropResultPtr());
					auto harvestApplication =
						unique_ptr<Harvest>(new Harvest(currentDate,
																						currentCM.crop(),
																						currentCM.cropResultPtr()));
					harvestApplication->apply(&monica);
				}
			}
		}

		//apply worksteps and cycle through crop rotation
		if(nextAbsoluteCMApplicationDate == currentDate)
		{
			debug() << "applying at: " << nextCMApplicationDate.toString()
				<< " absolute-at: " << nextAbsoluteCMApplicationDate.toString() << endl;
			//apply everything to do at current day
			//cout << currentPP.toString().c_str() << endl;
			currentCM.apply(nextCMApplicationDate, &monica);// , {{"Harvest", aggregateCropOutput}});

			//get the next application date to wait for (either absolute or relative)
			Date prevPPApplicationDate = nextCMApplicationDate;

			nextCMApplicationDate = currentCM.nextDate(nextCMApplicationDate);

			nextAbsoluteCMApplicationDate = useRelativeDates
				? nextCMApplicationDate.toAbsoluteDate
				(currentDate.year()
				 + (nextCMApplicationDate.dayOfYear() > prevPPApplicationDate.dayOfYear()
						? 0
						: 1),
				 true)
				: nextCMApplicationDate;

			debug() << "next app-date: " << nextCMApplicationDate.toString()
				<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
			//if application date was not valid, we're (probably) at the end
			//of the application list of this production process
			//-> go to the next one in the crop rotation


			if(!nextAbsoluteCMApplicationDate.isValid())
			{
				//to count the applied fertiliser for the next production process
				monica.resetFertiliserCounter();

				//resets crop values for use in next year
				currentCM.crop()->reset();

				cmci++;
				//start anew if we reached the end of the crop rotation
				if(cmci == env.cropRotation.end())
					cmci = env.cropRotation.begin();

				currentCM = *cmci;
				nextCMApplicationDate = currentCM.startDate();
				nextAbsoluteCMApplicationDate =
					useRelativeDates ? nextCMApplicationDate.toAbsoluteDate
					(currentDate.year() + (nextCMApplicationDate.dayOfYear() > prevPPApplicationDate.dayOfYear() ? 0 : 1),
					 true) : nextCMApplicationDate;
				debug() << "new valid next app-date: " << nextCMApplicationDate.toString()
					<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
			}
			//if we got our next date relative it might be possible that
			//the actual relative date belongs into the next year
			//this is the case if we're already (dayOfYear) past the next dayOfYear
			if(useRelativeDates && currentDate > nextAbsoluteCMApplicationDate)
				nextAbsoluteCMApplicationDate.addYears(1);
		}

		monica.step(currentDate, climateDataForStep(env.da, d).second);

		//-------------------------------------------------------------------------
		//store results
		
		for(auto& s : store)
			s.storeResultsIfSpecApplies(monica);

		//*
		//daily results
		//----------------
		storeResults(dailyOutputIds, out.daily, monica);

		//crop results
		//----------------
		if(monica.isCropPlanted())
			storeResults(cropOutputIds,
									 intermediateCropResults,
									 monica);

		//at (a certain time) results
		//-------------------
		//try to find exact date
		auto ati = atOutputIds.find(currentDate);
		//is not exact date, try to find relative one
		if(ati == atOutputIds.end())
			ati = atOutputIds.find(currentDate.toRelativeDate());
		if(ati != atOutputIds.end())
			storeResults(ati->second, out.at[ati->first], monica);

		if(currentDate.month() != currentMonth 
			 || d == nods - 1)
		{
			size_t i = 0;
			out.monthly[currentMonth].resize(intermediateMonthlyResults.size());
			for(auto oid : monthlyOutputIds)
			{
				if(!intermediateMonthlyResults.empty())
				{
					out.monthly[currentMonth][i].push_back(applyOIdOP(oid.timeAggOp, intermediateMonthlyResults.at(i)));
					intermediateMonthlyResults[i].clear();
				}
				++i;
			}

			currentMonth = currentDate.month();
		}
		else
			storeResults(monthlyOutputIds, intermediateMonthlyResults, monica);

		//yearly results 
		//------------------
		if(currentDate.year() != (currentDate - 1).year() 
			 && d > 0)
		{
			size_t i = 0;
			out.yearly.resize(intermediateYearlyResults.size());
			for(auto oid : yearlyOutputIds)
			{
				if(!intermediateYearlyResults.empty())
				{
					out.yearly[i].push_back(applyOIdOP(oid.timeAggOp, intermediateYearlyResults.at(i)));
					intermediateYearlyResults[i].clear();
				}
				++i;
			}
		}
		else
			storeResults(yearlyOutputIds, intermediateYearlyResults, monica);

		//(whole) run results 
		storeResults(runOutputIds, intermediateRunResults, monica);
		//*/
	}

	//*
	//store/aggregate results for a single run
	size_t i = 0;
	out.run.resize(runOutputIds.size());
	for(auto oid : runOutputIds)
	{
		if(!intermediateRunResults.empty())
			out.run[i] = applyOIdOP(oid.timeAggOp, intermediateRunResults.at(i));
		++i;
	}
	//*/
	
	for(const auto& sd : store)
	{
		auto os = sd.spec.origSpec.dump();
		out.origSpec2oids[os] = sd.outputIds;
		out.origSpec2results[os] = sd.results;
	}
	
	debug() << "returning from runMonica" << endl;
	return out;
}
