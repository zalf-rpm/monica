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
	return json11::Json::object {
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
	void extractAndStore(Json jv, Vector& v)
	{
		v.clear();
		for(Json cmj : jv.array_items())
			v.push_back(cmj);
	}
}

Errors Env::merge(json11::Json j)
{
	params.merge(j["params"]);

	da.merge(j["da"]);

	extractAndStore(j["cropRotation"], cropRotation);
	extractAndStore(j["dailyOutputIds"], dailyOutputIds);
	extractAndStore(j["monthlyOutputIds"], monthlyOutputIds);
	extractAndStore(j["yearlyOutputIds"], yearlyOutputIds);
	extractAndStore(j["runOutputIds"], runOutputIds);
	extractAndStore(j["cropOutputIds"], cropOutputIds);
	for(auto& p : j["atOutputIds"].object_items())
		extractAndStore(p.second, atOutputIds[Date::fromIsoDateString(p.first)]);

	set_bool_value(debugMode, j, "debugMode");

	return{};
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
		{"type", "Env"},
		{"params", params.to_json()},
		{"cropRotation", cr},
		{"dailyOutputIds", dailyOutputIds},
		{"monthlyOutputIds", monthlyOutputIds},
		{"yearlyOutputIds", yearlyOutputIds},
		{"runOutputIds", runOutputIds},
		{"cropOutputIds", cropOutputIds},
		{"monthlyOutputIds", aoids},
		{"da", da.to_json()},
		{"debugMode", debugMode}
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


double applyOIdOP(OId::OP op, const vector<double>& vs)
{
	double v = vs.back();

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

	return v;
}

Json applyOIdOP(OId::OP op, const vector<Json>& js)
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

	auto build = [&](Result2 r, decltype(m.ofs)::mapped_type of)
	{
		m.ofs[r.id] = of;
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

			build({id++, "Date", "", "output current date"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.currentDate.toIsoDateString());
			});
			build({id++, "Month", "", "output current Month"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(int(m.currentDate.month()));
			});
			build({id++, "Year", "", "output current Year"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(int(m.currentDate.year()));
			});
			build({id++, "Crop", "", "crop name"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? m.mcg->get_CropName() : "");
			});

			build({id++, "TraDef", "0;1", "TranspirationDeficit"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_TranspirationDeficit(), 2) : 0.0);
			});

			build({id++, "Tra", "mm", "ActualTranspiration"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_ActualTranspiration(), 2) : 0.0);
			});

			build({id++, "NDef", "0;1", "CropNRedux"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CropNRedux(), 2) : 0.0);
			});
			build({id++, "HeatRed", "0;1", " HeatStressRedux"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_HeatStressRedux(), 2) : 0.0);
			});
			build({id++, "FrostRed", "0;1", "FrostStressRedux"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_FrostStressRedux(), 2) : 0.0);
			});
			build({id++, "OxRed", "0;1", "OxygenDeficit"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OxygenDeficit(), 2) : 0.0);
			});

			build({id++, "Stage", "", "DevelopmentalStage"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? m.mcg->get_DevelopmentalStage() + 1 : 0);
			});
			build({id++, "TempSum", "°Cd", "CurrentTemperatureSum"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CurrentTemperatureSum(), 1) : 0.0);
			});
			build({id++, "VernF", "0;1", "VernalisationFactor"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_VernalisationFactor(), 2) : 0.0);
			});
			build({id++, "DaylF", "0;1", "DaylengthFactor"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_DaylengthFactor(), 2) : 0.0);
			});
			build({id++, "IncRoot", "kg ha-1", "OrganGrowthIncrement root"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(0), 2) : 0.0);
			});
			build({id++, "IncLeaf", "kg ha-1", "OrganGrowthIncrement leaf"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(1), 2) : 0.0);
			});
			build({id++, "IncShoot", "kg ha-1", "OrganGrowthIncrement shoot"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(2), 2) : 0.0);
			});
			build({id++, "IncFruit", "kg ha-1", "OrganGrowthIncrement fruit"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_OrganGrowthIncrement(3), 2) : 0.0);
			});
			build({id++, "RelDev", "0;1", "RelativeTotalDevelopment"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_RelativeTotalDevelopment(), 2) : 0.0);
			});
			build({id++, "LT50", "°C", "LT50"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_LT50(), 1) : 0.0);
			});
			build({id++, "AbBiom", "kg ha-1", "AbovegroundBiomass"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AbovegroundBiomass(), 1) : 0.0);
			});
			build({id++, "OrgBiom", "kgDM ha-1", "get_OrganBiomass(i)"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i)
				{
					return m.cropPlanted && m.mcg->get_NumberOfOrgans() >= i
						? m.mcg->get_OrganBiomass(i) : 0.0;
				}, 1);
			});
			build({id++, "Yield", "kgDM ha-1", "get_PrimaryCropYield"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_PrimaryCropYield(), 1) : 0.0);
			});
			build({id++, "SumYield", "kgDM ha-1", "get_AccumulatedPrimaryCropYield"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AccumulatedPrimaryCropYield(), 1) : 0.0);
			});
			build({id++, "GroPhot", "kgCH2O ha-1", "GrossPhotosynthesisHaRate"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_GrossPhotosynthesisHaRate(), 4) : 0.0);
			});
			build({id++, "NetPhot", "kgCH2O ha-1", "NetPhotosynthesis"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_NetPhotosynthesis(), 2) : 0.0);
			});
			build({id++, "MaintR", "kgCH2O ha-1", "MaintenanceRespirationAS"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_MaintenanceRespirationAS(), 4) : 0.0);
			});
			build({id++, "GrowthR", "kgCH2O ha-1", "GrowthRespirationAS"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_GrowthRespirationAS(), 4) : 0.0);
			});
			build({id++, "StomRes", "s m-1", "StomataResistance"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_StomataResistance(), 2) : 0.0);
			});
			build({id++, "Height", "m", "CropHeight"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CropHeight(), 2) : 0.0);
			});
			build({id++, "LAI", "m2 m-2", "LeafAreaIndex"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_LeafAreaIndex(), 4) : 0.0);
			});
			build({id++, "RootDep", "layer#", "RootingDepth"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? m.mcg->get_RootingDepth() : 0);
			});
			build({id++, "EffRootDep", "m", "Effective RootingDepth"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->getEffectiveRootingDepth(), 2) : 0.0);
			});

			build({id++, "TotBiomN", "kgN ha-1", "TotalBiomassNContent"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_TotalBiomassNContent(), 1) : 0.0);
			});
			build({id++, "AbBiomN", "kgN ha-1", "AbovegroundBiomassNContent"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AbovegroundBiomassNContent(), 1) : 0.0);
			});
			build({id++, "SumNUp", "kgN ha-1", "SumTotalNUptake"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_SumTotalNUptake(), 2) : 0.0);
			});
			build({id++, "ActNup", "kgN ha-1", "ActNUptake"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_ActNUptake(), 2) : 0.0);
			});
			build({id++, "PotNup", "kgN ha-1", "PotNUptake"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_PotNUptake(), 2) : 0.0);
			});
			build({id++, "NFixed", "kgN ha-1", "NFixed"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_BiologicalNFixation(), 2) : 0.0);
			});
			build({id++, "Target", "kgN ha-1", "TargetNConcentration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_TargetNConcentration(), 3) : 0.0);
			});
			build({id++, "CritN", "kgN ha-1", "CriticalNConcentration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_CriticalNConcentration(), 3) : 0.0);
			});
			build({id++, "AbBiomNc", "kgN ha-1", "AbovegroundBiomassNConcentration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AbovegroundBiomassNConcentration(), 3) : 0.0);
			});
			build({id++, "YieldNc", "kgN ha-1", "PrimaryYieldNConcentration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_PrimaryYieldNConcentration(), 3) : 0.0);
			});

			build({id++, "Protein", "kg kg-1", "RawProteinConcentration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_RawProteinConcentration(), 3) : 0.0);
			});

			build({id++, "NPP", "kgC ha-1", "NPP"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_NetPrimaryProduction(), 5) : 0.0);
			});
			build({id++, "NPP-Organs", "kgC ha-1", "organ specific NPP"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i)
				{
					return m.cropPlanted && m.mcg->get_NumberOfOrgans() >= i
						? m.mcg->get_OrganSpecificNPP(i) : 0.0;
				}, 4);
			});

			build({id++, "GPP", "kgC ha-1", "GPP"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_GrossPrimaryProduction(), 5) : 0.0);
			});

			build({id++, "Ra", "kgC ha-1", "autotrophic respiration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(m.cropPlanted ? round(m.mcg->get_AutotrophicRespiration(), 5) : 0.0);
			});
			build({id++, "Ra-Organs", "kgC ha-1", "organ specific autotrophic respiration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i)
				{
					return m.cropPlanted && m.mcg->get_NumberOfOrgans() >= i
						? m.mcg->get_OrganSpecificTotalRespired(i) : 0.0;
				}, 4);
			});

			build({id++, "Mois", "m3 m-3", "Soil moisture content"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.moist.get_SoilMoisture(i); }, 3);
			});

			build({id++, "Irrig", "mm", "Irrigation"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica.dailySumIrrigationWater(), 1));
			});
			build({id++, "Infilt", "mm", "Infiltration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_Infiltration(), 1));
			});
			build({id++, "Surface", "mm", "Surface water storage"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_SurfaceWaterStorage(), 1));
			});
			build({id++, "RunOff", "mm", "Surface water runoff"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_SurfaceRunOff(), 1));
			});
			build({id++, "SnowD", "mm", "Snow depth"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_SnowDepth(), 1));
			});
			build({id++, "FrostD", "m", "Frost front depth in soil"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_FrostDepth(), 1));
			});
			build({id++, "ThawD", "m", "Thaw front depth in soil"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_ThawDepth(), 1));
			});

			build({id++, "PASW", "m3 m-3", "PASW"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results,
											[&](int i){ return m.moist.get_SoilMoisture(i) - m.soilc.at(i).vs_PermanentWiltingPoint(); },
											3);
			});

			build({id++, "SurfTemp", "°C", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.temp.get_SoilSurfaceTemperature(), 1));
			});
			build({id++, "STemp", "°C", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.temp.get_SoilTemperature(i); }, 1);
			});
			build({id++, "Act_Ev", "mm", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_ActualEvaporation(), 1));
			});
			build({id++, "Act_ET", "mm", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_Evapotranspiration(), 1));
			});
			build({id++, "ET0", "mm", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_ET0(), 1));
			});
			build({id++, "Kc", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_KcFactor(), 1));
			});
			build({id++, "AtmCO2", "ppm", "Atmospheric CO2 concentration"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica.get_AtmosphericCO2Concentration(), 0));
			});
			build({id++, "Groundw", "m", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica.get_GroundwaterDepth(), 2));
			});
			build({id++, "Recharge", "mm", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_GroundwaterRecharge(), 3));
			});
			build({id++, "NLeach", "kgN ha-1", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.trans.get_NLeaching(), 3));
			});
			build({id++, "NO3", "kgN m-3", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc.at(i).get_SoilNO3(); }, 3);
			});
			build({id++, "Carb", "kgN m-3", "Soil Carbamid"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.soilc.at(0).get_SoilCarbamid(), 4));
			});
			build({id++, "NH4", "kgN m-3", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc.at(i).get_SoilNH4(); }, 4);
			});
			build({id++, "NO2", "kgN m-3", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc.at(i).get_SoilNO2(); }, 4);
			});
			build({id++, "SOC", "kgC kg-1", "get_SoilOrganicC"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc.at(i).vs_SoilOrganicCarbon(); }, 4);
			});
			build({id++, "SOC-X-Y", "gC m-2", "SOC-X-Y"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i)
				{
					return m.soilc.at(i).vs_SoilOrganicCarbon()
						* m.soilc.at(i).vs_SoilBulkDensity()
						* m.soilc.at(i).vs_LayerThickness
						* 1000;
				}, 4);
			});

			build({id++, "AOMf", "kgC m-3", "get_AOM_FastSum"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_AOM_FastSum(i); }, 4);
			});
			build({id++, "AOMs", "kgC m-3", "get_AOM_SlowSum"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_AOM_SlowSum(i); }, 4);
			});
			build({id++, "SMBf", "kgC m-3", "get_SMB_Fast"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_SMB_Fast(i); }, 4);
			});
			build({id++, "SMBs", "kgC m-3", "get_SMB_Slow"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_SMB_Slow(i); }, 4);
			});
			build({id++, "SOMf", "kgC m-3", "get_SOM_Fast"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_SOM_Fast(i); }, 4);
			});
			build({id++, "SOMs", "kgC m-3", "get_SOM_Slow"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_SOM_Slow(i); }, 4);
			});
			build({id++, "CBal", "kgC m-3", "get_CBalance"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_CBalance(i); }, 4);
			});

			build({id++, "Nmin", "kgN ha-1", "NetNMineralisationRate"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_NetNMineralisationRate(i); }, 6);
			});
			build({id++, "NetNmin", "kgN ha-1", "NetNmin"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org.get_NetNMineralisation(), 5));
			});
			build({id++, "Denit", "kgN ha-1", "Denit"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org.get_Denitrification(), 5));
			});
			build({id++, "N2O", "kgN ha-1", "N2O"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org.get_N2O_Produced(), 5));
			});
			build({id++, "SoilpH", "", "SoilpH"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.soilc.at(0).get_SoilpH(), 1));
			});
			build({id++, "NEP", "kgC ha-1", "NEP"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org.get_NetEcosystemProduction(), 5));
			});
			build({id++, "NEE", "kgC ha-", "NEE"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org.get_NetEcosystemExchange(), 5));
			});
			build({id++, "Rh", "kgC ha-", "Rh"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org.get_DecomposerRespiration(), 5));
			});

			build({id++, "Tmin", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::tmin, m.timestep), 4));
			});
			build({id++, "Tavg", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::tavg, m.timestep), 4));
			});
			build({id++, "Tmax", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::tmax, m.timestep), 4));
			});
			build({id++, "Precip", "mm", "Precipitation"},
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::precip, m.timestep), 2));
			});
			build({id++, "Wind", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::wind, m.timestep), 4));
			});
			build({id++, "Globrad", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::globrad, m.timestep), 4));
			});
			build({id++, "Relhumid", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::relhumid, m.timestep), 4));
			});
			build({id++, "Sunhours", "", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.da.dataForTimestep(Climate::sunhours, m.timestep), 4));
			});

			build({id++, "BedGrad", "0;1", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.moist.get_PercentageSoilCoverage(), 3));
			});
			build({id++, "N", "kgN m-3", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.soilc.at(i).get_SoilNmin(); }, 3);
			});
			build({id++, "Co", "kgC m-3", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_SoilOrganicC(i); }, 2);
			});
			build({id++, "NH3", "kgN ha-1", "NH3_Volatilised"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.org.get_NH3_Volatilised(), 3));
			});
			build({id++, "NFert", "kgN ha-1", "dailySumFertiliser"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica.dailySumFertiliser(), 1));
			});
			build({id++, "WaterContent", "%nFC", "soil water content"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i)
				{
					double smm3 = m.moist.get_SoilMoisture(i); // soilc.at(i).get_Vs_SoilMoisture_m3();
					double fc = m.soilc.at(i).vs_FieldCapacity();
					double pwp = m.soilc.at(i).vs_PermanentWiltingPoint();
					return smm3 / (fc - pwp); //[%nFK]
				}, 4);
			});
			build({id++, "CapillaryRise", "mm", "capillary rise"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.moist.get_CapillaryRise(i); }, 3);
			});
			build({id++, "PercolationRate", "mm", "percolation rate"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.moist.get_PercolationRate(i); }, 3);
			});
			build({id++, "SMB-CO2-ER", "", "soilOrganic.get_SMB_CO2EvolutionRate"}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				store<double>(oid, results, [&](int i){ return m.org.get_SMB_CO2EvolutionRate(i); }, 1);
			});
			build({id++, "Evapotranspiration", "mm", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica.getEvapotranspiration(), 1));
			});
			build({id++, "Evaporation", "mm", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica.getEvaporation(), 1));
			});
			build({id++, "Transpiration", "mm", ""}, 
						[](MonicaRefs& m, BOTRes::ResultVector& results, OId oid)
			{
				results.push_back(round(m.monica.getTranspiration(), 1));
			});

			tableBuild = true;
		}
	}

	return m;
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
	{
		J11Array jvs;
		for(auto& vs : p.second)
			jvs.push_back(vs);
		crop[p.first] = jvs;
	}
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
			for(auto& j : p.second.array_items())
				out.crop[p.first].push_back(j.array_items());

	ci = msg.find("run");
	if(ci != msg.end())
		for(auto& j : ci->second.array_items())
			out.run.push_back(j);
}

//-----------------------------------------------------------------------------

void storeResults(const vector<OId>& outputIds,
									vector<BOTRes::ResultVector>& results,
									MonicaRefs& mr,
									int timestep,
									Date currentDate)
{
	const auto& ofs = buildOutputTable().ofs;
	auto mrr = mr.refresh(timestep, currentDate);

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
	if(activateDebug)
	{
		writeDebugInputs(env, "inputs.json");
	}

	Result res;
	res.customId = env.customId;

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

	vector<BOTRes::ResultVector> intermediateMonthlyResults;
	vector<BOTRes::ResultVector> intermediateYearlyResults;
	vector<BOTRes::ResultVector> intermediateRunResults;
	map<string, vector<BOTRes::ResultVector>> intermediateCropResults;
	map<Date, vector<BOTRes::ResultVector>> intermediateAtResults;
	
	MonicaRefs monicaRefs
	{
		monica, monica.soilTemperature(), monica.soilMoisture(), monica.soilOrganic(), monica.soilColumn(),
		monica.soilTransport(), monica.cropGrowth(), monica.isCropPlanted(), env.da, 0, currentDate
	};

	//if for some reason there are no applications (no nothing) in the
	//production process: quit
	if(!nextAbsoluteCMApplicationDate.isValid())
	{
		debug() << "start of production-process: " << currentCM.toString()
			<< " is not valid" << endl;
		return res;
	}

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

					//auto harvestApplication = make_unique<Harvest>(currentDate, currentPP.crop(), currentPP.cropResultPtr());
					auto harvestApplication =
						unique_ptr<Harvest>(new Harvest(currentDate,
																						currentCM.crop(),
																						currentCM.cropResultPtr()));
					harvestApplication->apply(&monica);
				}
			}
		}

		//there's something to at this day
		if(nextAbsoluteCMApplicationDate == currentDate)
		{
			debug() << "applying at: " << nextCMApplicationDate.toString()
				<< " absolute-at: " << nextAbsoluteCMApplicationDate.toString() << endl;
			//apply everything to do at current day
			//cout << currentPP.toString().c_str() << endl;
			currentCM.apply(nextCMApplicationDate, &monica);

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
		{
			monica.cropStep(currentDate, dateAndClimateDataP.second);
			storeResults(env.cropOutputIds, 
									 intermediateCropResults[monica.currentCrop()->id()],
									 monicaRefs, 
									 d, currentDate);
		}
		
		monica.generalStep(currentDate, dateAndClimateDataP.second);
		storeResults(env.dailyOutputIds, res.out.daily, monicaRefs, d, currentDate);

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
				res.out.monthly[currentMonth][i].push_back(applyOIdOP(oid.timeAggOp, intermediateMonthlyResults.at(i)));
				intermediateMonthlyResults[i].clear();
				++i;
			}

			currentMonth = currentDate.month();
		}
		else
			storeResults(env.monthlyOutputIds, intermediateMonthlyResults, monicaRefs, d, currentDate);

		// Yearly accumulated values
		if(currentDate.year() != (currentDate - 1).year() 
			 && d > 0)
		{
			size_t i = 0;
			res.out.yearly.resize(intermediateYearlyResults.size());
			for(auto oid : env.yearlyOutputIds)
			{
				res.out.yearly[i].push_back(applyOIdOP(oid.timeAggOp, intermediateYearlyResults.at(i)));
				intermediateYearlyResults[i].clear();
				++i;
			}
		}
		else
			storeResults(env.yearlyOutputIds, intermediateYearlyResults, monicaRefs, d, currentDate);

		storeResults(env.runOutputIds, intermediateRunResults, monicaRefs, d, currentDate);
	}

	//store/aggregate results for a single run
	size_t i = 0;
	res.out.run.resize(env.runOutputIds.size());
	for(auto oid : env.runOutputIds)
	{
		res.out.run[i] = applyOIdOP(oid.timeAggOp, intermediateRunResults.at(i));
		++i;
	}

	//store/aggregate results for a single crop
	for(auto& p : intermediateCropResults)
	{
		auto& vs = res.out.crop[p.first];
		i = 0;
		vs.resize(env.cropOutputIds.size());
		for(auto oid : env.cropOutputIds)
		{
			auto& ivs = p.second.at(i);
			if(ivs.front().is_string())
				vs[i].push_back(ivs.front());
			else
				vs[i].push_back(applyOIdOP(oid.timeAggOp, p.second.at(i)));
			++i;
		}
	}
	
	debug() << "returning from runMonica" << endl;
	return res;
}




