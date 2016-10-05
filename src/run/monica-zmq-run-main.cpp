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

#include "zhelpers.hpp"

#include "json11/json11.hpp"

#include "tools/zmq-helper.h"
#include "tools/helper.h"

#include "db/abstract-db-connections.h"
#include "tools/debug.h"
#include "../run/run-monica.h"
#include "../run/run-monica-zmq.h"
#include "../io/database-io.h"
#include "../core/monica.h"
#include "tools/json11-helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "env-json-from-json-config.h"
#include "tools/algorithms.h"
#include "../io/csv-format.h"
#include "monica-zmq-defaults.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;

string appName = "monica-zmq-run";
string version = "2.0.0-beta";

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	bool debug = false, debugSet = false;
	string startDate, endDate;
	string pathToOutput;
	string pathToOutputFile;
	bool writeOutputFile = false;
	string address = defaultInputAddress;
	int port = defaultInputPort;
	string pathToSimJson = "./sim.json", crop, site, climate;
	string dailyOutputs;
	bool useLeapYears = true, useLeapYearsSet = false;

	auto printHelp = [=]()
	{
		cout
			<< appName << " [options] path-to-sim-json" << endl
			<< endl
			<< " -h   | --help ... this help output" << endl
			<< " -v   | --version ... outputs MONICA version" << endl
			<< endl
			<< " -d   | --debug ... show debug outputs" << endl
			<< " -a   | --address (PROXY-)ADDRESS (default: " << address << ") ... connect client to give IP address" << endl
			<< " -p   | --port (PROXY-)PORT (default: " << port << ") ... run server/connect client on/to given port" << endl
			<< " -sd  | --start-date ISO-DATE (default: start of given climate data) ... date in iso-date-format yyyy-mm-dd" << endl
			<< " -ed  | --end-date ISO-DATE (default: end of given climate data) ... date in iso-date-format yyyy-mm-dd" << endl
			<< " -nly | --no-leap-years ... skip 29th of february on leap years in climate data" << endl
			<< " -w   | --write-output-files ... write MONICA output files (rmout, smout)" << endl
			<< " -op  | --path-to-output DIRECTORY (default: .) ... path to output directory" << endl
			<< " -o   | --path-to-output-file FILE (default: ./rmout.csv) ... path to output file" << endl
			<< " -do  | --daily-outputs [LIST] (default: value of key 'sim.json:output.daily') ... list of daily output elements" << endl
			<< " -c   | --path-to-crop FILE (default: ./crop.json) ... path to crop.json file" << endl
			<< " -s   | --path-to-site FILE (default: ./site.json) ... path to site.json file" << endl
			<< " -w   | --path-to-climate FILE (default: ./climate.csv) ... path to climate.csv" << endl;
	};

	zmq::context_t context(1);

	if(argc > 1)
	{
		for(auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if(arg == "-d" || arg == "--debug")
				debug = debugSet = true;
			else if((arg == "-a" || arg == "--address")
							&& i + 1 < argc)
				address = argv[++i];
			else if((arg == "-p" || arg == "--port")
							&& i + 1 < argc)
				port = stoi(argv[++i]);
			else if((arg == "-sd" || arg == "--start-date")
			        && i+1 < argc)
				startDate = argv[++i];
			else if((arg == "-ed" || arg == "--end-date")
			        && i+1 < argc)
				endDate = argv[++i];
			else if(arg == "-nly" || arg == "--no-leap-years")
				useLeapYears = false, useLeapYearsSet = true;
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

		if(useLeapYearsSet)
			simm["use-leap-years"] = useLeapYears;

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

		auto env = createEnvJsonFromJsonConfigFiles(ps);
		activateDebug = env["debugMode"].bool_value();

		if(activateDebug)
			cout << "starting MONICA with JSON input files" << endl;

		Output output;
		Json res = sendZmqRequestMonicaFull(&context, string("tcp://") + address + ":" + to_string(port), env);
		addResultMessageToOutput(res.object_items(), output);

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
		bool includeAggRows = simm["output"]["csv-options"]["include-aggregation-rows"].bool_value();

		auto toOIdVector = [](const Json& a)
		{
			vector<OId> oids;
			for(auto j : a.array_items())
				oids.push_back(OId(j));
			return oids;
		};

		if(!output.daily.empty())
		{
			writeOutputHeaderRows(out, toOIdVector(env["dailyOutputIds"]), csvSep, includeHeaderRow, includeUnitsRow, includeAggRows);
			writeOutput(out, toOIdVector(env["dailyOutputIds"]), output.daily, csvSep);
		}

		if(!output.monthly.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, toOIdVector(env["monthlyOutputIds"]), csvSep, includeHeaderRow, includeUnitsRow, includeAggRows);
			for(auto& p : output.monthly)
				writeOutput(out, toOIdVector(env["monthlyOutputIds"]), p.second, csvSep);
		}

		if(!output.yearly.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, toOIdVector(env["yearlyOutputIds"]), csvSep, includeHeaderRow, includeUnitsRow, includeAggRows);
			writeOutput(out, toOIdVector(env["yearlyOutputIds"]), output.yearly, csvSep);
		}

		if(!output.at.empty())
		{
			for(auto& p : env["atOutputIds"].object_items())
			{
				out << endl;
				auto ci = output.at.find(p.first);
				if(ci != output.at.end())
				{
					out << p.first << endl;
					writeOutputHeaderRows(out, toOIdVector(p.second), csvSep, includeHeaderRow, includeUnitsRow, includeAggRows);
					writeOutput(out, toOIdVector(p.second), ci->second, csvSep);
				}
			}
		}

		auto makeWriteOutputCompatible = [](const J11Array& a)
		{
			vector<J11Array> vs;
			for(auto j : a)
				vs.push_back({j});
			return vs;
		};

		if(!output.crop.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, toOIdVector(env["cropOutputIds"]), csvSep, includeHeaderRow, includeUnitsRow, includeAggRows);
			for(auto& p : output.crop)
				writeOutput(out, toOIdVector(env["cropOutputIds"]), makeWriteOutputCompatible(p.second),
										csvSep);
		}

		if(!output.run.empty())
		{
			out << endl;
			writeOutputHeaderRows(out, toOIdVector(env["runOutputIds"]), csvSep, includeHeaderRow, includeUnitsRow, includeAggRows);
			writeOutput(out, toOIdVector(env["runOutputIds"]), makeWriteOutputCompatible(output.run), 
									csvSep);
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
