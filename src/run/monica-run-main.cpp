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
#include <tuple>

#include "json11/json11.hpp"

#include "tools/helper.h"
#include "tools/debug.h"
#include "../run/run-monica.h"
#include "tools/json11-helper.h"
#include "env-from-json-config.h"
#include "tools/algorithms.h"
#include "../io/csv-format.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;

string appName = "monica-run";
string version = "2.0.0-beta";

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	auto printHelp = [=]()
	{
		cout
			<< appName << endl
			<< " [-d | --debug] ... show debug outputs" << endl
			<< " [[-sd | --start-date] ISO-DATE (default: start of given climate data)] ... date in iso-date-format yyyy-mm-dd" << endl
			<< " [[-ed | --end-date] ISO-DATE (default: end of given climate data)] ... date in iso-date-format yyyy-mm-dd" << endl
			<< " [-w | --write-output-files] ... write MONICA output files (rmout, smout)" << endl
			<< " [[-op | --path-to-output] DIRECTORY (default: .)] ... path to output directory" << endl
			<< " [[-o | --path-to-output-file] FILE (default: ./rmout.csv)] ... path to output file" << endl
			<< " [[-do | --daily-outputs] [LIST] (default: value of key 'sim.json:output.daily')] ... list of daily output elements" << endl
			<< " [[-c | --path-to-crop] FILE (default: ./crop.json)] ... path to crop.json file" << endl
			<< " [[-s | --path-to-site] FILE (default: ./site.json)] ... path to site.json file" << endl
			<< " [[-w | --path-to-climate] FILE (default: ./climate.csv)] ... path to climate.csv" << endl
			<< " [-h | --help] ... this help output" << endl
			<< " [-v | --version] ... outputs " << appName << " version" << endl
			<< " path-to-sim-json ... path to sim.json file" << endl;
	};


	if(argc > 1)
	{
		bool debug = false, debugSet = false;
		string startDate, endDate;
		string pathToOutput;
		string pathToOutputFile;
		bool writeOutputFile = false;
		string pathToSimJson = "./sim.json", crop, site, climate;
		string dailyOutputs;

		for(auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if(arg == "-d" || arg == "--debug")
				debug = debugSet = true;
			else if((arg == "-sd" || arg == "--start-date")
			        && i+1 < argc)
				startDate = argv[++i];
			else if((arg == "-ed" || arg == "--end-date")
			        && i+1 < argc)
				endDate = argv[++i];
			else if((arg == "-op" || arg == "--path-to-output")
			        && i+1 < argc)
				pathToOutput = argv[++i];
			else if((arg == "-o" || arg == "--path-to-output-file")
							&& i + 1 < argc)
				pathToOutputFile = argv[++i];
			else if((arg == "-do" || arg == "--daily-outputs")
							&& i + 1 < argc)
				dailyOutputs = argv[++i];
			else if((arg == "-c" || arg == "--path-to-crop")
			        && i+1 < argc)
				crop = argv[++i];
			else if((arg == "-s" || arg == "--path-to-site")
			        && i+1 < argc)
				site = argv[++i];
			else if((arg == "-w" || arg == "--path-to-climate")
			        && i+1 < argc)
				climate = argv[++i];
			else if(arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if(arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
			else
				pathToSimJson = argv[i];
		}

		string pathOfSimJson, simFileName;
		tie(pathOfSimJson, simFileName) = splitPathToFile(pathToSimJson);

		auto simj = readAndParseJsonFile(pathToSimJson);
		if(simj.failure())
			for(auto e : simj.errors)
				cerr << e << endl;
		auto simm = simj.result.object_items();

		if(!startDate.empty())
			simm["start-date"] = startDate;

		if(!endDate.empty())
			simm["end-date"] = endDate;

		if(debugSet)
			simm["debug?"] = debug;

		if(!pathToOutput.empty())
			simm["path-to-output"] = pathToOutput;

		//if(!pathToOutputFile.empty())
		//	simm["path-to-output-file"] = pathToOutputFile;

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

		if(!dailyOutputs.empty())
		{
			auto outm = simm["output"].object_items();
			string err;
			J11Array daily;

			string trimmedDailyOutputs = trim(dailyOutputs);
			if(trimmedDailyOutputs.front() == '[')
				trimmedDailyOutputs.erase(0, 1);
			if(trimmedDailyOutputs.back() == ']')
				trimmedDailyOutputs.pop_back();

			for(auto el : splitString(trimmedDailyOutputs, ",", make_pair("[", "]")))
			{
				if(trim(el).at(0) == '[')
				{
					J11Array a;
					auto es = splitString(trim(el, "[]"), ",");
					if(es.size() >= 1)
						a.push_back(es.at(0));
					if(es.size() >= 3)
						a.push_back(stoi(es.at(1))), a.push_back(stoi(es.at(2)));
					if(es.size() >= 4)
						a.push_back(es.at(3));
					daily.push_back(a);
				}
				else
					daily.push_back(el);
			}
			outm["daily"] = daily;
			simm["output"] = outm;
		}

		map<string, string> ps;
		ps["sim-json-str"] = json11::Json(simm).dump();

		ps["crop-json-str"] = printPossibleErrors(readFile(simm["crop.json"].string_value()), activateDebug);
		ps["site-json-str"] = printPossibleErrors(readFile(simm["site.json"].string_value()), activateDebug);
		//ps["path-to-climate-csv"] = simm["climate.csv"].string_value();

		auto env = createEnvFromJsonConfigFiles(ps);
		activateDebug = env.debugMode;

		if(activateDebug)
			cout << "starting MONICA with JSON input files" << endl;

		Output output = runMonica(env).out;

		if(pathToOutputFile.empty() && simm["output"]["write-file?"].bool_value())
			pathToOutputFile = fixSystemSeparator(simm["path-to-output"].string_value() + "/"
																						+ simm["output"]["file-name"].string_value());

		writeOutputFile = !pathToOutputFile.empty();

		ofstream fout;
		if(writeOutputFile)
		{
			string path, filename;
			tie(path, filename) = splitPathToFile(pathToOutputFile);
			ensureDirExists(path);
			fout.open(pathToOutputFile);
			if(fout.fail())
			{
				cerr << "Error while opening output file \"" << pathToOutputFile << "\"" << endl;
				writeOutputFile = false;
			}
		}

		ostream& out = writeOutputFile ? fout : cout;

		string csvSep = simm["output"]["csv-options"]["csv-separator"].string_value();
		bool includeHeaderRow = simm["output"]["csv-options"]["include-header-row"].bool_value();
		bool includeUnitsRow = simm["output"]["csv-options"]["include-units-row"].bool_value();

		if(!output.daily.empty())
		{
			writeOutputHeaderRows(out, env.dailyOutputIds, csvSep, includeHeaderRow, includeUnitsRow, false);
			writeOutput(out, env.dailyOutputIds, output.daily, csvSep, includeHeaderRow, includeUnitsRow);
		}

		if(!output.monthly.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, env.monthlyOutputIds, csvSep, includeHeaderRow, includeUnitsRow);
			for(auto& p : output.monthly)
				writeOutput(out, env.monthlyOutputIds, p.second, csvSep, includeHeaderRow, includeUnitsRow);
		}

		if(!output.yearly.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, env.yearlyOutputIds, csvSep, includeHeaderRow, includeUnitsRow);
			writeOutput(out, env.yearlyOutputIds, output.yearly, csvSep, includeHeaderRow, includeUnitsRow);
		}

		if(!output.at.empty())
		{
			for(auto& p : env.atOutputIds)
			{
				out << endl;
				auto ci = output.at.find(p.first);
				if(ci != output.at.end())
				{
					out << p.first.toIsoDateString() << endl;
					writeOutputHeaderRows(out, p.second, csvSep, includeHeaderRow, includeUnitsRow);
					writeOutput(out, p.second, ci->second, csvSep, includeHeaderRow, includeUnitsRow);
				}
			}
		}

		if(!output.crop.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, env.cropOutputIds, csvSep, includeHeaderRow, includeUnitsRow);
			for(auto& p : output.crop)
				writeOutput(out, env.cropOutputIds, p.second, csvSep, includeHeaderRow, includeUnitsRow);
		}

		if(!output.run.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, env.runOutputIds, csvSep, includeHeaderRow, includeUnitsRow);
			writeOutput(out, env.runOutputIds, {output.run}, csvSep, includeHeaderRow, includeUnitsRow);
		}

		if(writeOutputFile)
			fout.close();

		if(activateDebug)
			cout << "finished MONICA" << endl;
	}
	else 
		printHelp();

	
	return 0;
}
