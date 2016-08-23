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

#include <fstream>
#include <algorithm>
#include <mutex>

#include "output.h"

#include "tools/debug.h"
#include "tools/helper.h"
#include "tools/algorithms.h"

#ifndef JUST_OUTPUTS
#include "../core/monica.h"
#endif

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

double Monica::applyOIdOP(OId::OP op, const vector<double>& vs)
{
	double v = 0.0;
	if(!vs.empty())
	{
		v = vs.back();

		switch(op)
		{
		case OId::AVG:
			v = accumulate(vs.begin(), vs.end(), 0.0) / vs.size();
			break;
		case OId::MEDIAN:
			v = median(vs);
			break;
		case OId::SUM:
			v = accumulate(vs.begin(), vs.end(), 0.0);
			break;
		case OId::MIN:
			v = minMax(vs).first;
			break;
		case OId::MAX:
			v = minMax(vs).second;
			break;
		case OId::FIRST:
			v = vs.front();
			break;
		case OId::LAST:
		case OId::NONE:
		default:;
		}
	}

	return v;
}

Json Monica::applyOIdOP(OId::OP op, const vector<Json>& js)
{
	Json res;

	if(!js.empty() && js.front().is_array())
	{
		vector<vector<double>> dss(js.front().array_items().size());
		for(auto& j : js)
		{
			int i = 0;
			for(auto& j2 : j.array_items())
			{
				dss[i].push_back(j2.number_value());
				++i;
			}
		}

		J11Array r;
		for(auto& ds : dss)
			r.push_back(applyOIdOP(op, ds));

		res = r;
	}
	else
	{
		vector<double> ds;
		for(auto j : js)
			ds.push_back(j.number_value());
		res = applyOIdOP(op, ds);
	}

	return res;
}

//-----------------------------------------------------------------------------

vector<OId> Monica::parseOutputIds(const J11Array& oidArray)
{
	vector<OId> outputIds;

	auto getAggregationOp = [](J11Array arr, int index, OId::OP def = OId::_UNDEFINED_OP_) -> OId::OP
	{
		if(arr.size() > index && arr[index].is_string())
		{
			string ops = arr[index].string_value();
			if(toUpper(ops) == "SUM")
				return OId::SUM;
			else if(toUpper(ops) == "AVG")
				return OId::AVG;
			else if(toUpper(ops) == "MEDIAN")
				return OId::MEDIAN;
			else if(toUpper(ops) == "MIN")
				return OId::MIN;
			else if(toUpper(ops) == "MAX")
				return OId::MAX;
			else if(toUpper(ops) == "FIRST")
				return OId::FIRST;
			else if(toUpper(ops) == "LAST")
				return OId::LAST;
			else if(toUpper(ops) == "NONE")
				return OId::NONE;
		}
		return def;
	};


	auto getOrgan = [](J11Array arr, int index, OId::ORGAN def = OId::_UNDEFINED_ORGAN_) -> OId::ORGAN
	{
		if(arr.size() > index && arr[index].is_string())
		{
			string ops = arr[index].string_value();
			if(toUpper(ops) == "ROOT")
				return OId::ROOT;
			else if(toUpper(ops) == "LEAF")
				return OId::LEAF;
			else if(toUpper(ops) == "SHOOT")
				return OId::SHOOT;
			else if(toUpper(ops) == "FRUIT")
				return OId::FRUIT;
			else if(toUpper(ops) == "STRUCT")
				return OId::STRUCT;
			else if(toUpper(ops) == "SUGAR")
				return OId::SUGAR;
		}
		return def;
	};

	const auto& name2result = buildOutputTable().name2result;
	for(Json idj : oidArray)
	{
		if(idj.is_string())
		{
			string name = idj.string_value();
			auto it = name2result.find(name);
			if(it != name2result.end())
			{
				auto data = it->second;
				OId oid(data.id);
				oid.name = data.name;
				oid.unit = data.unit;
				oid.jsonInput = name;
				outputIds.push_back(oid);
			}
		}
		else if(idj.is_array())
		{
			auto arr = idj.array_items();
			if(arr.size() >= 1)
			{
				OId oid;

				string name = arr[0].string_value();
				auto it = name2result.find(name);
				if(it != name2result.end())
				{
					auto data = it->second;
					oid.id = data.id;
					oid.name = data.name;
					oid.unit = data.unit;
					oid.jsonInput = Json(arr).dump();

					if(arr.size() >= 2)
					{
						auto val1 = arr[1];
						if(val1.is_number())
							oid.fromLayer = val1.int_value() - 1;
						else if(val1.is_string())
						{
							auto op = getAggregationOp(arr, 1);
							if(op != OId::_UNDEFINED_OP_)
								oid.timeAggOp = op;
							else
								oid.organ = getOrgan(arr, 1, OId::_UNDEFINED_ORGAN_);
						}
						else if(val1.is_array())
						{
							auto arr2 = arr[1].array_items();

							if(arr2.size() >= 1)
							{
								auto val1_0 = arr2[0];
								if(val1_0.is_number())
									oid.fromLayer = val1_0.int_value() - 1;
								else if(val1_0.is_string())
									oid.organ = getOrgan(arr2, 0, OId::_UNDEFINED_ORGAN_);
							}
							if(arr2.size() >= 2)
							{
								auto val1_1 = arr2[1];
								if(val1_1.is_number())
									oid.toLayer = val1_1.int_value() - 1;
								else if(val1_1.is_string())
								{
									oid.toLayer = oid.fromLayer;
									oid.layerAggOp = getAggregationOp(arr2, 1, OId::AVG);
								}
							}
							if(arr2.size() >= 3)
								oid.layerAggOp = getAggregationOp(arr2, 2, OId::AVG);
						}
					}
					if(arr.size() >= 3)
						oid.timeAggOp = getAggregationOp(arr, 2, OId::AVG);

					outputIds.push_back(oid);
				}
			}
		}
	}

	return outputIds;
}


//-----------------------------------------------------------------------------

void Monica::addOutputToResultMessage(Output& out, J11Object& msg)
{
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
	auto ci = msg.find("daily");
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
			out.crop[p.first] = p.second.array_items();

	ci = msg.find("run");
	if(ci != msg.end())
		out.run = ci->second.array_items();
}

//-----------------------------------------------------------------------------

template<typename T, typename Vector>
void store(OId oid, Vector& into, function<T(int)> getValue, int roundToDigits = 0)
{
	Vector multipleValues;
	vector<double> vs;
	if(oid.isOrgan())
		oid.toLayer = oid.fromLayer = int(oid.organ);

	for(int i = oid.fromLayer; i <= oid.toLayer; i++)
	{
		T v = getValue(i);
		if(oid.layerAggOp == OId::NONE)
			multipleValues.push_back(Tools::round(v, roundToDigits));
		else
			vs.push_back(v);
	}

	if(oid.layerAggOp == OId::NONE)
		into.push_back(multipleValues);
	else
		into.push_back(applyOIdOP(oid.layerAggOp, vs));
}

BOTRes& Monica::buildOutputTable()
{
	static mutex lockable;

	//map of output ids to outputfunction
	static BOTRes m;
	static bool tableBuild = false;

	auto build = [&](Result2 r
#ifndef JUST_OUTPUTS
									 , decltype(m.ofs)::mapped_type of
#endif
	){
#ifndef JUST_OUTPUTS
		m.ofs[r.id] = of;
#endif
		m.name2result[r.name] = r;
	};

	// only initialize once
	if(!tableBuild)
	{
		lock_guard<mutex> lock(lockable);

		//test if after waiting for the lock the other thread
		//already initialized the whole thing
		if(!tableBuild)
		{
			int id = 0;

			build({id++, "Date", "", "output current date"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.currentDate.toIsoDateString());
			}
#endif 
			);
			build({id++, "Month", "", "output current Month"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(int(m.currentDate.month()));
			}
#endif
			);
			build({id++, "Year", "", "output current Year"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(int(m.currentDate.year()));
			}
#endif 
			);
			build({id++, "Crop", "", "crop name"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? m.mcg->get_CropName() : "");
			}
#endif 
			);

			build({id++, "TraDef", "0;1", "TranspirationDeficit"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_TranspirationDeficit(), 2) : 0.0);
			}
#endif 
			);

			build({id++, "Tra", "mm", "ActualTranspiration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_ActualTranspiration(), 2) : 0.0);
			}
#endif 
			);

			build({id++, "NDef", "0;1", "CropNRedux"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CropNRedux(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "HeatRed", "0;1", " HeatStressRedux"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_HeatStressRedux(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "FrostRed", "0;1", "FrostStressRedux"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_FrostStressRedux(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "OxRed", "0;1", "OxygenDeficit"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OxygenDeficit(), 2) : 0.0);
			}
#endif 
			);

			build({id++, "Stage", "", "DevelopmentalStage"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? m.mcg->get_DevelopmentalStage() + 1 : 0);
			}
#endif 
			);
			build({id++, "TempSum", "°Cd", "CurrentTemperatureSum"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CurrentTemperatureSum(), 1) : 0.0);
			}
#endif 
			);
			build({id++, "VernF", "0;1", "VernalisationFactor"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_VernalisationFactor(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "DaylF", "0;1", "DaylengthFactor"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_DaylengthFactor(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "IncRoot", "kg ha-1", "OrganGrowthIncrement root"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(0), 2) : 0.0);
			}
#endif 
			);
			build({id++, "IncLeaf", "kg ha-1", "OrganGrowthIncrement leaf"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(1), 2) : 0.0);
			}
#endif 
			);
			build({id++, "IncShoot", "kg ha-1", "OrganGrowthIncrement shoot"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(2), 2) : 0.0);
			}
#endif 
			);
			build({id++, "IncFruit", "kg ha-1", "OrganGrowthIncrement fruit"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(3), 2) : 0.0);
			}
#endif 
			);
			build({id++, "RelDev", "0;1", "RelativeTotalDevelopment"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_RelativeTotalDevelopment(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "LT50", "°C", "LT50"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_LT50(), 1) : 0.0);
			}
#endif 
			);
			build({id++, "AbBiom", "kg ha-1", "AbovegroundBiomass"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AbovegroundBiomass(), 1) : 0.0);
			}
#endif 
			);
			build({id++, "OrgBiom", "kgDM ha-1", "get_OrganBiomass(i)"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				if(oid.isOrgan() 
					 && m.cropPlanted
					 && m.mcg->get_NumberOfOrgans() >= oid.organ)
					results.push_back(round(m.mcg->get_OrganBiomass(oid.organ), 1));
				else
					results.push_back(0.0);

				//store<double>(oid, results, [&](int i)
				//{
				//	return m.cropPlanted && m.mcg->get_NumberOfOrgans() >= i
				//		? m.mcg->get_OrganBiomass(i) : 0.0;
				//}, 1);
			}
#endif 
			);
			build({id++, "Yield", "kgDM ha-1", "get_PrimaryCropYield"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_PrimaryCropYield(), 1) : 0.0);
			}
#endif 
			);
			build({id++, "SumYield", "kgDM ha-1", "get_AccumulatedPrimaryCropYield"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AccumulatedPrimaryCropYield(), 1) : 0.0);
			}
#endif 
			);
			build({id++, "GroPhot", "kgCH2O ha-1", "GrossPhotosynthesisHaRate"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_GrossPhotosynthesisHaRate(), 4) : 0.0);
			}
#endif 
			);
			build({id++, "NetPhot", "kgCH2O ha-1", "NetPhotosynthesis"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_NetPhotosynthesis(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "MaintR", "kgCH2O ha-1", "MaintenanceRespirationAS"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_MaintenanceRespirationAS(), 4) : 0.0);
			}
#endif 
			);
			build({id++, "GrowthR", "kgCH2O ha-1", "GrowthRespirationAS"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_GrowthRespirationAS(), 4) : 0.0);
			}
#endif 
			);
			build({id++, "StomRes", "s m-1", "StomataResistance"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_StomataResistance(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "Height", "m", "CropHeight"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CropHeight(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "LAI", "m2 m-2", "LeafAreaIndex"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_LeafAreaIndex(), 4) : 0.0);
			}
#endif 
			);
			build({id++, "RootDep", "layer#", "RootingDepth"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? m.mcg->get_RootingDepth() : 0);
			}
#endif 
			);
			build({id++, "EffRootDep", "m", "Effective RootingDepth"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->getEffectiveRootingDepth(), 2) : 0.0);
			}
#endif 
			);

			build({id++, "TotBiomN", "kgN ha-1", "TotalBiomassNContent"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_TotalBiomassNContent(), 1) : 0.0);
			}
#endif 
			);
			build({id++, "AbBiomN", "kgN ha-1", "AbovegroundBiomassNContent"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AbovegroundBiomassNContent(), 1) : 0.0);
			}
#endif 
			);
			build({id++, "SumNUp", "kgN ha-1", "SumTotalNUptake"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_SumTotalNUptake(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "ActNup", "kgN ha-1", "ActNUptake"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_ActNUptake(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "PotNup", "kgN ha-1", "PotNUptake"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_PotNUptake(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "NFixed", "kgN ha-1", "NFixed"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_BiologicalNFixation(), 2) : 0.0);
			}
#endif 
			);
			build({id++, "Target", "kgN ha-1", "TargetNConcentration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_TargetNConcentration(), 3) : 0.0);
			}
#endif 
			);
			build({id++, "CritN", "kgN ha-1", "CriticalNConcentration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CriticalNConcentration(), 3) : 0.0);
			}
#endif 
			);
			build({id++, "AbBiomNc", "kgN ha-1", "AbovegroundBiomassNConcentration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AbovegroundBiomassNConcentration(), 3) : 0.0);
			}
#endif 
			);
			build({id++, "YieldNc", "kgN ha-1", "PrimaryYieldNConcentration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_PrimaryYieldNConcentration(), 3) : 0.0);
			}
#endif 
			);

			build({id++, "Protein", "kg kg-1", "RawProteinConcentration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_RawProteinConcentration(), 3) : 0.0);
			}
#endif 
			);

			build({id++, "NPP", "kgC ha-1", "NPP"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_NetPrimaryProduction(), 5) : 0.0);
			}
#endif 
			);
			build({id++, "NPP-Organs", "kgC ha-1", "organ specific NPP"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				if(oid.isOrgan()
					 && m.cropPlanted
					 && m.mcg->get_NumberOfOrgans() >= oid.organ)
					results.push_back(round(m.mcg->get_OrganSpecificNPP(oid.organ), 4));
				else
					results.push_back(0.0);

				//store<double>(oid, results, [&](int i)
				//{
				//	return m.cropPlanted && m.mcg->get_NumberOfOrgans() >= i
				//		? m.mcg->get_OrganSpecificNPP(i) : 0.0;
				//}, 4);
			}
#endif 
			);

			build({id++, "GPP", "kgC ha-1", "GPP"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_GrossPrimaryProduction(), 5) : 0.0);
			}
#endif 
			);

			build({id++, "Ra", "kgC ha-1", "autotrophic respiration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AutotrophicRespiration(), 5) : 0.0);
			}
#endif 
			);
			build({id++, "Ra-Organs", "kgC ha-1", "organ specific autotrophic respiration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				if(oid.isOrgan()
					 && m.cropPlanted
					 && m.mcg->get_NumberOfOrgans() >= oid.organ)
					results.push_back(round(m.mcg->get_OrganSpecificTotalRespired(oid.organ), 4));
				else
					results.push_back(0.0);

				//store<double>(oid, results, [&](int i)
				//{
				//	return m.cropPlanted && m.mcg->get_NumberOfOrgans() >= i
				//		? m.mcg->get_OrganSpecificTotalRespired(i) : 0.0;
				//}, 4);
			}
#endif 
			);

			build({id++, "Mois", "m3 m-3", "Soil moisture content"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.moist->get_SoilMoisture(i); }, 3);
			}
#endif 
			);

			build({id++, "Irrig", "mm", "Irrigation"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica->dailySumIrrigationWater(), 1));
			}
#endif 
			);
			build({id++, "Infilt", "mm", "Infiltration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_Infiltration(), 1));
			}
#endif 
			);
			build({id++, "Surface", "mm", "Surface water storage"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_SurfaceWaterStorage(), 1));
			}
#endif 
			);
			build({id++, "RunOff", "mm", "Surface water runoff"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_SurfaceRunOff(), 1));
			}
#endif 
			);
			build({id++, "SnowD", "mm", "Snow depth"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_SnowDepth(), 1));
			}
#endif 
			);
			build({id++, "FrostD", "m", "Frost front depth in soil"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_FrostDepth(), 1));
			}
#endif 
			);
			build({id++, "ThawD", "m", "Thaw front depth in soil"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_ThawDepth(), 1));
			}
#endif 
			);

			build({id++, "PASW", "m3 m-3", "PASW"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results,
											[&](int i){ return m.moist->get_SoilMoisture(i) - m.soilc->at(i).vs_PermanentWiltingPoint(); },
											3);
			}
#endif 
			);

			build({id++, "SurfTemp", "°C", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.temp->get_SoilSurfaceTemperature(), 1));
			}
#endif 
			);
			build({id++, "STemp", "°C", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.temp->get_SoilTemperature(i); }, 1);
			}
#endif 
			);
			build({id++, "Act_Ev", "mm", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_ActualEvaporation(), 1));
			}
#endif 
			);
			build({id++, "Act_ET", "mm", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_Evapotranspiration(), 1));
			}
#endif 
			);
			build({id++, "ET0", "mm", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_ET0(), 1));
			}
#endif 
			);
			build({id++, "Kc", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_KcFactor(), 1));
			}
#endif 
			);
			build({id++, "AtmCO2", "ppm", "Atmospheric CO2 concentration"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica->get_AtmosphericCO2Concentration(), 0));
			}
#endif 
			);
			build({id++, "Groundw", "m", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica->get_GroundwaterDepth(), 2));
			}
#endif 
			);
			build({id++, "Recharge", "mm", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_GroundwaterRecharge(), 3));
			}
#endif 
			);
			build({id++, "NLeach", "kgN ha-1", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.trans->get_NLeaching(), 3));
			}
#endif 
			);
			build({id++, "NO3", "kgN m-3", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc->at(i).get_SoilNO3(); }, 3);
			}
#endif 
			);
			build({id++, "Carb", "kgN m-3", "Soil Carbamid"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.soilc->at(0).get_SoilCarbamid(), 4));
			}
#endif 
			);
			build({id++, "NH4", "kgN m-3", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc->at(i).get_SoilNH4(); }, 4);
			}
#endif 
			);
			build({id++, "NO2", "kgN m-3", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc->at(i).get_SoilNO2(); }, 4);
			}
#endif 
			);
			build({id++, "SOC", "kgC kg-1", "get_SoilOrganicC"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc->at(i).vs_SoilOrganicCarbon(); }, 4);
			}
#endif 
			);
			build({id++, "SOC-X-Y", "gC m-2", "SOC-X-Y"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i)
				{
					return m.soilc->at(i).vs_SoilOrganicCarbon()
						* m.soilc->at(i).vs_SoilBulkDensity()
						* m.soilc->at(i).vs_LayerThickness
						* 1000;
				}, 4);
			}
#endif 
			);

			build({id++, "AOMf", "kgC m-3", "get_AOM_FastSum"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_AOM_FastSum(i); }, 4);
			}
#endif 
			);
			build({id++, "AOMs", "kgC m-3", "get_AOM_SlowSum"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_AOM_SlowSum(i); }, 4);
			}
#endif 
			);
			build({id++, "SMBf", "kgC m-3", "get_SMB_Fast"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_SMB_Fast(i); }, 4);
			}
#endif 
			);
			build({id++, "SMBs", "kgC m-3", "get_SMB_Slow"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_SMB_Slow(i); }, 4);
			}
#endif 
			);
			build({id++, "SOMf", "kgC m-3", "get_SOM_Fast"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_SOM_Fast(i); }, 4);
			}
#endif 
			);
			build({id++, "SOMs", "kgC m-3", "get_SOM_Slow"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_SOM_Slow(i); }, 4);
			}
#endif 
			);
			build({id++, "CBal", "kgC m-3", "get_CBalance"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_CBalance(i); }, 4);
			}
#endif 
			);

			build({id++, "Nmin", "kgN ha-1", "NetNMineralisationRate"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_NetNMineralisationRate(i); }, 6);
			}
#endif 
			);
			build({id++, "NetNmin", "kgN ha-1", "NetNmin"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org->get_NetNMineralisation(), 5));
			}
#endif 
			);
			build({id++, "Denit", "kgN ha-1", "Denit"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org->get_Denitrification(), 5));
			}
#endif 
			);
			build({id++, "N2O", "kgN ha-1", "N2O"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org->get_N2O_Produced(), 5));
			}
#endif 
			);
			build({id++, "SoilpH", "", "SoilpH"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.soilc->at(0).get_SoilpH(), 1));
			}
#endif 
			);
			build({id++, "NEP", "kgC ha-1", "NEP"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org->get_NetEcosystemProduction(), 5));
			}
#endif 
			);
			build({id++, "NEE", "kgC ha-", "NEE"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org->get_NetEcosystemExchange(), 5));
			}
#endif 
			);
			build({id++, "Rh", "kgC ha-", "Rh"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org->get_DecomposerRespiration(), 5));
			}
#endif 
			);

			build({id++, "Tmin", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::tmin, m.timestep), 4));
			}
#endif 
			);
			build({id++, "Tavg", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::tavg, m.timestep), 4));
			}
#endif 
			);
			build({id++, "Tmax", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::tmax, m.timestep), 4));
			}
#endif 
			);
			build({id++, "Precip", "mm", "Precipitation"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::precip, m.timestep), 2));
			}
#endif 
			);
			build({id++, "Wind", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::wind, m.timestep), 4));
			}
#endif 
			);
			build({id++, "Globrad", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::globrad, m.timestep), 4));
			}
#endif 
			);
			build({id++, "Relhumid", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::relhumid, m.timestep), 4));
			}
#endif 
			);
			build({id++, "Sunhours", "", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::sunhours, m.timestep), 4));
			}
#endif 
			);

			build({id++, "BedGrad", "0;1", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist->get_PercentageSoilCoverage(), 3));
			}
#endif 
			);
			build({id++, "N", "kgN m-3", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc->at(i).get_SoilNmin(); }, 3);
			}
#endif 
			);
			build({id++, "Co", "kgC m-3", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_SoilOrganicC(i); }, 2);
			}
#endif 
			);
			build({id++, "NH3", "kgN ha-1", "NH3_Volatilised"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org->get_NH3_Volatilised(), 3));
			}
#endif 
			);
			build({id++, "NFert", "kgN ha-1", "dailySumFertiliser"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica->dailySumFertiliser(), 1));
			}
#endif 
			);
			build({id++, "WaterContent", "%nFC", "soil water content"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i)
				{
					double smm3 = m.moist->get_SoilMoisture(i); // soilc.at(i).get_Vs_SoilMoisture_m3();
					double fc = m.soilc->at(i).vs_FieldCapacity();
					double pwp = m.soilc->at(i).vs_PermanentWiltingPoint();
					return smm3 / (fc - pwp); //[%nFK]
				}, 4);
			}
#endif 
			);
			build({id++, "CapillaryRise", "mm", "capillary rise"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.moist->get_CapillaryRise(i); }, 3);
			}
#endif 
			);
			build({id++, "PercolationRate", "mm", "percolation rate"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.moist->get_PercolationRate(i); }, 3);
			}
#endif 
			);
			build({id++, "SMB-CO2-ER", "", "soilOrganic.get_SMB_CO2EvolutionRate"}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org->get_SMB_CO2EvolutionRate(i); }, 1);
			}
#endif 
			);
			build({id++, "Evapotranspiration", "mm", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica->getEvapotranspiration(), 1));
			}
#endif 
			);
			build({id++, "Evaporation", "mm", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica->getEvaporation(), 1));
			}
#endif 
			);
			build({id++, "Transpiration", "mm", ""}
#ifndef JUST_OUTPUTS
						, [](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica->getTranspiration(), 1));
			}
#endif 
			);

			tableBuild = true;
		}
	}

	return m;
}

//-----------------------------------------------------------------------------
