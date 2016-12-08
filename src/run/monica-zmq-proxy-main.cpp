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

#include <cstdlib>

#include "zhelpers.hpp"
#include "tools/debug.h"
#include "monica-zmq-defaults.h"

using namespace Tools;
using namespace std;
using namespace Monica;

string appName = "monica-zmq-proxy";
string version = "0.0.1";

int main (int argc, 
					char** argv)
{
	int frontendPort = defaultProxyFrontendPort;
	int backendPort = defaultProxyBackendPort;
	bool startControlNode = false;
	int controlPort = defaultControlPort;
	bool usePipelinePorts = false;
	
	auto printHelp = [=]()
	{
		cout
			<< appName << " [options] " << endl
			<< endl
			<< "options:" << endl
			<< endl
			<< " -h | --help ... this help output" << endl
			<< " -v | --version ... outputs " << appName << " version" << endl
			<< endl
			<< " -p | --pipeline-ports (default: use Router/Dealer sockets)" << endl
			<< " -f | --frontend-port FRONTEND-PORT (default: " << frontendPort << ") ... run " << appName << " with given frontend port" << endl
			<< " -b | --backend-port BACKEND-PORT (default: " << backendPort << ") ... run " << appName << " with given backend port" << endl
			<< " -c | --start-control-node [CONTROL-NODE-PORT] (default: " << controlPort << ") ... start control node, connected to proxy, on given port" << endl
			<< " -d | --debug ... enable debug outputs" << endl;
	};

	for(auto i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if((arg == "-f" || arg == "--frontend-port")
			 && i + 1 < argc)
			frontendPort = stoi(argv[++i]);
		else if((arg == "-b" || arg == "--backend-port")
						&& i + 1 < argc)
			backendPort = stoi(argv[++i]);
		else if(arg == "-c" || arg == "--start-control-node")
		{
			startControlNode = true;
			if(i + 1 < argc && argv[i + 1][0] != '-')
				controlPort = stoi(argv[++i]);
		}
		else if(arg == "-p" || arg == "--pipeline-ports")
			usePipelinePorts = true;
		else if(arg == "-d" || arg == "--debug")
			activateDebug = true;
		else if(arg == "-h" || arg == "--help")
			printHelp(), exit(0);
		else if(arg == "-v" || arg == "--version")
			cout << appName << " version " << version << endl, exit(0);
	}
	
	zmq::context_t context(1);

	// setup the proxy 
	// socket facing clients
	zmq::socket_t frontend (context, usePipelinePorts ? ZMQ_PULL : ZMQ_ROUTER);
	string feAddress = string("tcp://*:") + to_string(frontendPort);
	try
	{
		frontend.bind(feAddress);
	}
	catch(zmq::error_t e)
	{
		cerr << "Couldn't bind frontend socket to address: " << feAddress << "! Error: [" << e.what() << "]" << endl;
		exit(1);
	}
	debug() << "Bound " << appName << " zeromq router socket to frontend address: " << feAddress << "!" << endl;

	// socket facing services
	zmq::socket_t backend (context, usePipelinePorts ? ZMQ_PUSH : ZMQ_DEALER);
	string beAddress = string("tcp://*:") + to_string(backendPort);
	try
	{
		backend.bind(beAddress);
	}
	catch(zmq::error_t e)
	{
		cerr << "Couldn't bind backend socket to address: " << beAddress << "! Error: [" << e.what() << "]" << endl;
		exit(1);
	}
	debug() << "Bound " << appName << " zeromq dealer socket to backend address: " << beAddress << "!" << endl;

	if(startControlNode)
	{
		ostringstream oss;
#ifdef WIN32
		oss << "start /b monica-zmq-control -f " << frontendPort << " -b " << backendPort << " -c " << controlPort;
#else
		oss << "monica-zmq-control -f " << frontendPort << " -b " << backendPort << " -c " << controlPort << " &";
#endif

		int res = system(oss.str().c_str());
		debug() << "result of running '" << oss.str() << "': " << res << endl;
	}
	
	// start the proxy
	try
	{
		zmq::proxy((void*)frontend, (void*)backend, nullptr);
	}
	catch(zmq::error_t e)
	{
		cerr << "Couldn't start proxy! Error: [" << e.what() << "]" << endl;
		exit(1);
	}

	return 0;
}
