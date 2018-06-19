/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Michael Berg-Mohnicke <michael.berg@zalf.de>

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
//#include "../core/monica-model.h"

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
	set_string_value(displayName, j, "displayName");
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
	return json11::Json::object
	{{"type", "OId"}
	,{"id", id}
	,{"name", name}
	,{"displayName", displayName}
	,{"unit", unit}
	,{"jsonInput", jsonInput}
	,{"layerAggOp", int(layerAggOp)}
	,{"timeAggOp", int(timeAggOp)}
	,{"organ", int(organ)}
	,{"fromLayer", fromLayer}
	,{"toLayer", toLayer}
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

std::string OId::outputName() const 
{
	string outName = name;
	if(isOrgan())
		outName = outName + "/" + toString(organ);
	if(!displayName.empty())
		outName = displayName;
	return outName;
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

	customId = j["customId"];// .string_value();

	for(const auto& d : j["data"].array_items())
	{
		vector<J11Array> vs;
		vector<J11Object> os;
		for(auto& j : d["results"].array_items())
		{
			if(j.is_array())
				vs.push_back(j.array_items());
			else if(j.is_object())
				os.push_back(j.object_items());
		}
		data.push_back({d["origSpec"].string_value(), toVector<OId>(d["outputIds"]), vs, os});
	}

	return es;
}

json11::Json Output::to_json() const
{
	J11Array ds;
	for(const auto& d : data)
	{
		J11Array rs;
		if(!d.results.empty())
			for(auto r : d.results)
				rs.push_back(r);
		else if(!d.resultsObj.empty())
			for(auto o : d.resultsObj)
				rs.push_back(o);
		ds.push_back(J11Object
		{{"origSpec", d.origSpec}
		,{"outputIds", toJsonArray(d.outputIds)}
		,{"results", rs}
		});
	}

	return json11::Json::object
	{{"type", "Output"}
	,{"customId", customId}
	,{"data", ds}
	};
}

//-----------------------------------------------------------------------------

