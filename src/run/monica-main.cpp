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

#include <stdio.h>
#include <iostream>
#include <fstream>

#include <string>

#include "db/abstract-db-connections.h"

#include "tools/debug.h"
#include "../run/run-monica.h"
#include "../io/database-io.h"
#include "../core/monica-typedefs.h"
#include "../core/monica.h"
#include "tools/json11-helper.h"
#include "climate/climate-file-io.h"
#include "../core/simulation.h"
#include "soil/conversion.h"
#include "env-from-json.h"

using namespace std;
using namespace Monica;
using namespace Tools;

#include "soil/soil.h"
void test()
{
	auto res = Soil::fcSatPwpFromKA5textureClass("fS",
																							 0,
																							 1.5*1000.0,
																							 0.8/100.0);

	//Json j = readAndParseFile("installer/Hohenfinow2/json/test.crop.json");
	//auto j2 = findAndReplaceReferences(j, j);
	//auto str = j2.dump();
}

void writeDbParams()
{
	//writeCropParameters("../monica-parameters/crops");
	//writeMineralFertilisers("../monica-parameters/mineral-fertilisers");
	//writeOrganicFertilisers("../monica-parameters/organic-fertilisers");
	//writeCropResidues("../monica-parameters/crop-residues");
	//writeUserParameters(MODE_HERMES, "../monica-parameters/user-parameters");
	//writeUserParameters(MODE_EVA2, "../monica-parameters/user-parameters");
	//writeUserParameters(MODE_MACSUR_SCALING, "../monica-parameters/user-parameters");
}

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	if(argc >= 1)// && false)
	{
		string pathToSimJson = "./sim.json";
		if(argc >= 2)
			pathToSimJson = argv[1];

		map<string, string> params;
		if(argc > 2)
		{
			int rem = (argc - 2) % 2;
			for (int i = 2; i < (argc - rem); i += 2)
				params[toLower(argv[i])] = argv[i + 1];
		}

		if(params["mode:"] == "hermes")
		{
			cout << "starting MONICA with old HERMES input files" << endl;
			Monica::runWithHermesData(fixSystemSeparator(params["path:"]));
			cout << "finished MONICA" << endl;
		}
		else
		{
			auto pathAndFile = splitPathToFile(pathToSimJson);
			auto pathOfSimJson = pathAndFile.first;

			auto simj = parseJsonString(readFile(pathToSimJson));
			auto simm = simj.object_items();

			if(!params["start-date:"].empty())
				simm["start-date"] = params["start-date:"];

			if(!params["end-date:"].empty())
				simm["end-date"] = params["end-date:"];

			if(!params["debug?:"].empty())
				simm["debug?"] = stob(params["debug?:"], simm["debug?"].bool_value());

			if(!params["write-output-files?:"].empty())
				simm["write-output-files?"] = stob(params["write-output-files?:"],
				                                   simm["write-output-files?"].bool_value());

			if(!params["path-to-output:"].empty())
				simm["path-to-output"] = params["path-to-output:"];

			simm["sim.json"] = pathToSimJson;

			if(!params["crop:"].empty())
				simm["crop.json"] = params["crop:"];
			auto pathToCropJson = simm["crop.json"].string_value();
			if(!isAbsolutePath(pathToCropJson))
				simm["crop.json"] = pathOfSimJson + pathToCropJson;

			if(!params["site:"].empty())
				simm["site.json"] = params["site:"];
			auto pathToSiteJson = simm["site.json"].string_value();
			if(!isAbsolutePath(pathToSiteJson))
				simm["site.json"] = pathOfSimJson + pathToSiteJson;

			if(!params["climate:"].empty())
				simm["climate.csv"] = params["climate:"];
			auto pathToClimateCSV = simm["climate.csv"].string_value();
			if(!isAbsolutePath(pathToClimateCSV))
				simm["climate.csv"] = pathOfSimJson + pathToClimateCSV;

			map<string, string> ps;
			ps["sim-json-str"] = json11::Json(simm).dump();
			ps["crop-json-str"] = readFile(simm["crop.json"].string_value());
			ps["site-json-str"] = readFile(simm["site.json"].string_value());
			ps["path-to-climate-csv"] = simm["climate.csv"].string_value();

			auto env = createEnvFromJsonConfigFiles(ps);
			activateDebug = env.debugMode;

			cout << "starting MONICA with JSON input files" << endl;
			auto res = runMonica(env);
			cout << "finished MONICA" << endl;
		}
	}
	
	//test();
	
	//writeDbParams();

	return 0;
}
