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
#include <vector>
#include <algorithm>

#include <kj/debug.h>

#include <kj/common.h>
#define KJ_MVCAP(var) var = kj::mv(var)

#include <capnp/ez-rpc.h>
#include <capnp/message.h>

#include "json11/json11.hpp"

#include "tools/helper.h"

#include "db/abstract-db-connections.h"
#include "tools/debug.h"
#include "run-monica.h"
#include "../io/database-io.h"
#include "../core/monica-model.h"
#include "tools/json11-helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "env-from-json-config.h"
#include "tools/algorithms.h"
#include "../io/csv-format.h"
#include "climate/climate-common.h"

#include "model.capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Climate;
using namespace zalf::capnp;

string appName = "monica-capnp-server";
string version = "1.0.0-beta";

std::map<std::string, DataAccessor> daCache;
bool startedServerInDebugMode = false;

DataAccessor fromCapnpData(capnp::List<rpc::Climate::Element>::Reader&& header, capnp::List<capnp::List<float>>::Reader&& data)
{
	DataAccessor da;
	
	typedef rpc::Climate::Element E;

	if (data.size() == 0)
		return da;

	vector<double> d(data.size());
	int i = 0;
	for (auto elem : header)
	{
		auto vs = data[i];
		transform(vs.begin(), vs.end(), d.begin(), [](float f) { return f; });
		switch (elem) {
		case E::TMIN: da.addClimateData(ACD::tmin, d); break;
		case E::TAVG: da.addClimateData(ACD::tavg, d); break;
		case E::TMAX: da.addClimateData(ACD::tmax, d); break;
		case E::PRECIP: da.addClimateData(ACD::precip, d); break;
		case E::RELHUMID: da.addClimateData(ACD::relhumid, d); break;
		case E::WIND: da.addClimateData(ACD::wind, d); break;
		case E::GLOBRAD: da.addClimateData(ACD::globrad, d); break;

		}
		i++;
	}

	return da;
}

class RunMonicaImpl final : public rpc::Model::Instance::Server
{
	// Implementation of the Model::Instance Cap'n Proto interface

public:
	kj::Promise<void> runEnv(RunEnvContext context) override
	{
		auto envR = context.getParams().getEnv(); 

		auto runMonica = [&context, envR](DataAccessor da = DataAccessor()) {
			string err;
			const Json& envJson = Json::parse(envR.getJsonEnv().cStr(), err);

			Env env(envJson);
			if (da.isValid())
				env.climateData = da;
			else if (!env.climateData.isValid() && !env.pathsToClimateCSV.empty())
				env.climateData = readClimateDataFromCSVFilesViaHeaders(env.pathsToClimateCSV, env.csvViaHeaderOptions);

			env.debugMode = startedServerInDebugMode && env.debugMode;

			env.params.userSoilMoistureParameters.getCapillaryRiseRate =
				[](string soilTexture, int distance) {
				return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
			};

			auto out = Monica::runMonica(env);

			context.getResults().setResult(out.toString());
		};

		if (envR.hasTimeSeries()) {
			auto ts = envR.getTimeSeries();
			auto headerProm = ts.headerRequest().send();
			auto dataTProm = ts.dataTRequest().send();
			
			auto finalProm = headerProm
				.then([KJ_MVCAP(dataTProm)](capnp::Response<rpc::Climate::TimeSeries::HeaderResults>&& headerRes) mutable {
				auto header = headerRes.getHeader();
				return dataTProm.then([KJ_MVCAP(header)](capnp::Response<rpc::Climate::TimeSeries::DataTResults>&& dataTRes) mutable {
					return fromCapnpData(kj::mv(header), dataTRes.getData());
				});
			}).then(runMonica);
		}
		else {
			runMonica();
		}

		return kj::READY_NOW;
	}
};


int main(int argc, const char* argv[]){

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	string address = "*";
	int port = -1;

	//init path to db-connections.ini
	if (auto monicaHome = getenv("MONICA_HOME"))
	{
		auto pathToFile = string(monicaHome) + Tools::pathSeparator() + "db-connections.ini";
		//init for dll/so
		initPathToDB(pathToFile);
		//init for monica-run
		Db::dbConnectionParameters(pathToFile);
	}

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	auto printHelp = [=]()
	{
		cout
			<< appName << "[options]" << endl
			<< endl
			<< "options:" << endl
			<< endl
			<< " -h | --help ... this help output" << endl
			<< " -v | --version ... outputs " << appName << " version and ZeroMQ version being used" << endl
			<< endl
			<< " -d | --debug ... show debug outputs" << endl
			<< " -a | --address ... ADDRESS (default: " << address << ")] "
			"... runs server bound to given address, may be '*' to bind to all local addresses" << endl
			<< " -p | --port ... PORT (default: none)] "
			"... runs the server bound to the port, PORT may be ommited to choose port automatically." << endl;
	};

	if (argc >= 1)
	{
		for (auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if (arg == "-d" || arg == "--debug") {
				activateDebug = true;
				startedServerInDebugMode = true;
			}
			else if (arg == "-a" || arg == "--address")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					address = argv[++i];
			}
			else if (arg == "-p" || arg == "--port")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					port = stoi(argv[++i]);
			}
			else if (arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if (arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		debug() << "starting Cap'n Proto MONICA server" << endl;

		// Set up a server.
		capnp::EzRpcServer server(kj::heap<RunMonicaImpl>(), address + (port < 0 ? "" : to_string(port)));

		// Write the port number to stdout, in case it was chosen automatically.
		auto& waitScope = server.getWaitScope();
		uint port = server.getPort().wait(waitScope);
		if (port == 0) {
			// The address format "unix:/path/to/socket" opens a unix domain socket,
			// in which case the port will be zero.
			std::cout << "Listening on Unix socket..." << std::endl;
		}
		else {
			std::cout << "Listening on port " << port << "..." << std::endl;
		}

		// Run forever, accepting connections and handling requests.
		kj::NEVER_DONE.wait(waitScope);

		debug() << "stopped Cap'n Proto MONICA server" << endl;
	}

	return 0;
}
