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
#include "env-from-json-files.h"

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

	if(argc > 1)// && false)
	{
		map<string, string> params;
		if((argc - 1) % 2 == 0)
			for(int i = 1; i < argc; i += 2)
				params[toLower(argv[i])] = argv[i + 1];
		else
			return 1;

		activateDebug = stob(params["debug?:"], false);

		if(params["mode:"] == "hermes")
		{
			cout << "starting MONICA with old HERMES input files" << endl;
			Monica::runWithHermesData(fixSystemSeparator(params["path:"]));
			cout << "finished MONICA" << endl;
		}
		else
		{
			PARMParams ps;
			ps.startDate = params["start-date:"];
			ps.endDate = params["end-date:"];
			ps.name2path["output"] = params["output:"];

			auto path = params["path:"];
			auto projectName = params["project:"];
			if(projectName.empty())
				projectName += ".";

			auto names2suffixes = {{"climate", "csv"},
			                       {"crop", "json"},
			                       {"site", "json"},
			                       {"sim", "json"}};
			for(auto n2s : names2suffixes)
			{
				auto name = n2s.first;
				auto suffix = n2s.second;
				auto p = params[name + ":"];
				ps.name2path[name] = p.empty() ? path + "/" + projectName + name + "." +
				                                 suffix : p;
			}

			cout << "starting MONICA with JSON input files: " << endl;
			cout << "\tstartDate: " << (ps.startDate.isValid() ? ps.startDate.toIsoDateString() : "all") << endl;
			cout << "\tendDate: " << (ps.endDate.isValid() ? ps.endDate.toIsoDateString() : "all") << endl;
			for(auto n2s : names2suffixes)
			{
				auto name = n2s.first;
				cout << "\t" << name << "." << n2s.second << ": " << ps.name2path[name] << endl;
			}
			cout << endl;

			auto env = createEnvFromJsonConfigFiles(ps);
			auto res = runMonica(env);
			cout << "finished MONICA" << endl;
		}
	}
	
	//test();
	
	//writeDbParams();

	return 0;
}
