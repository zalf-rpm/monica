/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Michael Berg-Mohnicke <michael.berg@zalf.de>
Tommaso Stella <tommaso.stella@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <fstream>
#include <algorithm>
#include <mutex>

#include "output.h"

#include "tools/json11-helper.h"
#include "tools/debug.h"
#include "tools/helper.h"
#include "tools/algorithms.h"
#include "../core/monica-model.h"

using namespace Monica;
using namespace Tools;
using namespace std;
using namespace json11;


OId::OId(json11::Json j)
{
	merge(j);
}

Errors OId::merge(json11::Json j)
{
	set_int_value(id, j, "id");
	set_string_value(name, j, "name");
	set_string_value(unit, j, "unit");
	set_string_value(jsonInput, j, "jsonInput");

	layerAggOp = OP(int_valueD(j, "layerAggOp", NONE));
	timeAggOp = OP(int_valueD(j, "timeAggOp", AVG));

	organ = ORGAN(int_valueD(j, "organ", _UNDEFINED_ORGAN_));

	set_int_value(fromLayer, j, "fromLayer");
	set_int_value(toLayer, j, "toLayer");

	return{};
}

json11::Json OId::to_json() const
{
	return json11::Json::object{
		{"type", "OId"},
		{"id", id},
		{"name", name},
		{"unit", unit},
		{"jsonInput", jsonInput},
		{"layerAggOp", int(layerAggOp)},
		{"timeAggOp", int(timeAggOp)},
		{"organ", int(organ)},
		{"fromLayer", fromLayer},
		{"toLayer", toLayer}
	};
}


std::string OId::toString(bool includeTimeAgg) const
{
	ostringstream oss;
	oss << "[";
	oss << name;
	if(isOrgan())
		oss << ", " << toString(organ);
	else if(isRange())
		oss << ", [" << (fromLayer + 1) << ", " << (toLayer + 1)
		<< (layerAggOp != OId::NONE ? string(", ") + toString(layerAggOp) : "")
		<< "]";
	else if(fromLayer >= 0)
		oss << ", " << (fromLayer + 1);
	if(includeTimeAgg)
		oss << ", " << toString(timeAggOp);
	oss << "]";

	return oss.str();
}

std::string OId::toString(OId::OP op) const
{
	string res("undef");
	switch(op)
	{
	case AVG: res = "AVG"; break;
	case MEDIAN: res = "MEDIAN"; break;
	case SUM: res = "SUM"; break;
	case MIN: res = "MIN"; break;
	case MAX: res = "MAX"; break;
	case FIRST: res = "FIRST"; break;
	case LAST: res = "LAST"; break;
	case NONE: res = "NONE"; break;
	case _UNDEFINED_OP_:
	default:;
	}
	return res;
}

std::string OId::toString(OId::ORGAN organ) const
{
	string res("undef");
	switch(organ)
	{
	case ROOT: res = "Root"; break;
	case LEAF: res = "Leaf"; break;
	case SHOOT: res = "Shoot"; break;
	case FRUIT: res = "Fruit"; break;
	case STRUCT: res = "Struct"; break;
	case SUGAR: res = "Sugar"; break;
	case _UNDEFINED_ORGAN_:
	default:;
	}
	return res;
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
	//interpret a string if possible as at pattern
	if(j.is_string() && j.string_value().size() == 10)
		j = J11Object{{"at", j.string_value()}};

	auto s = splitString(j["start"].string_value(), "-");
	DMY dmy;
	if(!s.empty() && !s[0].empty())
	{
		if(events().find(s[0]) != events().end())
			time2event["start"] = s[0];
		else
		{
			if(s.size() > 0)
				dmy.year = parseInt(s[0]);
			if(s.size() > 1)
				dmy.month = parseInt<size_t>(s[1]);
			if(s.size() > 2)
				dmy.day = parseInt<size_t>(s[2]);
			start = dmy;
		}
	}

	auto e = splitString(j["end"].string_value(), "-");
	dmy = DMY();
	if(!e.empty() && !e[0].empty())
	{
		if(events().find(e[0]) != events().end())
			time2event["end"] = e[0];
		else
		{
			if(e.size() > 0)
				dmy.year = parseInt(e[0]);
			if(e.size() > 1)
				dmy.month = parseInt<size_t>(e[1]);
			if(e.size() > 2)
				dmy.day = parseInt<size_t>(e[2]);
			end = dmy;
		}
	}

	auto a = splitString(j["at"].string_value(), "-");
	dmy = DMY();
	if(!a.empty() && !a[0].empty())
	{
		if(events().find(a[0]) != events().end())
			time2event["at"] = a[0];
		else
		{
			if(a.size() > 0)
				dmy.year = parseInt(a[0]);
			if(a.size() > 1)
				dmy.month = parseInt<size_t>(a[1]);
			if(a.size() > 2)
				dmy.day = parseInt<size_t>(a[2]);
			at = dmy;
		}
	}

	auto f = splitString(j["from"].string_value(), "-");
	dmy = DMY();
	if(!f.empty() && !f[0].empty())
	{
		if(events().find(f[0]) != events().end())
			time2event["from"] = f[0];
		else
		{
			if(f.size() > 0)
				dmy.year = parseInt(f[0]);
			if(f.size() > 1)
				dmy.month = parseInt<size_t>(f[1]);
			if(f.size() > 2)
				dmy.day = parseInt<size_t>(f[2]);
			from = dmy;
		}
	}

	auto t = splitString(j["to"].string_value(), "-");
	dmy = DMY();
	if(!t.empty() && !t[0].empty())
	{
		if(events().find(t[0]) != events().end())
			time2event["to"] = t[0];
		else
		{
			if(t.size() > 0)
				dmy.year = parseInt(t[0]);
			if(t.size() > 1)
				dmy.month = parseInt<size_t>(t[1]);
			if(t.size() > 2)
				dmy.day = parseInt<size_t>(t[2]);
			to = dmy;
		}
	}

	return{};
}

json11::Json Spec::to_json() const
{
	auto padDM = [](int i){ return (i < 10 ? string("0") : string()) + to_string(i); };
	auto padY = [](int i){ return (i < 10 ? string("0") : (i < 100 ? string("00") : (i < 1000 ? string("000") : string()))) + to_string(i); };

	string nothing = "xxxx-xx-xx";

	J11Object o{{"type", "Spec"}};
	if(start.isValue())
	{
		auto s = string("")
			+ (start.value().year.isValue() ? padY(start.value().year.value()) : "xxxx")
			+ (start.value().month.isValue() ? padDM(start.value().month.value()) : "xx")
			+ (start.value().day.isValue() ? padDM(start.value().day.value()) : "xx");
		if(s != nothing)
			o["start"] = s;
	}

	if(end.isValue())
	{
		auto e = string("")
			+ (end.value().year.isValue() ? padY(end.value().year.value()) : "xxxx")
			+ (end.value().month.isValue() ? padDM(end.value().month.value()) : "xx")
			+ (end.value().day.isValue() ? padDM(end.value().day.value()) : "xx");
		if(e != nothing)
			o["end"] = e;
	}

	if(at.isValue())
	{
		auto a = string("")
			+ (at.value().year.isValue() ? padY(at.value().year.value()) : "xxxx")
			+ (at.value().month.isValue() ? padDM(at.value().month.value()) : "xx")
			+ (at.value().day.isValue() ? padDM(at.value().day.value()) : "xx");
		o["at"] = a;
	}

	if(from.isValue())
	{
		auto f = string("")
			+ (from.value().year.isValue() ? padY(from.value().year.value()) : "xxxx")
			+ (from.value().month.isValue() ? padDM(from.value().month.value()) : "xx")
			+ (from.value().day.isValue() ? padDM(from.value().day.value()) : "xx");
		if(f != nothing)
			o["from"] = f;
	}

	if(to.isValue())
	{
		auto t = string("")
			+ (to.value().year.isValue() ? padY(to.value().year.value()) : "xxxx")
			+ (to.value().month.isValue() ? padDM(to.value().month.value()) : "xx")
			+ (to.value().day.isValue() ? padDM(to.value().day.value()) : "xx");
		if(t != nothing)
			o["to"] = t;
	}

	return o;
}

std::set<std::string> Spec::events()
{
	static const set<string> es = 
	{"seeding"
	,"harvesting"
	,"cutting"
	};

	return es;
}

//-----------------------------------------------------------------------------

Output::Output(json11::Json j)
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

Errors Output::merge(json11::Json j)
{
	Errors es;
	es.append(extractAndStore(j["dailyOutputIds"], dailyOutputIds));
	es.append(extractAndStore(j["monthlyOutputIds"], monthlyOutputIds));
	es.append(extractAndStore(j["yearlyOutputIds"], yearlyOutputIds));
	for(auto& p : j["atOutputIds"].object_items())
	es.append(extractAndStore(p.second, atOutputIds[Date::fromIsoDateString(p.first)]));
	es.append(extractAndStore(j["cropOutputIds"], cropOutputIds));
	es.append(extractAndStore(j["runOutputIds"], runOutputIds));

	customId = j["customId"].string_value();

	for(auto& j : j["daily"].array_items())
		daily.push_back(j.array_items());

	for(auto& p : j["monthly"].object_items())
		for(auto& j : p.second.array_items())
			monthly[stoi(p.first)].push_back(j.array_items());

	for(auto& j : j["yearly"].array_items())
		yearly.push_back(j.array_items());

	for(auto& p : j["at"].object_items())
		for(auto& j : p.second.array_items())
			at[Date::fromIsoDateString(p.first)].push_back(j.array_items());

	for(auto& p : j["crop"].object_items())
		for(auto& j : p.second.array_items())
			crop[p.first].push_back(j.array_items());

	run = j["run"].array_items();

	for(auto& p : j["origSpec2oids"].object_items())
		origSpec2oids[p.first] = toVector<OId>(p.second);

	for(auto& p : j["origSpec2results"].object_items())
	{
		vector<J11Array> v;
		for(auto& j : p.second.array_items())
			v.push_back(j.array_items());
		origSpec2results[p.first] = v;
	}

	return es;
}

json11::Json Output::to_json() const
{
	J11Object aoids;
	for(auto p : atOutputIds)
		aoids[p.first.toIsoDateString()] = p.second;

	J11Array daily_;
	for(auto& vs : daily)
		daily_.push_back(vs);

	J11Object monthly_;
	for(auto& p : monthly)
	{
		J11Array jvs;
		for(auto& vs : p.second)
			jvs.push_back(vs);
		monthly_[to_string(p.first)] = jvs;
	}

	J11Array yearly_;
	for(auto& vs : yearly)
		yearly_.push_back(vs);

	J11Object at_;
	for(auto& p : at)
	{
		J11Array jvs;
		for(auto& vs : p.second)
			jvs.push_back(vs);
		at_[p.first.toIsoDateString()] = jvs;
	}

	J11Object crop_;
	for(auto& p : crop)
		crop_[p.first] = p.second;

	J11Object os2oids;
	for(auto& p : origSpec2oids)
		os2oids[p.first] = toJsonArray(p.second);

	J11Object os2res;
	for(auto& p : origSpec2results)
	{
		J11Array a;
		for(auto rs : p.second)
			a.push_back(rs);
		os2oids[p.first] = a;
	}

	return json11::Json::object
	{{"type", "Output"}
	,{"dailyOutputIds", dailyOutputIds}
	,{"monthlyOutputIds", monthlyOutputIds}
	,{"yearlyOutputIds", yearlyOutputIds}
	,{"atOutputIds", aoids}
	,{"cropOutputIds", cropOutputIds}
	,{"runOutputIds", runOutputIds}
	,{"customId", customId}
	,{"daily", daily_}
	,{"monthly", monthly_}
	,{"yearly", yearly_}
	,{"at", at_}
	,{"crop", crop_}
	,{"run", run}
	,{"origSpec2oids", os2oids}
	,{"origSpec2results", os2res}
	};
}


//-----------------------------------------------------------------------------

/*
void Monica::addOutputToResultMessage(Output& out, J11Object& msg)
{
	msg["customId"] = out.customId;

	J11Array daily;
	for(auto& vs : out.daily)
		daily.push_back(vs);
	msg["daily"] = daily;

	J11Object monthly;
	for(auto& p : out.monthly)
	{
		J11Array jvs;
		for(auto& vs : p.second)
			jvs.push_back(vs);
		monthly[to_string(p.first)] = jvs;
	}
	msg["monthly"] = monthly;

	J11Array yearly;
	for(auto& vs : out.yearly)
		yearly.push_back(vs);
	msg["yearly"] = yearly;

	J11Object at;
	for(auto& p : out.at)
	{
		J11Array jvs;
		for(auto& vs : p.second)
			jvs.push_back(vs);
		at[p.first.toIsoDateString()] = jvs;
	}
	msg["at"] = at;

	J11Object crop;
	for(auto& p : out.crop)
		crop[p.first] = p.second;
	msg["crop"] = crop;

	msg["run"] = out.run;
}

//-----------------------------------------------------------------------------

void Monica::addResultMessageToOutput(const J11Object& msg, Output& out)
{
	auto ci = msg.find("customId");
	if(ci != msg.end())
		out.customId = ci->second.string_value();

	ci = msg.find("daily");
	if(ci != msg.end())
		for(auto& j : ci->second.array_items())
			out.daily.push_back(j.array_items());

	ci = msg.find("monthly");
	if(ci != msg.end())
		for(auto& p : ci->second.object_items())
			for(auto& j : p.second.array_items())
				out.monthly[stoi(p.first)].push_back(j.array_items());

	ci = msg.find("yearly");
	if(ci != msg.end())
		for(auto& j : ci->second.array_items())
			out.yearly.push_back(j.array_items());

	ci = msg.find("at");
	if(ci != msg.end())
		for(auto& p : ci->second.object_items())
			for(auto& j : p.second.array_items())
				out.at[Date::fromIsoDateString(p.first)].push_back(j.array_items());

	ci = msg.find("crop");
	if(ci != msg.end())
		for(auto& p : ci->second.object_items())
			for(auto& j : p.second.array_items())
				out.crop[p.first].push_back(j.array_items());

	ci = msg.find("run");
	if(ci != msg.end())
		out.run = ci->second.array_items();
}
//*/

//-----------------------------------------------------------------------------
