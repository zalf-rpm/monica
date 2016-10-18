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

	es.append(extractAndStore(j["cropRotation"], cropRotation));
	es.append(extractAndStore(j["dailyOutputIds"], dailyOutputIds));
	es.append(extractAndStore(j["monthlyOutputIds"], monthlyOutputIds));
	es.append(extractAndStore(j["yearlyOutputIds"], yearlyOutputIds));
	for(auto& p : j["atOutputIds"].object_items())
		es.append(extractAndStore(p.second, atOutputIds[Date::fromIsoDateString(p.first)]));
	es.append(extractAndStore(j["cropOutputIds"], cropOutputIds));
	es.append(extractAndStore(j["runOutputIds"], runOutputIds));

	set_bool_value(debugMode, j, "debugMode");
	
	set_string_value(pathToClimateCSV, j, "pathToClimateCSV");
	csvViaHeaderOptions = j["csvViaHeaderOptions"];

	set_string_value(customId, j, "customId");

	return es;
}

json11::Json Env::to_json() const
{
	J11Array cr;
	for(const auto& c : cropRotation)
		cr.push_back(c.to_json());

	J11Object aoids;
	for(auto p : atOutputIds)
		aoids[p.first.toIsoDateString()] = p.second;

	return json11::Json::object{
		 {"type", "Env"}
		,{"params", params.to_json()}
		,{"cropRotation", cr}
		,{"dailyOutputIds", dailyOutputIds}
		,{"monthlyOutputIds", monthlyOutputIds}
		,{"yearlyOutputIds", yearlyOutputIds}
		,{"atOutputIds", aoids}
		,{"cropOutputIds", cropOutputIds}
		,{"runOutputIds", runOutputIds}
		,{"da", da.to_json()}
		,{"debugMode", debugMode}
		,{"pathToClimateCSV", pathToClimateCSV}
		,{"csvViaHeaderOptions", csvViaHeaderOptions}
		,{"customId", customId}
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

//--------------------------------------------------------------------------------------

pair<Date, map<Climate::ACD, double>>
climateDataForStep(const Climate::DataAccessor& da, size_t stepNo)
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

	map<Climate::ACD, double> m{
		{ Climate::tmin, tmin },
	{ Climate::tavg, tavg },
	{ Climate::tmax, tmax },
	{ Climate::precip, precip },
	{ Climate::wind, wind },
	{ Climate::globrad, globrad },
	{ Climate::relhumid, relhumid }};
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
	pout << "{" << endl;
	auto cropPs = env.params.userCropParameters.to_json().dump();
	pout << "\"userCropParameters\":" << endl << cropPs << "," << endl;
	auto simPs = env.params.simulationParameters.to_json().dump();
	pout << "\"simulationParameters\":" << endl << simPs << "," << endl;
	auto envPs = env.params.userEnvironmentParameters.to_json().dump();
	pout << "\"userEnvironmentParameters\":" << endl << envPs << "," << endl;
	auto smPs = env.params.userSoilMoistureParameters.to_json().dump();
	pout << "\"userSoilMoistureParameters\":" << endl << smPs << "," << endl;
	auto soPs = env.params.userSoilOrganicParameters.to_json().dump();
	pout << "\"userSoilOrganicParameters\":" << endl << soPs << "," << endl;
	auto stempPs = env.params.userSoilTemperatureParameters.to_json().dump();
	pout << "\"userSoilTemperatureParameters\":" << endl << stempPs << "," << endl;
	auto stransPs = env.params.userSoilTransportParameters.to_json().dump();
	pout << "\"userSoilTransportParameters\":" << endl << stransPs << "," << endl;
	auto sitePs = env.params.siteParameters.to_json().dump();
	pout << "\"siteParameters\":" << endl << sitePs << "," << endl;
	pout << "\"crop-rotation\": {" << endl;
	bool isFirstItem = true;
	for(auto cm : env.cropRotation)
	{
		pout
			<< (isFirstItem ? "  " : ", ")
			<< "\"" << (cm.crop() ? cm.crop()->id() : "NULL crop") << "\":" << cm.to_json().dump() << endl;
		isFirstItem = !isFirstItem && isFirstItem;
	}
	pout << "}," << endl;
	pout << "\"climate-data\": [" << endl;
	isFirstItem = true;
	for(size_t i = 0, nosp = env.da.noOfStepsPossible(); i < nosp; i++)
	{
		pout
			<< (isFirstItem ? "  " : ", ")
			<< "[\"" << env.da.dateForStep(i).toIsoDateString()
			<< "\", " << env.da.dataForTimestep(tavg, i)
			<< ", " << env.da.dataForTimestep(precip, i)
			<< ", " << env.da.dataForTimestep(globrad, i)
			<< "]" << endl;
		isFirstItem = !isFirstItem && isFirstItem;
	}
	pout << "]" << endl;
	pout << "}" << endl;
	pout.flush();
	pout.close();
}


//-----------------------------------------------------------------------------

void storeResults(const vector<OId>& outputIds,
									vector<BOTRes::ResultVector>& results,
									MonicaRefs& mr,
									int timestep,
									Date currentDate)
{
	const auto& ofs = buildOutputTable().ofs;

	auto mrr = MonicaRefs(mr);
	mrr.timestep = timestep;
	mrr.currentDate = currentDate;
	mrr.mcg = mrr.monica->cropGrowth();
	mrr.cropPlanted = mrr.monica->isCropPlanted();

	size_t i = 0;
	results.resize(outputIds.size());
	for(auto oid : outputIds)
	{
		auto ofi = ofs.find(oid.id);
		if(ofi != ofs.end())
			ofi->second(mrr, results[i], oid);
		++i;
	}
};

Result Monica::runMonica(Env env)
{
	activateDebug = env.debugMode;
	if(activateDebug)
	{
		writeDebugInputs(env, "inputs.json");
	}

	Result res;
	res.customId = env.customId;
	res.out.customId = env.customId;

	if(env.cropRotation.empty())
	{
		debug() << "Error: Crop rotation is empty!" << endl;
		return res;
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

	vector<BOTRes::ResultVector> intermediateMonthlyResults;
	vector<BOTRes::ResultVector> intermediateYearlyResults;
	vector<BOTRes::ResultVector> intermediateRunResults;
	vector<BOTRes::ResultVector> intermediateCropResults;
	
	MonicaRefs monicaRefs
	{
		&monica, &monica.soilTemperature(), &monica.soilMoisture(), &monica.soilOrganic(), &monica.soilColumn(),
		&monica.soilTransport(), monica.cropGrowth(), monica.isCropPlanted(), env.da, 0, currentDate
	};

	//if for some reason there are no applications (no nothing) in the
	//production process: quit
	if(!nextAbsoluteCMApplicationDate.isValid())
	{
		debug() << "start of production-process: " << currentCM.toString()
			<< " is not valid" << endl;
		return res;
	}

	auto aggregateCropOutput = [&]()
	{
		size_t i = 0;
		auto& vs = res.out.crop[monica.currentCrop()->id()];
		vs.resize(intermediateCropResults.size());
		for(auto oid : env.cropOutputIds)
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
			currentCM.apply(nextCMApplicationDate, &monica, {{"Harvest", aggregateCropOutput}});

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
				//get yieldresults for crop
				CMResult r = currentCM.cropResult();
				r.customId = currentCM.customId();
				r.date = currentDate;

				if(!env.params.simulationParameters.p_UseSecondaryYields)
					r.results[secondaryYield] = 0;
				r.results[sumFertiliser] = monica.sumFertiliser();
				r.results[daysWithCrop] = monica.daysWithCrop();
				r.results[NStress] = monica.getAccumulatedNStress();
				r.results[WaterStress] = monica.getAccumulatedWaterStress();
				r.results[HeatStress] = monica.getAccumulatedHeatStress();
				r.results[OxygenStress] = monica.getAccumulatedOxygenStress();

				res.pvrs.push_back(r);
				//        debug() << "py: " << r.pvResults[primaryYield] << endl;
				//            << " sy: " << r.pvResults[secondaryYield]
				//            << " iw: " << r.pvResults[sumIrrigation]
				//            << " sf: " << monica.sumFertiliser()
				//            << endl;

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

		const auto& dateAndClimateDataP = climateDataForStep(env.da, d);

		// run crop step
		if(monica.isCropPlanted())
			monica.cropStep(currentDate, dateAndClimateDataP.second);
		
		monica.generalStep(currentDate, dateAndClimateDataP.second);

		//-------------------------------------------------------------------------
		//store results
		
		//daily results
		//----------------
		storeResults(env.dailyOutputIds, res.out.daily, monicaRefs, d, currentDate);

		//crop results
		//----------------
		if(monica.isCropPlanted())
			storeResults(env.cropOutputIds,
									 intermediateCropResults,
									 monicaRefs,
									 d, currentDate);

		//at (a certain time) results
		//-------------------
		//try to find exact date
		auto ati = env.atOutputIds.find(currentDate);
		//is not exact date, try to find relative one
		if(ati == env.atOutputIds.end())
			ati = env.atOutputIds.find(currentDate.toRelativeDate());
		if(ati != env.atOutputIds.end())
			storeResults(ati->second, res.out.at[ati->first], monicaRefs, d, currentDate);

		if(currentDate.month() != currentMonth 
			 || d == nods - 1)
		{
			size_t i = 0;
			res.out.monthly[currentMonth].resize(intermediateMonthlyResults.size());
			for(auto oid : env.monthlyOutputIds)
			{
				if(!intermediateMonthlyResults.empty())
				{
					res.out.monthly[currentMonth][i].push_back(applyOIdOP(oid.timeAggOp, intermediateMonthlyResults.at(i)));
					intermediateMonthlyResults[i].clear();
				}
				++i;
			}

			currentMonth = currentDate.month();
		}
		else
			storeResults(env.monthlyOutputIds, intermediateMonthlyResults, monicaRefs, d, currentDate);

		//yearly results 
		//------------------
		if(currentDate.year() != (currentDate - 1).year() 
			 && d > 0)
		{
			size_t i = 0;
			res.out.yearly.resize(intermediateYearlyResults.size());
			for(auto oid : env.yearlyOutputIds)
			{
				if(!intermediateYearlyResults.empty())
				{
					res.out.yearly[i].push_back(applyOIdOP(oid.timeAggOp, intermediateYearlyResults.at(i)));
					intermediateYearlyResults[i].clear();
				}
				++i;
			}
		}
		else
			storeResults(env.yearlyOutputIds, intermediateYearlyResults, monicaRefs, d, currentDate);

		//(whole) run results 
		storeResults(env.runOutputIds, intermediateRunResults, monicaRefs, d, currentDate);
	}

	//store/aggregate results for a single run
	size_t i = 0;
	res.out.run.resize(env.runOutputIds.size());
	for(auto oid : env.runOutputIds)
	{
		if(!intermediateRunResults.empty())
			res.out.run[i] = applyOIdOP(oid.timeAggOp, intermediateRunResults.at(i));
		++i;
	}
	
	debug() << "returning from runMonica" << endl;
	return res;
}
