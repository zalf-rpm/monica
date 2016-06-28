/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <iostream>
#include <fstream>
#include <string>

#include "env-from-json.h"
#include "tools/debug.h"
#include "../run/run-monica.h"
#include "../io/database-io.h"
#include "../core/monica-typedefs.h"
#include "../core/monica.h"
#include "tools/json11-helper.h"
#include "tools/helper.h"
#include "climate/climate-file-io.h"
#include "../core/simulation.h"
#include "soil/conversion.h"
#include "soil/soil-from-db.h"
#include "../core/monica-parameters.h"

using namespace std;
using namespace Monica;
using namespace json11;
using namespace Tools;
using namespace Climate;

EResult<Json> Monica::readAndParseJsonFile(string path)
{
	auto r = readFile(path);
	if(r.success())
		return parseJsonString(r.result);
	return{Json(), r.errors};
}

EResult<Json> Monica::parseJsonString(string jsonString)
{
	string err;
	Json j = Json::parse(jsonString, err);
	if(!err.empty())
		return{j, string("Error parsing JSON object: '") + jsonString + "' "};
	return{j};
}

/*
struct JsonAndErrors 
{
	JsonAndErrors(Json j) : json(j) {}
	JsonAndErrors(string e) { errors.push_back(e); }
	JsonAndErrors(vector<string> es) : errors(es) {}
	JsonAndErrors(Json j, string e) : json(j) { errors.push_back(e); }
	JsonAndErrors(Json j, vector<string> es) : json(j), errors(es) {}

	bool success() const { return errors.empty(); }

	Json json;
	vector<string> errors;
};
*/

typedef EResult<Json> JsonAndErrors;

const map<string, function<JsonAndErrors(const Json&, const Json&)>>& supportedPatterns();

//! returns pair<Json, success?>
JsonAndErrors findAndReplaceReferences(const Json& root, const Json& j)
{
	auto sp = supportedPatterns();

	//auto jstr = j.dump();
	bool success = true;
	vector<string> errors;

	if(j.is_array())
	{
		J11Array arr;

		bool arrayIsReferenceFunction = false;
		if(j[0].is_string())
		{
			auto p = sp.find(j[0].string_value());
			if(p != sp.end())
			{
				arrayIsReferenceFunction = true;

				//check for nested function invocations in the arguments
				J11Array funcArr;
				for(auto i : j.array_items())
				{
					auto r = findAndReplaceReferences(root, i);
					success = success && r.success();
					if(!r.success())
						for(auto e : r.errors)
							errors.push_back(e);
					funcArr.push_back(r.result);
				}

				//invoke function
				auto jaes = (p->second)(root, funcArr);

				success = success && jaes.success();
				if(!jaes.success())
					for(auto e : jaes.errors)
						errors.push_back(e);

				//if successful try to recurse into result for functions in result
				if(jaes.success())
				{
					auto r = findAndReplaceReferences(root, jaes.result);
					success = success && r.success();
					if(!r.success())
						for(auto e : r.errors)
							errors.push_back(e);
					return{r.result, errors};
				}
				else
					return{J11Object(), errors};
			}
		}

		if(!arrayIsReferenceFunction)
			for(auto jv : j.array_items())
			{
				auto r = findAndReplaceReferences(root, jv);
				success = success && r.success();
				if(!r.success())
					for(auto e : r.errors)
						errors.push_back(e);
				arr.push_back(r.result);
			}

		return{arr, errors};
	}
	else if(j.is_object())
	{
		J11Object obj;

		for(auto p : j.object_items())
		{
			auto r = findAndReplaceReferences(root, p.second);
			success = success && r.success();
			if(!r.success())
				for(auto e : r.errors)
					errors.push_back(e);
			obj[p.first] = r.result;
		}

		return{obj, errors};
	}

	return{j, errors};
}

const map<string, function<JsonAndErrors(const Json&, const Json&)>>& supportedPatterns()
{
	auto ref = [](const Json& root, const Json& j) -> JsonAndErrors
	{
		if(j.array_items().size() == 3
		   && j[1].is_string()
		   && j[2].is_string())
			return{root[j[1].string_value()][j[2].string_value()]};
		return{j, string("Couldn't resolve reference: ") + j.dump() + "!"};
	};

	auto fromDb = [](const Json&, const Json& j) -> JsonAndErrors
	{
		if((j.array_items().size() >= 3 && j[1].is_string())
		   || (j.array_items().size() == 2 && j[1].is_object()))
		{
			bool isParamMap = j[1].is_object();

			auto type = isParamMap ? j[1]["type"].string_value() :j[1].string_value();
			string err;
			string db = isParamMap && j[1].has_shape({{"db", Json::STRING}}, err)
			            ? j[1]["db"].string_value()
			            : "";
			if(type == "mineral_fertiliser")
			{
				if(db.empty())
					db = "monica";
				auto name = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{getMineralFertiliserParametersFromMonicaDB(name, db).to_json()};
			}
			else if(type == "organic_fertiliser")
			{
				if(db.empty())
					db = "monica";
				auto name = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{getOrganicFertiliserParametersFromMonicaDB(name, db)->to_json()};
			}
			else if(type == "crop_residue"
			        && j.array_items().size() >= 3)
			{
				if(db.empty())
					db = "monica";
				auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
				auto residueType = isParamMap ? j[1]["residue-type"].string_value()
				                              : j.array_items().size() == 4 ? j[3].string_value()
				                                                            : "";
				return{getResidueParametersFromMonicaDB(species, residueType, db)->to_json()};
			}
			else if(type == "species")
			{
				if(db.empty())
					db = "monica";
				auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
				return{getSpeciesParametersFromMonicaDB(species, db)->to_json()};
			}
			else if(type == "cultivar"
			        && j.array_items().size() >= 3)
			{
				if(db.empty())
					db = "monica";
				auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
				auto cultivar = isParamMap ? j[1]["cultivar"].string_value()
				                           : j.array_items().size() == 4 ? j[3].string_value()
				                                                         : "";
				return{getCultivarParametersFromMonicaDB(species, cultivar, db)->to_json()};
			}
			else if(type == "crop"
			        && j.array_items().size() >= 3)
			{
				if(db.empty())
					db = "monica";
				auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
				auto cultivar = isParamMap ? j[1]["cultivar"].string_value()
				                           : j.array_items().size() == 4 ? j[3].string_value()
				                                                         : "";
				return{getCropParametersFromMonicaDB(species, cultivar, db)->to_json()};
			}
			else if(type == "soil-temperature-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{readUserSoilTemperatureParametersFromDatabase(module, db).to_json()};
			}
			else if(type == "environment-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{readUserEnvironmentParametersFromDatabase(module, db).to_json()};
			}
			else if(type == "soil-organic-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{readUserSoilOrganicParametersFromDatabase(module, db).to_json()};
			}
			else if(type == "soil-transport-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{readUserSoilTransportParametersFromDatabase(module, db).to_json()};
			}
			else if(type == "soil-moisture-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{readUserSoilTemperatureParametersFromDatabase(module, db).to_json()};
			}
			else if(type == "crop-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return{readUserCropParametersFromDatabase(module, db).to_json()};
			}
			else if(type == "soil-profile"
			        && (isParamMap
			            || (!isParamMap && j[2].is_number())))
			{
				if(db.empty())
					db = "soil";
				vector<Json> spjs;
				int profileId = isParamMap ? j[1]["id"].int_value() : j[2].int_value();
				auto sps = Soil::soilParameters(db, profileId);
				for(auto sp : *sps)
					spjs.push_back(sp.to_json());

				return{spjs};
			}
			else if(type == "soil-layer"
			        && (isParamMap
			            || (j.array_items().size() == 4
			                && j[2].is_number()
			                && j[3].is_number())))
			{
				if(db.empty())
					db = "soil";
				int profileId = isParamMap ? j[1]["id"].int_value() : j[2].int_value();
				size_t layerNo = size_t(isParamMap ? j[1]["no"].int_value() : j[3].int_value());
				auto sps = Soil::soilParameters(db, profileId);
				if(0 < layerNo && layerNo <= sps->size())
					return{sps->at(layerNo - 1).to_json()};

				return{j, string("Couldn't load soil-layer from database: ") + j.dump() + "!"};
			}
		}

		return{j, string("Couldn't load data from DB: ") + j.dump() + "!"};
	};

	auto fromFile = [](const Json& root, const Json& j) -> JsonAndErrors
	{
		string error;

		if(j.array_items().size() == 2
		   && j[1].is_string())
		{
			string basePath = string_valueD(root, "include-file-base-path", ".");
			string pathToFile = j[1].string_value();
			pathToFile = fixSystemSeparator(isAbsolutePath(pathToFile)
																			? pathToFile
																			: basePath + "/" + pathToFile);
			auto jo = readAndParseJsonFile(pathToFile);
			if(jo.success() && !jo.result.is_null())
				return{jo.result};
			
			return{j, string("Couldn't include file with path: '") + pathToFile + "'!"};
		}

		return{j, string("Couldn't include file with function: ") + j.dump() + "!"};
	};

	auto humus2corg = [](const Json&, const Json& j) -> JsonAndErrors
	{
		if(j.array_items().size() == 2
		   && j[1].is_number())
			return{Soil::humus_st2corg(j[1].int_value())};
		return{j, string("Couldn't convert humus level to corg: ") + j.dump() + "!"};
	};

	auto ld2trd = [](const Json&, const Json& j) -> JsonAndErrors
	{
		if(j.array_items().size() == 3
		   && j[1].is_number()
		   && j[2].is_number())
			return{Soil::ld_eff2trd(j[1].int_value(), j[2].number_value())};
		return{j, string("Couldn't convert bulk density class to raw density using function: ") + j.dump() + "!"};
	};

	auto KA52clay = [](const Json&, const Json& j) -> JsonAndErrors
	{
		if(j.array_items().size() == 2
		   && j[1].is_string())
			return{Soil::KA5texture2clay(j[1].string_value())};
		return{j, string("Couldn't get soil clay content from KA5 soil class: ") + j.dump() + "!"};
	};

	auto KA52sand = [](const Json&, const Json& j) -> JsonAndErrors
	{
		if(j.array_items().size() == 2
		   && j[1].is_string())
			return{Soil::KA5texture2sand(j[1].string_value())};
		return{j, string("Couldn't get soil sand content from KA5 soil class: ") + j.dump() + "!"};;
	};

	auto sandClay2lambda = [](const Json&, const Json& j) -> JsonAndErrors
	{
		if(j.array_items().size() == 3
		   && j[1].is_number()
		   && j[2].is_number())
			return{Soil::sandAndClay2lambda(j[1].number_value(), j[2].number_value())};
		return{j, string("Couldn't get lambda value from soil sand and clay content: ") + j.dump() + "!"};
	};

	auto percent = [](const Json&, const Json& j) -> JsonAndErrors
	{
		if(j.array_items().size() == 2
		   && j[1].is_number())
			return{j[1].number_value() / 100.0};
		return{j, string("Couldn't convert percent to decimal percent value: ") + j.dump() + "!"};
	};

	static map<string, function<JsonAndErrors(const Json&,const Json&)>> m{
			{"include-from-db", fromDb},
			{"include-from-file", fromFile},
			{"ref", ref},
			{"humus_st2corg", humus2corg},
			{"ld_eff2trd", ld2trd},
			{"KA5TextureClass2clay", KA52clay},
			{"KA5TextureClass2sand", KA52sand},
			{"sandAndClay2lambda", sandClay2lambda},
			{"%", percent}};
	return m;
}

Env Monica::createEnvFromJsonConfigFiles(std::map<std::string, std::string> params)
{
	vector<Json> cropSiteSim;
	for(auto name : {"crop-json-str", "site-json-str", "sim-json-str"})
		cropSiteSim.push_back(printPossibleErrors(parseJsonString(params[name])));

	for(auto& j : cropSiteSim)
		if(j.is_null())
			return Env();

	string pathToParameters = cropSiteSim.at(2)["include-file-base-path"].string_value();

	auto addBasePath = [&](Json& j, string basePath)
	{
		string err;
		if(!j.has_shape({{"include-file-base-path", Json::STRING}}, err))
		{
			auto m = j.object_items();
			m["include-file-base-path"] = pathToParameters;
			j = m;
		}
	};

	vector<Json> cropSiteSim2;
	for(auto& j : cropSiteSim)
	{
		addBasePath(j, pathToParameters);
		auto r = findAndReplaceReferences(j, j);
		if(r.success())
			cropSiteSim2.push_back(r.result);
		else
		{
			for(auto e : r.errors)
				cerr << e << endl;
			return Env();
		}
	}

	auto cropj = cropSiteSim2.at(0);
	auto sitej = cropSiteSim2.at(1);
	auto simj = cropSiteSim2.at(2);

	Env env;
	
	//store debug mode in env, take from sim.json, but prefer params map
	env.debugMode = simj["debug?"].bool_value();

	bool success = true;
	success = success && printPossibleErrors(env.params.userEnvironmentParameters.merge(sitej["EnvironmentParameters"]), activateDebug);
	success = success && printPossibleErrors(env.params.userCropParameters.merge(cropj["CropParameters"]), activateDebug);
	success = success && printPossibleErrors(env.params.userSoilTemperatureParameters.merge(sitej["SoilTemperatureParameters"]), activateDebug);
	success = success && printPossibleErrors(env.params.userSoilTransportParameters.merge(sitej["SoilTransportParameters"]), activateDebug);
	success = success && printPossibleErrors(env.params.userSoilOrganicParameters.merge(sitej["SoilOrganicParameters"]), activateDebug);
	success = success && printPossibleErrors(env.params.userSoilMoistureParameters.merge(sitej["SoilMoistureParameters"]), activateDebug);
	env.params.userSoilMoistureParameters.getCapillaryRiseRate =
			[](string soilTexture, int distance)
			{
				return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
			};
	success = success && printPossibleErrors(env.params.siteParameters.merge(sitej["SiteParameters"]), activateDebug);
	success = success && printPossibleErrors(env.params.simulationParameters.merge(simj), activateDebug);

	for(Json cmj : cropj["cropRotation"].array_items())
	{
		CultivationMethod cm;
		success = success && printPossibleErrors(cm.merge(cmj), activateDebug);
		env.cropRotation.push_back(cm);
	}

	if(!success)
		return Env();

	for(Json idj : simj["output"]["daily"].array_items())
	{
		if(idj.is_string())
		{
			string name = idj.string_value();
			auto it = result3().find(name);
			if(it != result3().end())
			{
				auto data = it->second;
				env.outputIds.push_back(OId(data.id));
				env.outputId2nameAndUnit[data.id] = make_pair(data.name, data.unit);
			}
		}
		else if(idj.is_array())
		{
			auto arr = idj.array_items();
			if(arr.size() >= 1)
			{
				OId oid;
				if(arr.size() >= 3 && arr[1].is_number() && arr[2].is_number())
				{
					oid.from = arr[1].int_value() - 1;
					oid.to = arr[2].int_value() - 1;
				}
				if(arr.size() >= 4 && arr[3].is_string())
				{
					string ops = arr[3].string_value();
					if(toUpper(ops) == "SUM")
						oid.op = OId::SUM;
					else if(toUpper(ops) == "AVG")
						oid.op = OId::AVG;
				}

				string name = arr[0].string_value();
				auto it = result3().find(name);
				if(it != result3().end())
				{
					auto data = it->second;
					oid.id = data.id;
					env.outputIds.push_back(oid);
					env.outputId2nameAndUnit[data.id] = make_pair(data.name, data.unit);
				}
			}
		}
	}
	
	//get no of climate file header lines from sim.json, but prefer from params map
	auto climateDataSettings = simj["climate.csv-options"];
	map<string, string> headerNames;
	for(auto p : climateDataSettings["header-to-acd-names"].object_items())
		headerNames[p.first] = p.second.string_value();

	CSVViaHeaderOptions options;
	options.separator = string_valueD(climateDataSettings, "csv-separator", ",");
	options.noOfHeaderLines = size_t(int_valueD(climateDataSettings, "no-of-climate-file-header-lines", 2));
	options.headerName2ACDName = headerNames;

	//add start/end date to sim json object
	set_iso_date_value(options.startDate, simj, "start-date");
	set_iso_date_value(options.endDate, simj, "end-date");
	debug() << "startDate: " << options.startDate.toIsoDateString()
		<< " endDate: " << options.endDate.toIsoDateString()
		<< " use leap years?: " << (options.startDate.useLeapYears() ? "true" : "false")
		<< endl;

	env.da = readClimateDataFromCSVFileViaHeaders(simj["climate.csv"].string_value(),
	                                              options);

	if(!env.da.isValid())
		return Env();

	//env.params.setWriteOutputFiles(simj["output"]["write-file?"].bool_value());
	env.params.setPathToOutputDir(simj["path-to-output"].string_value());

	return env;
}

