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
#include <tuple>
#include <vector>
#include <algorithm>

#include <kj/debug.h>

#include <kj/common.h>
#define KJ_MVCAP(var) var = kj::mv(var)

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/rpc-twoparty.h>
#include <kj/thread.h>

#include "tools/helper.h"
#include "db/abstract-db-connections.h"
#include "tools/debug.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Climate;
using namespace zalf::capnp;

string appName = "monica-capnp-server";
string version = "1.0.0-beta";

int main(int argc, const char* argv[]) {

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	string proxyAddress = "localhost";
	int proxyPort = 6666;
	bool connectToProxy = false;

	string address = "*";
	int port = -1;

	bool hideServer = false;
	bool startedServerInDebugMode = false;

	//init path to db-connections.ini
	if (auto monicaHome = getenv("MONICA_HOME"))
	{
		auto pathToFile = string(monicaHome) + Tools::pathSeparator() + "db-connections.ini";
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
			<< " -h | --help "
			"... this help output" << endl
			<< " -v | --version "
			"... outputs " << appName << " version and ZeroMQ version being used" << endl
			<< endl
			<< " -d | --debug "
			"... show debug outputs" << endl
			<< " -i | --hide "
			"... hide server (default: " << (hideServer ? "true" : "false") << " as service on give address and port" << endl
			<< " -a | --address ... ADDRESS (default: " << address << ")] "
			"... runs server bound to given address, may be '*' to bind to all local addresses" << endl
			<< " -p | --port ... PORT (default: none)] "
			"... runs the server bound to the port, PORT may be ommited to choose port automatically." << endl
			<< " -cp | --connect-to-proxy "
			"... connect to proxy at -pa and -pp" << endl
			<< " -pa | --proxy-address "
			"... ADDRESS (default: " << proxyAddress << ")] "
			"... connects server to proxy running at given address" << endl
			<< " -pp | --proxy-port ... PORT (default: " << proxyPort << ")] "
			"... connects server to proxy running on given port." << endl;
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
			else if (arg == "-i" || arg == "--hide") 
			{
				hideServer = true;
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
			else if (arg == "-cp" || arg == "--connect-to-proxy")
			{
				connectToProxy = true;
			}
			else if (arg == "-pa" || arg == "--proxy-address")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					proxyAddress = argv[++i];
			}
			else if (arg == "-pp" || arg == "--proxy-port")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					proxyPort = stoi(argv[++i]);
			}
			else if (arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if (arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		debug() << "starting Cap'n Proto MONICA server" << endl;

		//create monica server implementation
		rpc::Model::EnvInstance::Client runMonicaImplClient = kj::heap<RunMonicaImpl>(startedServerInDebugMode);

		if (connectToProxy)
		{
			//create client connection to proxy
			try
			{
				capnp::EzRpcClient client(proxyAddress, proxyPort);

				auto& cWaitScope = client.getWaitScope();

				// Request the bootstrap capability from the server.
				rpc::Model::EnvInstanceProxy::Client cap = client.getMain<rpc::Model::EnvInstanceProxy>();

				// Make a call to the capability.
				auto request = cap.registerService2Request();
				request.setService(runMonicaImplClient);
				request.send().wait(cWaitScope);

				if (hideServer)
					kj::NEVER_DONE.wait(cWaitScope);
			}
			catch (exception e)
			{
				cerr << "Couldn't connect to proxy at address: " << proxyAddress << ":" << proxyPort << endl 
					<< "Exception: " << e.what() << endl;
			}
		}

		if(!hideServer)
		{
			capnp::EzRpcServer server(runMonicaImplClient, address + (port < 0 ? "" : string(":") + to_string(port)));

			// Write the port number to stdout, in case it was chosen automatically.
			auto& waitScope = server.getWaitScope();
			port = server.getPort().wait(waitScope);
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
		}

		debug() << "stopped Cap'n Proto MONICA server" << endl;
	}

	return 0;
}
