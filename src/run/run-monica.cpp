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
#include "../core/crop-growth.h"

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

	es.append(climateData.merge(j["climateData"]));

	events = j["events"];
	outputs = j["outputs"];

	es.append(extractAndStore(j["cropRotation"], cropRotation));
	
	set_bool_value(debugMode, j, "debugMode");
	
	if(j["pathToClimateCSV"].is_string() && !j["pathToClimateCSV"].string_value().empty())
		pathsToClimateCSV.push_back(j["pathToClimateCSV"].string_value());
	else if(j["pathToClimateCSV"].is_array())
	{
		auto paths = toStringVector(j["pathToClimateCSV"].array_items());
		for(auto path : paths)
			if(!path.empty())
				pathsToClimateCSV.push_back(path);
	}
	csvViaHeaderOptions = j["csvViaHeaderOptions"];

	customId = j["customId"];
	set_string_value(sharedId, j, "sharedId");

	return es;
}

json11::Json Env::to_json() const
{
	J11Array cr;
	for(const auto& c : cropRotation)
		cr.push_back(c.to_json());

	return json11::Json::object
	{{"type", "Env"}
	,{"params", params.to_json()}
	,{"cropRotation", cr}
	,{"climateData", climateData.to_json()}
	,{"debugMode", debugMode}
	,{"pathsToClimateCSV", toPrimJsonArray(pathsToClimateCSV)}
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
	s << "ClimateData: from: " << climateData.startDate().toString()
		<< " to: " << climateData.endDate().toString() << endl;
	s << "Fruchtfolge: " << endl;
	for(const CultivationMethod& cm : cropRotation)
		s << cm.toString() << endl;
	s << "customId: " << customId.dump();
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

	climateData.addOrReplaceClimateData(AvailableClimateData(acd), data);
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

void storeResults2(const vector<OId>& outputIds,
									 vector<J11Object>& results,
									 const MonicaModel& monica)
{
	const auto& ofs = buildOutputTable().ofs;

	J11Object result;
	for(auto oid : outputIds)
	{
		auto ofi = ofs.find(oid.id);
		if(ofi != ofs.end())
			result[oid.outputName()] = ofi->second(monica, oid);
	}
	results.push_back(result);
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

void StoreData::aggregateResultsObj()
{
	if(!intermediateResults.empty())
	{
		assert(intermediateResults.size() == outputIds.size());

		J11Object result;
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
					case OId::FIRST: result[oid.outputName()] = ivs.front(); break;
					case OId::LAST: result[oid.outputName()] = ivs.back(); break;
					default: result[oid.outputName()] = ivs.front();
					}
				}
				else
					result[oid.outputName()] = applyOIdOP(oid.timeAggOp, ivs);

				intermediateResults[i].clear();
			}
			++i;
		}
		resultsObj.push_back(result);
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

void StoreData::storeResultsIfSpecAppliesObj(const MonicaModel& monica)
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
					storeResults2(outputIds, resultsObj, monica);
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
						aggregateResultsObj();
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
							aggregateResultsObj();
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
				storeResults2(outputIds, resultsObj, monica);
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
							aggregateResultsObj();
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
				storeResults2(outputIds, resultsObj, monica);
			}
		}
		//spec.at.isValue() can also mean "xxxx-xx-xx" = daily values
		else if(spec.at.isValue())
		{
			storeResults2(outputIds, resultsObj, monica);
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
					aggregateResultsObj();
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
	bool returnObjOutputs = env.returnObjOutputs();
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
	monica.simulationParametersNC().startDate = env.climateData.startDate();
	monica.simulationParametersNC().endDate = env.climateData.endDate();

	debug() << "currentDate" << endl;
	Date currentDate = env.climateData.startDate();

	//cropRotation is a shadow of the env.cropRotation, which will hold pointers to CMs in env.cropRotation, but might shrink
	//if pure absolute CMs are finished
	vector<CultivationMethod*> cropRotation;
	for(auto& cm : env.cropRotation)
		cropRotation.push_back(&cm);

	//iterator through the crop rotation
	auto cmit = cropRotation.begin();

	auto findNextCultivationMethod = [&](Date currentDate,
																			 bool advanceToNextCM = true)
	{
		CultivationMethod* currentCM = nullptr;
		Date nextAbsoluteCMApplicationDate;

		//it might be possible that the next cultivation method has to be skipped (if cover/catch crop)
		bool notFoundNextCM = true;
		while(notFoundNextCM)
		{
			if(advanceToNextCM)
			{
				//delete fully cultivation methods with only absolute worksteps,
				//because they won't participate in a new run when wrapping the crop rotation 
				if((*cmit)->areOnlyAbsoluteWorksteps() 
					 || !(*cmit)->repeat())
					cmit = cropRotation.erase(cmit);
				else
					cmit++;

				//start anew if we reached the end of the crop rotation
				if(cmit == cropRotation.end())
					cmit = cropRotation.begin();
			}

			//check if there's at least a cultivation method left in cropRotation
			if(cmit != cropRotation.end())
			{
				advanceToNextCM = true;
				currentCM = *cmit;
				
				//addedYear tells that the start of the cultivation method was before currentDate and thus the whole 
				//CM had to be moved into the next year
				//is possible for relative dates
				bool addedYear = currentCM->reinit(currentDate);
				if(addedYear)
				{
					//current CM is a cover crop, check if the latest sowing date would have been before current date, 
					//if so, skip current CM
					if(currentCM->isCoverCrop())
					{
						//if current CM's latest sowing date is actually after current date, we have to 
						//reinit current CM again, but this time prevent shifting it to the next year
						if(!(notFoundNextCM = currentCM->absLatestSowingDate().withYear(currentDate.year()) < currentDate))
							currentCM->reinit(currentDate, true);
					}
					else //if current CM was marked skipable, skip it
						notFoundNextCM = currentCM->canBeSkipped();
				}
				else //not added year or CM was had also absolute dates
				{
					if(currentCM->isCoverCrop())
						notFoundNextCM = currentCM->absLatestSowingDate() < currentDate;
					else if(currentCM->canBeSkipped())
						notFoundNextCM = currentCM->absStartDate() < currentDate;
					else
						notFoundNextCM = false;
				}

				if(notFoundNextCM)
					nextAbsoluteCMApplicationDate = Date();
				else
				{
					nextAbsoluteCMApplicationDate = currentCM->staticWorksteps().empty() ? Date() : currentCM->absStartDate(false);
					debug() << "new valid next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
				}
			}
			else
			{
				currentCM = nullptr;
				nextAbsoluteCMApplicationDate = Date();
				notFoundNextCM = false;
			}
		}

		return make_pair(currentCM, nextAbsoluteCMApplicationDate);
	};

	//direct handle to current cultivation method
	CultivationMethod* currentCM;
	Date nextAbsoluteCMApplicationDate;
	tie(currentCM, nextAbsoluteCMApplicationDate) = findNextCultivationMethod(currentDate, false);

	vector<StoreData> store = setupStorage(env.events, env.climateData.startDate(), env.climateData.endDate());
	
	for(size_t d = 0, nods = env.climateData.noOfStepsPossible(); d < nods; ++d, ++currentDate)
	{
		debug() << "currentDate: " << currentDate.toString() << endl;

		monica.dailyReset();

		monica.setCurrentStepDate(currentDate);
		monica.setCurrentStepClimateData(env.climateData.allDataForStep(d, env.params.siteParameters.vs_Latitude));

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
		{
			if(returnObjOutputs)
				s.storeResultsIfSpecAppliesObj(monica);
			else
				s.storeResultsIfSpecApplies(monica);
		}

		//if the next application date is not valid, we're at the end
		//of the application list of this cultivation method
		//and go to the next one in the crop rotation
		if(currentCM 
			 && currentCM->allDynamicWorkstepsFinished()
			 && !nextAbsoluteCMApplicationDate.isValid())
		{
			//to count the applied fertiliser for the next production process
			monica.resetFertiliserCounter();

			tie(currentCM, nextAbsoluteCMApplicationDate) = findNextCultivationMethod(currentDate + 1);
		}
	}
	
	for(auto& sd : store)
	{
		//aggregate results of while events or unfinished other from/to ranges (where to event didn't happen yet)
		if(returnObjOutputs)
			sd.aggregateResultsObj();
		else
			sd.aggregateResults();
		out.data.push_back({sd.spec.origSpec.dump(), sd.outputIds, sd.results, sd.resultsObj});
	}

	debug() << "returning from runMonica" << endl;

#ifdef TEST_HOURLY_OUTPUT
	tout(true);
#endif

	return out;
}
