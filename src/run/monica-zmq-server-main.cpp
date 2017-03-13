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
#include "run-monica.h"
#include "serve-monica-zmq.h"
#include "../io/database-io.h"
#include "../core/monica-model.h"
#include "tools/json11-helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "env-from-json-config.h"
#include "tools/algorithms.h"
#include "../io/csv-format.h"
#include "monica-zmq-defaults.h"

using namespace std;
using namespace Monica;
using namespace Monica::ZmqServer;
using namespace Tools;
using namespace json11;

string appName = "monica-zmq-server";
string version = "2.0.0-beta";

/*
int main_(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	string address = defaultInputAddress; 
	int port = defaultInputPort;
	string outputAddress = defaultOutputAddress;
	int outputPort = defaultOutputPort;
	string controlAddress = defaultControlAddress;
	int controlPort = defaultControlPort;
	bool usePipeline = false;
	bool connectToZmqProxy = false;

	auto printHelp = [=]()
	{
		cout
			<< appName << endl
			<< " [-d | --debug] ... show debug outputs" << endl
			<< " [[-c | --connect-to-proxy]] ... connect MONICA server process to a ZeroMQ proxy" << endl
			<< " [[-a | --address] (PROXY-)ADDRESS (default: " << address << ")] ... connect client to give IP address" << endl
			<< " [[-p | --port] (PROXY-)PORT (default: " << port << ")] ... run server/connect client on/to given port" << endl
			<< " [[-od | --output-defaults] ... use different result socket (parameter is optional, when non default result address/port are used)" << endl
			<< " [[-oa | --output-address] ADDRESS (default: " << outputAddress << ")] ... bind socket to this IP address for results" << endl
			<< " [[-op | --output-port] PORT (default: " << outputPort << ")] ... bind socket to this port for results" << endl
			<< " [[-ca | --control-address] ADDRESS (default: " << controlAddress << ")] ... connect socket to this IP address for control messages" << endl
			<< " [[-cp | --control-port] PORT (default: " << controlPort << ")] ... bind socket to this port for control messages" << endl
			<< " [-h | --help] ... this help output" << endl
			<< " [-v | --version] ... outputs MONICA version" << endl;
	};

	zmq::context_t context(1);

	if(argc >= 1)
	{
		for(auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if(arg == "-d" || arg == "--debug")
				activateDebug = true;
			else if((arg == "-c" || arg == "--connect-to-proxy"))
				connectToZmqProxy = true;
			else if((arg == "-a" || arg == "--address")
							&& i + 1 < argc)
				address = argv[++i];
			else if((arg == "-p" || arg == "--port")
							&& i + 1 < argc)
				port = stoi(argv[++i]);
			if(arg == "-od" || arg == "--output-defaults")
				usePipeline = true;
			else if((arg == "-oa" || arg == "--output-address")
							&& i + 1 < argc)
				outputAddress = argv[++i], usePipeline = true;
			else if((arg == "-op" || arg == "--output-port")
							&& i + 1 < argc)
				outputPort = stoi(argv[++i]), usePipeline = true;
			else if((arg == "-ca" || arg == "--control-address")
							&& i + 1 < argc)
				controlAddress = argv[++i];
			else if((arg == "-cp" || arg == "--control-port")
							&& i + 1 < argc)
				controlPort = stoi(argv[++i]);
			else if(arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if(arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		debug() << "starting ZeroMQ MONICA server" << endl;
		
		string recvAddress = string("tcp://") + address + ":" + to_string(port);
		map<ZmqSocketRole, pair<ZmqSocketType, string>> addresses;
		if(usePipeline)
		{
			addresses[ReceiveJob] = make_pair(Pull, recvAddress);
			addresses[SendResult] = make_pair(Push, string("tcp://") + outputAddress + ":" + to_string(outputPort));
		} 
		else if(connectToZmqProxy)
			addresses[ReceiveJob] = make_pair(ProxyReply, recvAddress);
		else
			addresses[ReceiveJob] = make_pair(Reply, recvAddress);

		addresses[Control] = make_pair(Subscribe, string("tcp://") + controlAddress + ":" + to_string(controlPort));

		serveZmqMonicaFull(&context, addresses);

		debug() << "stopped ZeroMQ MONICA server" << endl;
	}

	return 0;
}
*/

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//init path to db-connections.ini
	if(auto monicaHome = getenv("MONICA_HOME"))
	{
		auto pathToFile = string(monicaHome) + pathSeparator() + "db-connections.ini";
		//init for dll/so
		initPathToDB(pathToFile);
		//init for monica-run
		Db::dbConnectionParameters(pathToFile);
	}

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	string serveAddress = defServeAddress;
	string proxyAddress = defProxyBackendAddress;
	bool connectToZmqProxy = false;
	string inputAddress = defInputAddress;
	string outputAddress = defOutputAddress;
	bool usePipeline = false;
	string controlAddress = defControlAddress;

	SocketOp inputOp = ZmqServer::connect;
	SocketOp outputOp = ZmqServer::connect;

	int major, minor, patch;
	zmq::version(&major, &minor, &patch);

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
			<< " -s | --serve-address [ADDRESS] (default: " << serveAddress << ")] ... serve MONICA on given address" << endl
			<< " -p | --proxy-address [(PROXY-)ADDRESS1[,ADDRESS2,...]] (default: " << inputAddress << ")] ... receive work via proxy from given address(es)" << endl
			<< " -bi | --bind-input ... bind the input port" << endl
			<< " -ci | --connect-input (default) ... connect the input port" << endl
			<< " -i | --input-address [bind|connect]|[ADDRESS1[,ADDRESS2,...]] (default: " << inputAddress << ")] ... receive work from given address(es)" << endl
			<< " -bo | --bind-output ... bind the output port" << endl
			<< " -co | --connect-output (default) ... connect the output port" << endl
			<< " -o | --output-address [ADDRESS1[,ADDRESS2,...]] (default: " << outputAddress << ")] ... send results to this address(es)" << endl
			<< " -c | --control-address [ADDRESS] (default: " << controlAddress << ")] ... connect MONICA server to this address for control messages" << endl;
	};

	zmq::context_t context(1);

	if(argc >= 1)
	{
		for(auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if(arg == "-d" || arg == "--debug")
				activateDebug = true;
			else if(arg == "-s" || arg == "--serve-address")
			{
				if(i + 1 < argc && argv[i + 1][0] != '-')
					serveAddress = argv[++i];
			}
			else if(arg == "-p" || arg == "--proxy-address")
			{
				connectToZmqProxy = true;
				if(i + 1 < argc && argv[i + 1][0] != '-')
					proxyAddress = argv[++i];
			}
			if(arg == "-bi" || arg == "--bind-input")
				inputOp = ZmqServer::bind;
			if(arg == "-ci" || arg == "--connect-input")
				inputOp = ZmqServer::connect;
			else if(arg == "-i" || arg == "--input-address")
			{
				if(i + 1 < argc && argv[i + 1][0] != '-')
					inputAddress = argv[++i];
			}
			if(arg == "-bo" || arg == "--bind-output")
				outputOp = ZmqServer::bind;
			if(arg == "-co" || arg == "--connect-output")
				outputOp = ZmqServer::connect;
			else if(arg == "-o" || arg == "--output-address")
			{
				usePipeline = true;
				if(i + 1 < argc && argv[i + 1][0] != '-')
					outputAddress = argv[++i];
			}
			else if(arg == "-c" || arg == "--control-address")
			{
				if(i + 1 < argc && argv[i + 1][0] != '-')
					controlAddress = argv[++i];
			}
			else if(arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if(arg == "-v" || arg == "--version")
				cout << appName << " version " << version << " ZeroMQ version: " << major << "." << minor << "." << patch << endl, exit(0);
		}

		debug() << "starting ZeroMQ MONICA server" << endl;

		map<SocketRole, SocketConfig> addresses;
		if(usePipeline)
		{
			addresses[ReceiveJob] = {Pull, splitString(inputAddress, ","), inputOp};
			addresses[SendResult] = {Push, splitString(outputAddress, ","), outputOp};
		}
		else if(connectToZmqProxy)
			addresses[ReceiveJob] = {ProxyReply, splitString(proxyAddress, ","), ZmqServer::connect};
		else
			addresses[ReceiveJob] = {Reply, splitString(serveAddress, ","), ZmqServer::bind};

		addresses[Control] = {Subscribe, vector<string>{controlAddress}, ZmqServer::connect};

		serveZmqMonicaFull(&context, addresses);

		debug() << "stopped ZeroMQ MONICA server" << endl;
	}

	return 0;
}
