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
#include "../core/monica-typedefs.h"
#include "../core/monica.h"
#include "tools/json11-helper.h"
#include "climate/climate-file-io.h"
#include "../core/simulation.h"
#include "soil/conversion.h"
#include "env-from-json.h"
#include "tools/algorithms.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;

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

void sendControlMessage(zmq::context_t& context,
												string proxyAddress,
												int frontendProxyPort,
												string messageType,
												int count)
{
	zmq::socket_t socket(context, ZMQ_REQ);

	string address = string("tcp://") + proxyAddress + ":" + to_string(frontendProxyPort);
	try
	{
		socket.connect(address);
	}
	catch(zmq::error_t e)
	{
		cerr << "Coulnd't connect socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
	}
	//cout << "Bound " << appName << " zeromq reply socket to address: " << address << "!" << endl;

	J11Object resultMsg;
	resultMsg["type"] = messageType;
	resultMsg["count"] = count;
	s_send(socket, Json(resultMsg).dump());

	auto msg = receiveMsg(socket);
	cout << "Received ack: " << msg.type() << endl;
}

string appName = "monica";
string version = "2.0.0-beta";

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	enum Mode { monica, /*hermes,*/ zmqClient, zmqServer };

#ifndef NO_ZMQ
	zmq::context_t context(1);
#endif

	if(argc >= 1)// && false)
	{
		bool debug = false, debugSet = false;
		string startDate, endDate;
		string pathToOutput;
		string pathToOutputFile;
		bool writeOutputFile = false;
		Mode mode = monica;
		int port = 5560;
		string address = "localhost";
		string pathToSimJson = "./sim.json", crop, site, climate;
		bool useZmqProxy = false;
		string controlAddress = "localhost";
		int controlPort = 6666;
		string command = "";
		int count = 1;
		string dailyOutputs;

		for(auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if(arg == "-d" || arg == "--debug")
				debug = debugSet = true;
			else if(arg == "--use-zmq-proxy")
				useZmqProxy = true;
			//else if(arg == "--hermes")
			//	mode = hermes;
			else if(arg == "--zmq-client")
				mode = zmqClient;
			else if(arg == "--zmq-server")
				mode = zmqServer;
			else if((arg == "-ca" || arg == "--control-address")
							&& i + 1 < argc)
				controlAddress = argv[++i];
			else if((arg == "-cp" || arg == "--control-port")
							&& i + 1 < argc)
				controlPort = stoi(argv[++i]);
			else if(arg == "--send"
							&& i + 1 < argc)
				command = argv[++i];
			else if(arg == "--count"
							&& i + 1 < argc)
				count = atoi(argv[++i]);
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
			{
				cout 
					<< "./" << appName << endl
					<< "\t [-d | --debug] ... show debug outputs" << endl
					<< "\t [--use-zmq-proxy] ... connect MONICA process to a ZeroMQ proxy" << endl
					//<< "\t [--hermes] ... use old hermes format files" << endl
					<< "\t [--zmq-client] ... run in client mode communicating to a MONICA ZeroMQ server" << endl
					<< "\t [--zmq-server] ... run in server mode communicating with MONICA ZeroMQ clients" << endl
					<< "\t [[-ca | --control-address] CONTROL-ADDRESS (default: " << controlAddress << ")] ... address of control node" << endl
					<< "\t [[-cp | --control-port] CONTROL-PORT (default: " << controlPort << ")] ... port of control node" << endl
					<< "\t [--send] COMMAND (start-new | start-max | stop)] ... send message to zmq control node" << endl
					<< "\t [--count] COUNT (default: " << count << ")] ... tell in control message how many MONICA processes to start/stop" << endl
					<< "\t [[-a | --address] (PROXY-)ADDRESS (default: " << address << ")] ... connect client to give IP address" << endl
					<< "\t [[-p | --port] (PROXY-)PORT (default: " << port << ")] ... run server/connect client on/to given port" << endl
					<< "\t [[-sd | --start-date] ISO-DATE (default: start of given climate data)] ... date in iso-date-format yyyy-mm-dd" << endl
					<< "\t [[-ed | --end-date] ISO-DATE (default: end of given climate data)] ... date in iso-date-format yyyy-mm-dd" << endl
					<< "\t [-w | --write-output-files] ... write MONICA output files (rmout, smout)" << endl
					<< "\t [[-op | --path-to-output] DIRECTORY (default: .)] ... path to output directory" << endl
					<< "\t [[-o | --path-to-output-file] FILE (default: ./rmout.csv)] ... path to output file" << endl
					<< "\t [[-do | --daily-outputs] [LIST] (default: value of key 'sim.json:output.daily')] ... list of daily output elements" << endl
					<< "\t [[-c | --path-to-crop] FILE (default: ./crop.json)] ... path to crop.json file" << endl
					<< "\t [[-s | --path-to-site] FILE (default: ./site.json)] ... path to site.json file" << endl
					<< "\t [[-w | --path-to-climate] FILE (default: ./climate.csv)] ... path to climate.csv" << endl
					<< "\t [-h | --help] ... this help output" << endl
					<< "\t [-v | --version] ... outputs MONICA version" << endl
					<< "\t path-to-sim-json ... path to sim.json file" << endl;
			  exit(0);
			}
			else if(arg == "-v" || arg == "--version")
				cout << "MONICA version " << version << endl, exit(0);
			else
				pathToSimJson = argv[i];
		}

		if(!command.empty())
		{
			if(command == "start-new"
				 || command == "start-max"
				 || command == "stop")
				sendControlMessage(context, controlAddress, controlPort, command, count);
			else if(debug)
				cout << "Control command: " << command << " unknown, should be one of [start-new, start-max or stop]!" << endl;
		}
		//else if(mode == hermes)
		//{
		//	if(debug)
		//		cout << "starting MONICA with old HERMES input files" << endl;
		//Monica::runWithHermesData(fixSystemSeparator(pathToSimJson), debug);
		//
		//	if(debug)
		//		cout << "finished MONICA" << endl;
		//}
		else if(mode == zmqServer)
		{
			if(debug)
				cout << "starting ZeroMQ MONICA server" << endl;

			startZeroMQMonicaFull(&context, string("tcp://") + (useZmqProxy ? address : "*") + ":" + to_string(port), useZmqProxy);

			if(debug)
				cout << "stopped ZeroMQ MONICA server" << endl;
		}
		else
		{
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

			map<int, J11Array> dailyOut;

			if(mode == monica)
			{
				dailyOut = runMonica(env).out.daily;
			}
			else
			{
				Json res = runZeroMQMonicaFull(&context, string("tcp://") + address + ":" + to_string(port), env);
				for(auto& p : res["daily"].object_items())
					dailyOut[stoi(p.first)] = p.second.array_items();
			}
			
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

			ostringstream oss1, oss2;
			for(auto oid : env.outputIds)
			{
				string name, unit;
				tie(name, unit) = env.outputId2nameAndUnit.at(oid.id);

				for(int i = oid.from; i < oid.to; i++)
				{
					oss1 << "\"" << name << (oid.isRange() ? string("_") + to_string(i) : "") << "\"" << csvSep;
					oss2 << "[" << unit << "]" << csvSep;
				}
			}

			if(includeHeaderRow)
				out << oss1.str() << endl;
			if(includeUnitsRow)
				out << oss2.str() << endl;

			if(!dailyOut.empty())
			{
				for(size_t i = 0, size = dailyOut.begin()->second.size(); i < size; i++)
				{
					for(auto oid : env.outputIds)
					{
						Json j = dailyOut[oid.id].at(i);
						switch(j.type())
						{
						case Json::NUMBER: out << j.number_value() << csvSep; break;
						case Json::STRING: out << j.string_value() << csvSep; break;
						case Json::BOOL: out << j.bool_value() << csvSep; break;
						case Json::ARRAY:
						{
							for(Json jv : j.array_items())
							{
								switch(jv.type())
								{
								case Json::NUMBER: out << jv.number_value() << csvSep; break;
								case Json::STRING: out << jv.string_value() << csvSep; break;
								case Json::BOOL: out << jv.bool_value() << csvSep; break;
								default: out << "UNKNOWN" << csvSep;
								}
							}
							break;
						default: out << "UNKNOWN" << csvSep;
						}
						}
					}
					out << endl;
				}
			}
			out.flush();

			if(writeOutputFile)
				fout.close();

			if(activateDebug)
				cout << "finished MONICA" << endl;
		}
	}
	
	//test();
	
	//writeDbParams();

	return 0;
}

/*
int main_(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	enum Mode { monica, hermes, zmqClient, zmqServer };

#ifndef NO_ZMQ
	zmq::context_t context(1);
#endif

	if(argc >= 1)// && false)
	{
		bool debug = false, debugSet = false;
		string startDate, endDate;
		bool writeOutputFiles = false, writeOutputFilesSet = false;
		string pathToOutput;
		Mode mode = monica;
		int port = 5560;
		string address = "localhost";
		string pathToSimJson = "./sim.json", crop, site, climate;
		bool useZmqProxy = false;
		string controlAddress = "localhost";
		int controlPort = 6666;
		string command = "";
		int count = 1;

		for(auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if(arg == "-d" || arg == "--debug")
				debug = debugSet = true;
			else if(arg == "--use-zmq-proxy")
				useZmqProxy = true;
			else if(arg == "--hermes")
				mode = hermes;
			else if(arg == "--zmq-client")
				mode = zmqClient;
			else if(arg == "--zmq-server")
				mode = zmqServer;
			else if((arg == "-ca" || arg == "--control-address")
							&& i + 1 < argc)
				controlAddress = argv[++i];
			else if((arg == "-cp" || arg == "--control-port")
							&& i + 1 < argc)
				controlPort = stoi(argv[++i]);
			else if(arg == "--send"
							&& i + 1 < argc)
				command = argv[++i];
			else if(arg == "--count"
							&& i + 1 < argc)
				count = atoi(argv[++i]);
			else if((arg == "-a" || arg == "--address")
							&& i + 1 < argc)
				address = argv[++i];
			else if((arg == "-p" || arg == "--port")
							&& i + 1 < argc)
				port = stoi(argv[++i]);
			else if((arg == "-sd" || arg == "--start-date")
							&& i + 1 < argc)
				startDate = argv[++i];
			else if((arg == "-ed" || arg == "--end-date")
							&& i + 1 < argc)
				endDate = argv[++i];
			else if(arg == "-w" || arg == "--write-output-files")
				writeOutputFiles = writeOutputFilesSet = true;
			else if((arg == "-o" || arg == "--path-to-output")
							&& i + 1 < argc)
				pathToOutput = argv[++i];
			else if((arg == "-c" || arg == "--path-to-crop")
							&& i + 1 < argc)
				crop = argv[++i];
			else if((arg == "-s" || arg == "--path-to-site")
							&& i + 1 < argc)
				site = argv[++i];
			else if((arg == "-w" || arg == "--path-to-climate")
							&& i + 1 < argc)
				climate = argv[++i];
			else if(arg == "-h" || arg == "--help")
			{
				cout
					<< "./" << appName << endl
					<< "\t [-d | --debug] ... show debug outputs" << endl
					<< "\t [--use-zmq-proxy] ... connect MONICA process to a ZeroMQ proxy" << endl
					<< "\t [--hermes] ... use old hermes format files" << endl
					<< "\t [--zmq-client] ... run in client mode communicating to a MONICA ZeroMQ server" << endl
					<< "\t [--zmq-server] ... run in server mode communicating with MONICA ZeroMQ clients" << endl
					<< "\t [[-ca | --control-address] CONTROL-ADDRESS (default: " << controlAddress << ")] ... address of control node" << endl
					<< "\t [[-cp | --control-port] CONTROL-PORT (default: " << controlPort << ")] ... port of control node" << endl
					<< "\t [--send] COMMAND (start-new | start-max | stop)] ... send message to zmq control node" << endl
					<< "\t [--count] COUNT (default: " << count << ")] ... tell in control message how many MONICA processes to start/stop" << endl
					<< "\t [[-a | --address] (PROXY-)ADDRESS (default: " << address << ")] ... connect client to give IP address" << endl
					<< "\t [[-p | --port] (PROXY-)PORT (default: " << port << ")] ... run server/connect client on/to given port" << endl
					<< "\t [[-sd | --start-date] ISO-DATE (default: start of given climate data)] ... date in iso-date-format yyyy-mm-dd" << endl
					<< "\t [[-ed | --end-date] ISO-DATE (default: end of given climate data)] ... date in iso-date-format yyyy-mm-dd" << endl
					<< "\t [-w | --write-output-files] ... write MONICA output files (rmout, smout)" << endl
					<< "\t [[-o | --path-to-output] DIRECTORY (default: .)] ... path to output directory" << endl
					<< "\t [[-c | --path-to-crop] FILE (default: ./crop.json)] ... path to crop.json file" << endl
					<< "\t [[-s | --path-to-site] FILE (default: ./site.json)] ... path to site.json file" << endl
					<< "\t [[-w | --path-to-climate] FILE (default: ./climate.csv)] ... path to climate.csv" << endl
					<< "\t [-h | --help] ... this help output" << endl
					<< "\t [-v | --version] ... outputs MONICA version" << endl
					<< "\t path-to-sim-json ... path to sim.json file" << endl;
				exit(0);
			}
			else if(arg == "-v" || arg == "--version")
				cout << "MONICA version " << version << endl, exit(0);
			else
				pathToSimJson = argv[i];
		}

		if(!command.empty())
		{
			if(command == "start-new"
				 || command == "start-max"
				 || command == "stop")
				sendControlMessage(context, controlAddress, controlPort, command, count);
			else if(debug)
				cout << "Control command: " << command << " unknown, should be one of [start-new, start-max or stop]!" << endl;
		}
		else if(mode == hermes)
		{
			if(debug)
				cout << "starting MONICA with old HERMES input files" << endl;

			Monica::runWithHermesData(fixSystemSeparator(pathToSimJson), debug);

			if(debug)
				cout << "finished MONICA" << endl;
		}
		else if(mode == zmqServer)
		{
			if(debug)
				cout << "starting ZeroMQ MONICA server" << endl;

			startZeroMQMonicaFull(&context, string("tcp://") + (useZmqProxy ? address : "*") + ":" + to_string(port), useZmqProxy);

			if(debug)
				cout << "stopped ZeroMQ MONICA server" << endl;
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

			if(mode == monica)
			{
				auto res = runMonica(env);
			}
			else
			{
				runZeroMQMonicaFull(&context, string("tcp://") + address + ":" + to_string(port), env);
			}

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
//*/