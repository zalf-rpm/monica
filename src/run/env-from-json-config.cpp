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
#include <set>

#include "env-from-json-config.h"
#include "tools/debug.h"
#include "../run/run-monica.h"
#include "../io/database-io.h"
#include "tools/json11-helper.h"
#include "tools/helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "soil/soil-from-db.h"
#include "../io/output.h"

using namespace std;
using namespace Monica;
using namespace json11;
using namespace Tools;
using namespace Climate;

Env Monica::createEnvFromJsonConfigFiles(std::map<std::string, std::string> params)
{
	Env env;
	if(!printPossibleErrors(env.merge(createEnvJsonFromJsonConfigFiles(params)), activateDebug))
		return Env();
	return env;
	
	/*
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
	//collect all errors in all files and don't stop as early as possible
	set<string> errors;
	for(auto& j : cropSiteSim)
	{
		addBasePath(j, pathToParameters);
		auto r = findAndReplaceReferences(j, j);
		if(r.success())
			cropSiteSim2.push_back(r.result);
		else
			errors.insert(r.errors.begin(), r.errors.end());
	}

	if(!errors.empty())
	{
		for(auto e : errors)
			cerr << e << endl;
		return Env();
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

	env.dailyOutputIds = parseOutputIds(simj["output"]["daily"].array_items());

	env.monthlyOutputIds = parseOutputIds(simj["output"]["monthly"].array_items());

	env.yearlyOutputIds = parseOutputIds(simj["output"]["yearly"].array_items());

	env.cropOutputIds = parseOutputIds(simj["output"]["crop"].array_items());

	if(simj["output"]["at"].is_object())
	{
		for(auto p : simj["output"]["at"].object_items())
		{
			Date d = Date::fromIsoDateString(p.first);
			if(d.isValid())
				env.atOutputIds[d] = parseOutputIds(p.second.array_items());
		}
	}

	env.runOutputIds = parseOutputIds(simj["output"]["run"].array_items());
	
	auto climateDataSettings = simj["climate.csv-options"].object_items();
	climateDataSettings["start-date"] = simj["start-date"];
	climateDataSettings["end-date"] = simj["end-date"];
	climateDataSettings["use-leap-years"] = simj["use-leap-years"];
	CSVViaHeaderOptions options(climateDataSettings);
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
	//*/
}

//-----------------------------------------------------------------------------

