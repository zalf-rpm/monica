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

using namespace std;
using namespace Monica;
using namespace json11;
using namespace Tools;
using namespace Climate;

Json Monica::readAndParseJsonFile(string path)
{
	return parseJsonString(readFile(path));
}

Json Monica::parseJsonString(string jsonString)
{
	string err;
	Json j = Json::parse(jsonString, err);
	if(!err.empty())
		cerr << "Error parsing json object string: " << jsonString << err << endl;

	return j;
}

const map<string, function<pair<Json, bool>(const Json&, const Json&)>>& supportedPatterns();

Json findAndReplaceReferences(const Json& root, const Json& j)
{
	auto sp = supportedPatterns();

	//auto jstr = j.dump();

	if(j.is_array())
	{
		Json::array arr;

		bool arrayIsReferenceFunction = false;
		if(j[0].is_string())
		{
			auto p = sp.find(j[0].string_value());
			if(p != sp.end())
			{
				arrayIsReferenceFunction = true;

				//check for nested function invocations in the arguments
				Json::array funcArr;
				for(auto i : j.array_items())
					funcArr.push_back(findAndReplaceReferences(root, i));

				//invoke function
				auto jAndSuccess = (p->second)(root, funcArr);

				//if successful try to recurse into result for functions in result
				if(jAndSuccess.second)
					return findAndReplaceReferences(root, jAndSuccess.first);
			}
		}

		if(!arrayIsReferenceFunction)
			for(auto jv : j.array_items())
				arr.push_back(findAndReplaceReferences(root, jv));

		return arr;
	}
	else if(j.is_object())
	{
		Json::object obj;

		for(auto p : j.object_items())
			obj[p.first] = findAndReplaceReferences(root, p.second);

		return obj;
	}

	return j;
}

const map<string, function<pair<Json, bool>(const Json&, const Json&)>>& supportedPatterns()
{
	auto ref = [](const Json& root, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 3
		   && j[1].is_string()
		   && j[2].is_string())
			return make_pair(root[j[1].string_value()][j[2].string_value()], true);
		return make_pair(j, false);
	};

	auto fromDb = [](const Json&, const Json& j) -> pair<Json, bool>
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
				return make_pair(getMineralFertiliserParametersFromMonicaDB(name, db).to_json(),
				                 true);
			}
			else if(type == "organic_fertiliser")
			{
				if(db.empty())
					db = "monica";
				auto name = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return make_pair(getOrganicFertiliserParametersFromMonicaDB(name, db)->to_json(),
				                 true);
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
				return make_pair(getResidueParametersFromMonicaDB(species,
				                                                  residueType,
				                                                  db)->to_json(),
				                 true);
			}
			else if(type == "species")
			{
				if(db.empty())
					db = "monica";
				auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
				return make_pair(getSpeciesParametersFromMonicaDB(species, db)->to_json(),
				                 true);
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
				return make_pair(getCultivarParametersFromMonicaDB(species,
				                                                   cultivar,
				                                                   db)->to_json(),
				                 true);
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
				return make_pair(getCropParametersFromMonicaDB(species,
				                                               cultivar,
				                                               db)->to_json(),
				                 true);
			}
			else if(type == "soil-temperature-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return make_pair(readUserSoilTemperatureParametersFromDatabase(module, db).to_json(),
				                 true);
			}
			else if(type == "environment-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return make_pair(readUserEnvironmentParametersFromDatabase(module, db).to_json(),
				                 true);
			}
			else if(type == "soil-organic-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return make_pair(readUserSoilOrganicParametersFromDatabase(module, db).to_json(),
				                 true);
			}
			else if(type == "soil-transport-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return make_pair(readUserSoilTransportParametersFromDatabase(module, db).to_json(),
				                 true);
			}
			else if(type == "soil-moisture-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return make_pair(readUserSoilTemperatureParametersFromDatabase(module, db).to_json(),
				                 true);
			}
			else if(type == "crop-params")
			{
				if(db.empty())
					db = "monica";
				auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
				return make_pair(readUserCropParametersFromDatabase(module, db).to_json(),
				                 true);
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

				return make_pair(spjs, true);
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
					return make_pair(sps->at(layerNo - 1).to_json(), true);

				return make_pair(j, false);
			}
		}

		return make_pair(j, false);
	};

	auto fromFile = [](const Json& root, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
		   && j[1].is_string())
		{
			string basePath = string_valueD(root, "base-path", ".");
			string pathToFile = j[1].string_value();
			pathToFile = isAbsolutePath(pathToFile)
			             ? pathToFile
			             : basePath + "/" + pathToFile;
			auto jo = readAndParseJsonFile(pathToFile);
			if(!jo.is_null())
				return make_pair(jo, true);
		}

		return make_pair(j, false);
	};

	auto humus2corg = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
		   && j[1].is_number())
			return make_pair(Soil::humus_st2corg(j[1].int_value()), true);
		return make_pair(j, false);
	};

	auto ld2trd = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 3
		   && j[1].is_number()
		   && j[2].is_number())
			return make_pair(Soil::ld_eff2trd(j[1].int_value(), j[2].number_value()),
			                 true);
		return make_pair(j, false);
	};

	auto KA52clay = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
		   && j[1].is_string())
			return make_pair(Soil::KA5texture2clay(j[1].string_value()), true);
		return make_pair(j, false);
	};

	auto KA52sand = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
		   && j[1].is_string())
			return make_pair(Soil::KA5texture2sand(j[1].string_value()), true);
		return make_pair(j, false);
	};

	auto sandClay2lambda = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 3
		   && j[1].is_number()
		   && j[2].is_number())
			return make_pair(Soil::sandAndClay2lambda(j[1].number_value(),
			                                          j[2].number_value()),
			                 true);
		return make_pair(j, false);
	};

	auto percent = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
		   && j[1].is_number())
			return make_pair(j[1].number_value() / 100.0, true);
		return make_pair(j, false);
	};

	static map<string, function<pair<Json, bool>(const Json&,const Json&)>> m{
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
		cropSiteSim.push_back(parseJsonString(params[name]));

	vector<Json> cropSiteSim2;
	for(auto& j : cropSiteSim)
		cropSiteSim2.push_back(findAndReplaceReferences(j, j));

	//for(auto j : cropSiteSim2)
	//{
	//	auto str = j.dump();
		//cout << str << endl;
	//}

	//cout << cropSiteSim2[1].dump() << endl;

	auto cropj = cropSiteSim2.at(0);
	auto sitej = cropSiteSim2.at(1);
	auto simj = cropSiteSim2.at(2);

	Env env;

	//store debug mode in env, take from sim.json, but prefer params map
	env.debugMode = simj["debug?"].bool_value();

	env.params.userEnvironmentParameters.merge(sitej["EnvironmentParameters"]);
	env.params.userCropParameters.merge(cropj["CropParameters"]);
	env.params.userSoilTemperatureParameters.merge(sitej["SoilTemperatureParameters"]);
	env.params.userSoilTransportParameters.merge(sitej["SoilTransportParameters"]);
	env.params.userSoilOrganicParameters.merge(sitej["SoilOrganicParameters"]);
	env.params.userSoilMoistureParameters.merge(sitej["SoilMoistureParameters"]);
	env.params.userSoilMoistureParameters.getCapillaryRiseRate =
			[](string soilTexture, int distance)
			{
				return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
			};
	env.params.siteParameters.merge(sitej["SiteParameters"]);
	env.params.simulationParameters.merge(simj);

	for(Json cmj : cropj["cropRotation"].array_items())
		env.cropRotation.push_back(cmj);

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
	cout << "startDate: " << options.startDate.toIsoDateString()
	<< " endDate: " << options.endDate.toIsoDateString()
	<< " use leap years?: " << (options.startDate.useLeapYears() ? "true" : "false")
	<< endl;

	env.da = readClimateDataFromCSVFileViaHeaders(simj["climate.csv"].string_value(),
	                                              options);

	if(!env.da.isValid())
		return Env();

	env.params.setWriteOutputFiles(simj["write-output-files?"].bool_value());
	env.params.setPathToOutputDir(simj["path-to-output"].string_value());

	return env;
}

