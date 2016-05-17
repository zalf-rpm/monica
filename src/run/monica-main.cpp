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
	string monicaVersion = "2.1";

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	if(argc >= 1)// && false)
	{
		bool debug = false, debugSet = false;
		string startDate, endDate;
		bool writeOutputFiles = false, writeOutputFilesSet = false;
		string pathToOutput;
		bool hermesMode = false;
		string pathToSimJson = "./sim.json",crop, site, climate;

		for(auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if(arg == "-d" || arg == "--debug")
				debug = debugSet = true;
			else if(arg == "--hermes")
				hermesMode = true;
			else if((arg == "-s" || arg == "--start-date")
			        && i+1 < argc)
				startDate = argv[++i];
			else if((arg == "-e" || arg == "--end-date")
			        && i+1 < argc)
				startDate = argv[++i];
			else if(arg == "-w" || arg == "--write-output-files")
				writeOutputFiles = writeOutputFilesSet = true;
			else if((arg == "-o" || arg == "--path-to-output")
			        && i+1 < argc)
				pathToOutput = argv[++i];
			else if((arg == "-c" || arg == "--path-to-crop")
			        && i+1 < argc)
				crop = argv[++i];
			else if((arg == "-s" || arg == "--path-to-site")
			        && i+1 < argc)
				site = argv[++i];
			else if((arg == "-cl" || arg == "--path-to-climate")
			        && i+1 < argc)
				climate = argv[++i];
			else if(arg == "-h" || arg == "--help")
			{
				cout << "./monica [-d | --debug]\t\t\t ... show debug outputs" << endl
				<< "\t [--hermes]\t\t\t ... use old hermes format files" << endl
				<< "\t [-s | --start-date]\t\t ... date in iso-date-format yyyy-mm-dd" << endl
				<< "\t [-e | --end-date]\t\t ... date in iso-date-format yyyy-mm-dd" << endl
				<< "\t [-w | --write-output-files]\t ... write MONICA output files (rmout, smout)" << endl
				<< "\t [-o | --path-to-output]\t ... path to output directory" << endl
				<< "\t [-c | --path-to-crop]\t\t ... path to crop.json file" << endl
				<< "\t [-s | --path-to-site]\t\t ... path to site.json file" << endl
				<< "\t [-w | --path-to-climate]\t ... path to climate.csv" << endl
				<< "\t [-h | --help]\t\t\t ... this help output" << endl
				<< "\t [-v | --version]\t\t ... outputs MONICA version" << endl
					<< "\t path-to-sim-json ... path to sim.json file" << endl;
			  exit(0);
			}
			else if(arg == "-v" || arg == "--version")
				cout << "MONICA version " << monicaVersion << endl, exit(0);
			else
				pathToSimJson = argv[i];
		}

		if(hermesMode)
		{
			if(debug)
				cout << "starting MONICA with old HERMES input files" << endl;

			Monica::runWithHermesData(fixSystemSeparator(pathToSimJson), debug);

			if(debug)
				cout << "finished MONICA" << endl;
		}
		else
		{
			auto pathAndFile = splitPathToFile(pathToSimJson);
			auto pathOfSimJson = pathAndFile.first;

			auto simj = parseJsonString(readFile(pathToSimJson));
			auto simm = simj.object_items();

			if(!startDate.empty())
				simm["start-date"] = startDate;

			if(!endDate.empty())
				simm["end-date"] = endDate;

			if(debugSet)
				simm["debug?"] = debug;

			if(writeOutputFilesSet)
				simm["write-output-files?"] = writeOutputFiles;

			if(!pathToOutput.empty())
				simm["path-to-output"] = pathToOutput;

			simm["sim.json"] = pathToSimJson;

			if(!crop.empty())
				simm["crop.json"] = crop;
			auto pathToCropJson = simm["crop.json"].string_value();
			if(!isAbsolutePath(pathToCropJson))
				simm["crop.json"] = pathOfSimJson + pathToCropJson;

			if(!site.empty())
				simm["site.json"] = site;
			auto pathToSiteJson = simm["site.json"].string_value();
			if(!isAbsolutePath(pathToSiteJson))
				simm["site.json"] = pathOfSimJson + pathToSiteJson;

			if(!climate.empty())
				simm["climate.csv"] = climate;
			auto pathToClimateCSV = simm["climate.csv"].string_value();
			if(!isAbsolutePath(pathToClimateCSV))
				simm["climate.csv"] = pathOfSimJson + pathToClimateCSV;

			map<string, string> ps;
			ps["sim-json-str"] = json11::Json(simm).dump();
			ps["crop-json-str"] = readFile(simm["crop.json"].string_value());
			ps["site-json-str"] = readFile(simm["site.json"].string_value());
			//ps["path-to-climate-csv"] = simm["climate.csv"].string_value();

			auto env = createEnvFromJsonConfigFiles(ps);
			activateDebug = env.debugMode;
			
			// open rmout.dat
			//debug() << "Outputpath: " << (env.params.pathToOutputDir() + pathSeparator() + "rmout.csv") << endl;
			auto fout = make_shared<ofstream>();
			env.fout = fout;
			fout->open(ensureDirExists(env.params.pathToOutputDir() + pathSeparator()) + "rmout.csv");
			if(fout->fail())
			{
				cerr << "Error while opening output file \"" << (env.params.pathToOutputDir() + pathSeparator() + "rmout.csv") << "\"" << endl;
				//return res;
			}

			// open smout.dat
			auto gout = make_shared<ofstream>();
			env.gout = gout;
			gout->open(ensureDirExists(env.params.pathToOutputDir() + pathSeparator()) + "smout.csv");
			if(gout->fail())
			{
				cerr << "Error while opening output file \"" << (env.params.pathToOutputDir() + pathSeparator() + "smout.csv").c_str() << "\"" << endl;
				//return res;
			}

			if(activateDebug)
				cout << "starting MONICA with JSON input files" << endl;

			auto res = runMonica(env);

			if(gout)
				gout->close();
			if(fout)
				fout->close();

			if(activateDebug)
				cout << "finished MONICA" << endl;
		}
	}
	
	//test();
	
	//writeDbParams();

	return 0;
}
