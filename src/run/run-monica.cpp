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

function<bool(const MonicaModel&)> buildExpression(J11Array a)
{
	if(a.size() == 3
		 && (a[0].is_number() || a[0].is_string() || a[0].is_array())
		 && a[1].is_string()
		 && (a[2].is_number() || a[2].is_string() || a[2].is_array()))
	{
		Json leftj = a[0];
		string ops = a[1].string_value();
		Json rightj = a[2];
			
		function<bool(double, double)> op;
		if(ops == "<")
			op = [](double l, double r){ 
			return l < r; 
		};
		else if(ops == "<=")
			op = [](double l, double r){ return l <= r; };
		else if(ops == "=")
			op = [](double l, double r){ return l == r; };
		else if(ops == "!=")
			op = [](double l, double r){ return l != r; };
		else if(ops == ">")
			op = [](double l, double r){ return l > r; };
		else if(ops == ">=")
			op = [](double l, double r){ return l >= r; };

		const auto& ofs = buildOutputTable().ofs;
		decltype(buildOutputTable().ofs)::mapped_type lf, rf;
		OId loid, roid;
		if(!leftj.is_number())
		{
			auto loids = parseOutputIds({leftj});
			if(!loids.empty())
			{
				loid = loids.front();
				auto ofi = ofs.find(loid.id);
				if(ofi != ofs.end())
					lf = ofi->second;
			}
		}
		if(!rightj.is_number())
		{
			auto roids = parseOutputIds({rightj});
			if(!roids.empty())
			{
				roid = roids.front();
				auto ofi = ofs.find(roid.id);
				if(ofi != ofs.end())
					rf = ofi->second;
			}
		}

		auto applyOp = [=](Json lj, Json rj)
		{
			if(lj.is_number() && rj.is_number())
				return op(lj.number_value(), rj.number_value());
			else if(lj.is_array() && rj.is_number())
			{
				double rn = rj.number_value();
				auto lja = lj.array_items();
				return accumulate(lja.begin(), lja.end(), true, [=](bool acc, Json j)
				{
					return acc && (j.is_number() ? op(j.number_value(), rn) : false);
				});
			}
			else if(lj.is_number() && rj.is_array())
			{
				double ln = lj.number_value();
				auto rja = rj.array_items();
				return accumulate(rja.begin(), rja.end(), true, [=](bool acc, Json j)
				{
					return acc && (j.is_number() ? op(j.number_value(), ln) : false);
				});
			}
			else if(lj.is_array() && rj.is_array())
			{
				auto lja = lj.array_items();
				auto rja = rj.array_items();
				vector<bool> res;
				//compare values point wise (dot product)
				transform(lja.begin(), lja.end(), rja.begin(), back_inserter(res), [=](Json left, Json right)
				{
					return left.is_number() && right.is_number() ? op(left.number_value(), right.number_value()) : false;
				});
				return accumulate(res.begin(), res.end(), true, [](bool acc, bool v){ return acc && v; });
			}
			return false;
		};
		
		if(lf && rf && op)
		{
			return [=](const MonicaModel& m)
			{ 
				return applyOp(lf(m, loid), rf(m, roid));
			};
		}
		else if(lf && rightj.is_number() && op)
		{
			return [=](const MonicaModel& m)
			{
				return applyOp(lf(m, loid), rightj);
			};
		}
		else if(leftj.is_number() && rf && op)
		{
			return [=](const MonicaModel& m)
			{
				return applyOp(leftj, rf(m, roid));
			};
		}
	}

	return function<bool(const MonicaModel&)>();
}

Tools::Errors Spec::merge(json11::Json j)
{
	init(start, j, "start");
	init(end, j, "end");
	Maybe<DMY> dummy;
	init(dummy, j, "while");
	init(at, j, "at");
	init(from, j, "from");
	init(to, j, "to");
	
	return{};
}

void Spec::init(Maybe<DMY>& member, Json j, string time)
{
	auto jt = j[time];
	
	//is an expression event
	if(jt.is_array())
	{
		if(auto f = buildExpression(jt.array_items()))
		{
			time2expression[time] = f;
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
							aggregateResults();
						}
					}
				}
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
	,{"crop", J11Object{{"from", "seeding"}, {"to", "harvesting"}}}
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

	vector<StoreData> store = setupStorage(env.events, env.da.startDate(), env.da.endDate());

	//if for some reason there are no applications (no nothing) in the
	//production process: quit
	if(!nextAbsoluteCMApplicationDate.isValid())
	{
		debug() << "start of production-process: " << currentCM.toString()
			<< " is not valid" << endl;
		return out;
	}

	auto calcNextAbsoluteCMApplicationDate = [=](Date currentDate, Date nextCMAppDate, Date prevCMAppDate)
	{
		return useRelativeDates ? nextCMAppDate.toAbsoluteDate
			(currentDate.year() + (nextCMAppDate.dayOfYear() > prevCMAppDate.dayOfYear()
														 ? 0
														 : 1),
			 true)
			: nextCMAppDate;
	};

	//beware: !!!! if there are absolute days used, then there is basically
	//no rotation if the last crop in the crop rotation has changed
	//the loop starts anew but the first crops date has already passed
	//so the crop won't be seeded again or any work applied
	//thus for absolute dates the crop rotation has to be as long as there
	//are climate data !!!!!

	for(unsigned int d = 0; d < nods; ++d, ++currentDate, ++dim)
	{
		monica.dailyReset();

		debug() << "currentDate: " << currentDate.toString() << endl;

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

					//aggregateCropOutput();

					//auto harvestApplication = make_unique<Harvest>(currentDate, currentPP.crop(), currentPP.cropResultPtr());
					auto harvestApplication =
						unique_ptr<Harvest>(new Harvest(currentDate,
																						currentCM.crop()));
					harvestApplication->apply(&monica);
				}
			}
		}

		//apply worksteps and cycle through crop rotation
		Date prevCMApplicationDate = nextCMApplicationDate;
		if(nextAbsoluteCMApplicationDate == currentDate)
		{
			debug() << "applying at: " << nextCMApplicationDate.toString()
				<< " absolute-at: " << nextAbsoluteCMApplicationDate.toString() << endl;
			//apply everything to do at current day
			//cout << currentPP.toString().c_str() << endl;
			currentCM.apply(nextCMApplicationDate, &monica);// , {{"Harvest", aggregateCropOutput}});

			//get the next application date to wait for (either absolute or relative)
			nextCMApplicationDate = currentCM.nextDate(nextCMApplicationDate);
			nextAbsoluteCMApplicationDate = calcNextAbsoluteCMApplicationDate(currentDate, nextCMApplicationDate, prevCMApplicationDate);

			debug() << "next app-date: " << nextCMApplicationDate.toString()
				<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
		}

		//monica main stepping method
		monica.step(currentDate, climateDataForStep(env.da, d).second);

		//store results
		for(auto& s : store)
			s.storeResultsIfSpecApplies(monica);

		//if the next application date is not valid, we're (probably) at the end
		//of the application list of this cultivation method
		//and go to the next one in the crop rotation
		if(!nextAbsoluteCMApplicationDate.isValid())
		{
			//to count the applied fertiliser for the next production process
			monica.resetFertiliserCounter();

			cmci++;
			//start anew if we reached the end of the crop rotation
			if(cmci == env.cropRotation.end())
				cmci = env.cropRotation.begin();

			currentCM = *cmci;
			nextCMApplicationDate = currentCM.startDate();
			nextAbsoluteCMApplicationDate = calcNextAbsoluteCMApplicationDate(currentDate, nextCMApplicationDate, prevCMApplicationDate);
			debug() << "new valid next app-date: " << nextCMApplicationDate.toString()
				<< " next abs app-date: " << nextAbsoluteCMApplicationDate.toString() << endl;
		}
		//if we got our next date relative it might be possible that
		//the actual relative date belongs into the next year
		//this is the case if we're already (dayOfYear) past the next dayOfYear
		if(useRelativeDates && currentDate > nextAbsoluteCMApplicationDate)
			nextAbsoluteCMApplicationDate.addYears(1);
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
