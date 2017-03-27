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
	else if(name == "co2")
		acd = co2;

	da.addOrReplaceClimateData(AvailableClimateData(acd), data);
}

//--------------------------------------------------------------------------------------

/*
pair<Date, map<Climate::ACD, double>> 
climateDataForStep(const Climate::DataAccessor& da,
									 size_t stepNo,
									 double latitude)
{
	Date startDate = da.startDate();
	Date currentDate = startDate + stepNo;

	// test if data for relhumid are available; if not, value is set to -1.0
	double relhumid = da.hasAvailableClimateData(Climate::relhumid)
		? da.dataForTimestep(Climate::relhumid, stepNo)
		: -1.0;

	double globrad = da.hasAvailableClimateData(Climate::globrad)
		? da.dataForTimestep(Climate::globrad, stepNo)
		: (da.hasAvailableClimateData(Climate::sunhours)
			 ? Tools::sunshine2globalRadiation(currentDate.julianDay(),
																				 da.dataForTimestep(Climate::sunhours, stepNo),
																				 latitude, true)
			 : -1.0);
	
	map<Climate::ACD, double> m
	{{ Climate::tmin, da.dataForTimestep(Climate::tmin, stepNo)}
	,{ Climate::tavg, da.dataForTimestep(Climate::tavg, stepNo)}
	,{ Climate::tmax, da.dataForTimestep(Climate::tmax, stepNo)}
	,{ Climate::precip, da.dataForTimestep(Climate::precip, stepNo)}
	,{ Climate::wind, da.dataForTimestep(Climate::wind, stepNo)}
	,{ Climate::globrad, globrad}
	,{ Climate::relhumid, relhumid }
	};
	return make_pair(currentDate, m);
}
*/

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

template<typename T = int>
Maybe<T> parseInt(const string& s)
{
	Maybe<T> res;
	if(s.empty() || (s.size() > 1 && s.substr(0, 2) == "xx"))
		return res;
	else
	{
		try { res = stoi(s); }
		catch(exception e) {}
	}
	return res;
}


Tools::Errors Spec::merge(json11::Json j)
{
	init(start, j, "start");
	init(end, j, "end");
	init(at, j, "at");
	init(from, j, "from");
	init(to, j, "to");
	Maybe<DMY> dummy;
	init(dummy, j, "while");

	return{};
}

void Spec::init(Maybe<DMY>& member, Json j, string time)
{
	auto jt = j[time];
	
	//is an expression event
	if(jt.is_array())
	{
		if(auto f = buildCompareExpression(jt.array_items()))
		{
			time2expression[time] = f;
			if(eventType == eUnset)
				eventType = eExpression;
		}
	}
	else if(jt.is_string())
	{
		auto jts = jt.string_value();
		if(!jts.empty())
		{
			auto s = splitString(jts, "-");
			//is date event
			if(jts.size() == 10
				 && s.size() == 3
				 && s[0].size() == 4
				 && s[1].size() == 2
				 && s[2].size() == 2)
			{
				DMY dmy;
				dmy.year = parseInt(s[0]);
				dmy.month = parseInt<size_t>(s[1]);
				dmy.day = parseInt<size_t>(s[2]);
				member = dmy;
				eventType = eDate;
			}
			//treat all other strings as potential workstep event
			else
			{
				time2event[time] = jts;
				eventType = eCrop;
			}
		}
	}
}

json11::Json Spec::to_json() const
{
	auto padDM = [](int i){ return (i < 10 ? string("0") : string()) + to_string(i); };
	auto padY = [](int i)
	{ return (i < 10
						? string("0")
						: (i < 100
							 ? string("00")
							 : (i < 1000
									? string("000")
									: string())))
		+ to_string(i); };

	J11Object o{{"type", "Spec"}};

	auto extendMsgBy = [&](const Maybe<DMY>& member, string time)
	{
		if(member.isValue())
		{
			auto s = string("")
				+ (member.value().year.isValue() ? padY(member.value().year.value()) : "xxxx")
				+ (member.value().month.isValue() ? padDM(member.value().month.value()) : "xx")
				+ (member.value().day.isValue() ? padDM(member.value().day.value()) : "xx");
			if(s != "xxxx-xx-xx")
				o[time] = s;
		}
	};

	extendMsgBy(start, "start");
	extendMsgBy(end, "end");
	extendMsgBy(at, "at");
	extendMsgBy(from, "from"); 
	extendMsgBy(to, "to");
	
	return o;
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
			results[i].push_back(ofi->second(monica, oid));
		++i;
	}
};

//-----------------------------------------------------------------------------

void StoreData::aggregateResults()
{
	if(!intermediateResults.empty())
	{
		if(results.size() < intermediateResults.size())
			results.resize(intermediateResults.size());

		assert(intermediateResults.size() == outputIds.size());

		size_t i = 0;
		for(auto oid : outputIds)
		{
			auto& ivs = intermediateResults.at(i);
			if(!ivs.empty())
			{
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

void StoreData::storeResultsIfSpecApplies(const MonicaModel& monica)
{
	string os = spec.origSpec.dump();

	switch(spec.eventType)
	{
	case Spec::eExpression:
	{
		bool isCurrentlyEndEvent = false;
		if(!spec.time2expression.empty())
		{
			//check and possibly set start/end markers
			if(withinEventStartEndRange.isNothing() || !withinEventStartEndRange.value())
			{
				if(auto f = spec.time2expression["start"])
					withinEventStartEndRange = f(monica);
			}
			else if(withinEventStartEndRange.isValue())
			{
				if(auto f = spec.time2expression["end"])
					isCurrentlyEndEvent = f(monica);
			}

			//do something if we are in start/end range or nothing is set at all (means do it always)
			if(withinEventStartEndRange.isNothing() || withinEventStartEndRange.value())
			{
				//check for at event
				auto af = spec.time2expression["at"];
				if(af && af(monica))
				{
					storeResults(outputIds, results, monica);
				}
				//or while event
				else if(auto wf = spec.time2expression["while"])
				{
					if(wf(monica))
					{
						storeResults(outputIds, intermediateResults, monica);
					}
					else if(!intermediateResults.empty()
									&& !intermediateResults.front().empty())
					{
						//if while event was not successful but we got intermediate results, they should be aggregated
						aggregateResults();
					}
				}
				//or from/to range event
				else
				{
					bool isCurrentlyToEvent = false;
					if(withinEventFromToRange.isNothing() || !withinEventFromToRange.value())
					{
						if(auto f = spec.time2expression["from"])
							withinEventFromToRange = f(monica);
					}
					else if(withinEventFromToRange.isValue())
					{
						if(auto f = spec.time2expression["to"])
							isCurrentlyToEvent = f(monica);
					}

					if(withinEventFromToRange.value())
					{
						storeResults(outputIds, intermediateResults, monica);

						if(isCurrentlyToEvent)
						{
							aggregateResults();
							withinEventFromToRange = false;
						}

						if(isCurrentlyEndEvent)
							withinEventStartEndRange = false;
					}
				}
			}
		}
	}
	break;
	case Spec::eCrop:
	{
		bool isCurrentlyEndEvent = false;
		const auto& currentEvents = monica.currentEvents();
		if(!spec.time2event.empty() || !currentEvents.empty())
		{
			//set possibly start/end markers
			if(withinEventStartEndRange.isNothing() || !withinEventStartEndRange.value())
			{
				auto s = spec.time2event["start"];
				if(!s.empty())
				{
					if(currentEvents.find(s) != currentEvents.end())
						withinEventStartEndRange = true;
				}
			}
			else if(withinEventStartEndRange.isValue())
			{
				auto e = spec.time2event["end"];
				if(!e.empty())
				{
					if(currentEvents.find(e) != currentEvents.end())
						isCurrentlyEndEvent = true;
				}
			}

			//is at event
			auto a = spec.time2event["at"];
			if(!a.empty() && currentEvents.find(a) != currentEvents.end())
			{
				storeResults(outputIds, results, monica);
			}
			//is from/to event
			else
			{
				bool isCurrentlyToEvent = false;
				if(withinEventFromToRange.isNothing() || !withinEventFromToRange.value())
				{
					auto f = spec.time2event["from"];
					if(!f.empty())
					{
						if(currentEvents.find(f) != currentEvents.end())
							withinEventFromToRange = true;
					}
				}
				else if(withinEventFromToRange.isValue())
				{
					auto t = spec.time2event["to"];
					if(!t.empty())
					{
						if(currentEvents.find(t) != currentEvents.end())
							isCurrentlyToEvent = true;
					}
				}

				if(withinEventStartEndRange.isNothing() || withinEventStartEndRange.value())
				{
					if(withinEventFromToRange.value())
					{
						// allow an while expression in an eCrop section
						if(auto wf = spec.time2expression["while"])
						{
							if(wf(monica))
							{
								storeResults(outputIds, intermediateResults, monica);
							}
						}
						else
							storeResults(outputIds, intermediateResults, monica);

						if(isCurrentlyToEvent)
						{
							aggregateResults();
							withinEventFromToRange = false;
						}

						if(isCurrentlyEndEvent)
							withinEventStartEndRange = false;
					}
				}
			}
		}
	}
	break;
	case Spec::eDate:
	{
		auto cd = monica.currentStepDate();

		auto y = cd.year();
		auto m = cd.month();
		auto d = cd.day();

		if(spec.start.isValue() || spec.end.isValue())
		{
			//build dates to compare against
			auto sv = spec.start.value();
			Date start(sv.day.isNothing() ? d : sv.day.value(),
								 sv.month.isNothing() ? m : sv.month.value(),
								 sv.year.isNothing() ? y : sv.year.value(),
								 false, true);

			auto ev = spec.end.value();
			Date end(ev.day.isNothing() ? d : ev.day.value(),
							 ev.month.isNothing() ? m : ev.month.value(),
							 ev.year.isNothing() ? y : ev.year.value(),
							 false, true);

			//check if we are in the start/end range or no year, month, day specified
			if(cd < start || cd > end)
				return;
		}

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
			//build dates to compare against
			auto fv = spec.from.value();
			Date from(fv.day.isNothing() ? d : fv.day.value(),
								fv.month.isNothing() ? m : fv.month.value(),
								fv.year.isNothing() ? y : fv.year.value(),
								false, true);

			auto tv = spec.to.value();
			Date to(tv.day.isNothing() ? d : tv.day.value(),
							tv.month.isNothing() ? m : tv.month.value(),
							tv.year.isNothing() ? y : tv.year.value(),
							false, true);

			//check if we are in the aggregating from/to range
			if(from <= cd && cd <= to)
			{
				storeResults(outputIds, intermediateResults, monica);

				//if on last day of range or last day in month (even if month has less than 31 days (= marker for end of month))
				//aggregate intermediate values
				if(cd == to)
					aggregateResults();
			}
		}
	}
	break;
	default:;
	}
}

vector<StoreData> setupStorage(json11::Json event2oids, Date startDate, Date endDate)
{
	map<string, Json> shortcuts = 
	{{"daily", J11Object{{"at", "xxxx-xx-xx"}}}
	,{"monthly", J11Object{{"from", "xxxx-xx-01"}, {"to", "xxxx-xx-31"}}}
	,{"yearly", J11Object{{"from", "xxxx-01-01"}, {"to", "xxxx-12-31"}}}
	,{"run", J11Object{{"from", startDate.toIsoDateString()}, {"to", endDate.toIsoDateString()}}}
	,{"crop", J11Object{{"from", "Sowing"}, {"to", "Harvest"}}}
	};

	vector<StoreData> storeData;

	auto e2os = event2oids.array_items();
	for(size_t i = 0, size = e2os.size(); i < size; i+=2)
	{
		StoreData sd;
		sd.spec.origSpec = e2os[i];
		Json spec = sd.spec.origSpec;
		//find shortcut for string or store string as 'at' pattern
		if(spec.is_string())
		{
			auto ss = spec.string_value();
			auto ci = shortcuts.find(ss);
			if(ci != shortcuts.end())
				spec = ci->second;
			else 
				spec = J11Object{{"at", ss}};
		}
		//an array means it's an expression pattern to be stored at 'at' 
		else if(spec.is_array()
						&& spec.array_items().size() == 4
						&& spec[0].is_string()
						&& (spec[0].string_value() == "while"
								|| spec[0].string_value() == "at"))
		{
			auto sa = spec.array_items();
			spec = J11Object{{spec[0].string_value(), J11Array(sa.begin() + 1, sa.end())}};
		}
		else if(spec.is_array())
			spec = J11Object{{"at", spec}};
		//everything else (number, bool, null) we ignore
		//object is the default we assume
		else if(!spec.is_object())
			continue;
		sd.spec.merge(spec);
		sd.outputIds = parseOutputIds(e2os[i+1].array_items());
		
		storeData.push_back(sd);
	}

	return storeData;
}

void Monica::initPathToDB(const std::string& initialPathToIniFile)
{
	Db::dbConnectionParameters(initialPathToIniFile);
}

Output Monica::runMonica(Env env)
{
	Output out;
	out.customId = env.customId;

	activateDebug = env.debugMode;
	if(activateDebug)
	{
		writeDebugInputs(env, "inputs.json");
	}

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

	//cropRotation is a shadow of the env.cropRotation, which will hold pointers to CMs in env.cropRotation, but might shrink
	//if pure absolute CMs are finished
	vector<CultivationMethod*> cropRotation;
	for(auto& cm : env.cropRotation)
		cropRotation.push_back(&cm);

	//iterator through the crop rotation
	auto cmci = cropRotation.begin();
	
	//keep track of the year a cultivation method has been used in, to prevent initializing it again for the same year
	map<int, set<CultivationMethod*>> year2cm;
	
	//direct handle to current cultivation method
	CultivationMethod* currentCM = cmci == cropRotation.end() ? nullptr : *cmci;
	currentCM->reinit(currentDate);
	year2cm[currentDate.year()].insert(*cmci);

	Date nextAbsoluteCMApplicationDate = currentCM->staticWorksteps().empty() ? Date() : currentCM->absStartDate();

	vector<StoreData> store = setupStorage(env.events, env.da.startDate(), env.da.endDate());
	
	for(size_t d = 0, nods = env.da.noOfStepsPossible(); d < nods; ++d, ++currentDate)
	{
		debug() << "currentDate: " << currentDate.toString() << endl;

		monica.dailyReset();

		monica.setCurrentStepDate(currentDate);
		monica.setCurrentStepClimateData(env.da.allDataForStep(d, env.params.siteParameters.vs_Latitude));

		// test if monica's crop has been dying in previous step
		// if yes, it will be incorporated into soil
		if(monica.cropGrowth() && monica.cropGrowth()->isDying())
			monica.incorporateCurrentCrop();

		//try to apply dynamic worksteps
		if(currentCM)
			currentCM->apply(&monica);

		//apply worksteps and cycle through crop rotation
		if(currentCM && nextAbsoluteCMApplicationDate == currentDate)
		{
			debug() << "applying absolute-at: " << nextAbsoluteCMApplicationDate.toString() << endl;
			currentCM->absApply(nextAbsoluteCMApplicationDate, &monica);

			nextAbsoluteCMApplicationDate = currentCM->nextAbsDate(nextAbsoluteCMApplicationDate);
						
			debug() << " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
		}

		//monica main stepping method
		monica.step();

		//store results
		for(auto& s : store)
			s.storeResultsIfSpecApplies(monica);

		//if the next application date is not valid, we're at the end
		//of the application list of this cultivation method
		//and go to the next one in the crop rotation
		if(currentCM 
			 && currentCM->allDynamicWorkstepsFinished()
			 && !nextAbsoluteCMApplicationDate.isValid())
		{
			//to count the applied fertiliser for the next production process
			monica.resetFertiliserCounter();

			//delete fully cultivation methods with only absolute worksteps,
			//because they won't participate in a new run when wrapping the crop rotation 
			//delete fully cultivation methods with only absolute worksteps,
			//because they won't participate in a new run when wrapping the crop rotation 
			if((*cmci)->areOnlyAbsoluteWorksteps())
				cmci = cropRotation.erase(cmci);
			else
				cmci++;

			//start anew if we reached the end of the crop rotation
			if(cmci == cropRotation.end())
				cmci = cropRotation.begin();

			//check if there's at least a cultivation method left in cropRotation
			if(cmci != cropRotation.end())
			{
				//if the newly set cultivation method has already been used this year
				int year = currentDate.year();
				auto it = year2cm.find(year);
				if(it != year2cm.end())
				{
					if(it->second.find(*cmci) != it->second.end())
						year++;
				}

				currentCM = *cmci;
				currentCM->reinit(currentDate);
				year2cm[year].insert(*cmci);
				nextAbsoluteCMApplicationDate = currentCM->staticWorksteps().empty() ? Date() : currentCM->absStartDate();
				debug() << "new valid next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
			}
			else
			{
				currentCM = nullptr;
				nextAbsoluteCMApplicationDate = Date();
			}
		}
	}
	
	for(auto& sd : store)
	{
		//aggregate results of while events or unfinished other from/to ranges (where to event didn't happen yet)
		sd.aggregateResults();
		out.data.push_back({sd.spec.origSpec.dump(), sd.outputIds, sd.results});
	}

	debug() << "returning from runMonica" << endl;
	return out;
}
