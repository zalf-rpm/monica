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

#include "zhelpers.hpp"

using namespace std;

string appName = "monica-zmq-proxy";
string version = "0.0.1";

int main (int argc, 
					char** argv)
{
	int frontendPort = 5555;
	int backendPort = 5556;
	
	for(auto i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if((arg == "-f" || arg == "--frontend-port")
			 && i + 1 < argc)
			frontendPort = stoi(argv[++i]);
		else if((arg == "-b" || arg == "--backend-port")
						&& i + 1 < argc)
			backendPort = stoi(argv[++i]);
		else if(arg == "-h" || arg == "--help")
		{
			cout
				<< "./" << appName << " " << endl
				<< "\t [[-f | --frontend-port] FRONTEND-PORT (default: " << frontendPort << ")]\t ... run " << appName << " with given frontend port" << endl
				<< "\t [[-b | --backend-port] BACKEND-PORT (default: " << backendPort << ")]\t ... run " << appName << " with given backend port" << endl
				<< "\t [-h | --help]\t\t\t ... this help output" << endl
				<< "\t [-v | --version]\t\t ... outputs " << appName << " version" << endl;
			exit(0);
		}
		else if(arg == "-v" || arg == "--version")
			cout << appName << " version " << version << endl, exit(0);
	}
	
	zmq::context_t context(1);
	
	// setup the proxy 
	// socket facing clients
	zmq::socket_t frontend (context, ZMQ_ROUTER);
	string feAddress = string("tcp://*:") + to_string(frontendPort);
	try
	{
		frontend.bind(feAddress);
	}
	catch(zmq::error_t e)
	{
		cerr << "Couldn't bind frontend socket to address: " << feAddress << "! Error: [" << e.what() << "]" << endl;
	}
	cout << "Bound " << appName << " zeromq router socket to frontend address: " << feAddress << "!" << endl;

	// socket facing services
	zmq::socket_t backend (context, ZMQ_DEALER);
	string beAddress = string("tcp://*:") + to_string(backendPort);
	try
	{
		backend.bind(beAddress);
	}
	catch(zmq::error_t e)
	{
		cerr << "Couldn't bind backend socket to address: " << beAddress << "! Error: [" << e.what() << "]" << endl;
	}
	cout << "Bound " << appName << " zeromq dealer socket to backend address: " << beAddress << "!" << endl;

	// start the proxy
	try
	{
		zmq::proxy(&frontend, &backend, nullptr);
	}
	catch(zmq::error_t e)
	{
		cerr << "Couldn't start proxy! Error: [" << e.what() << "]" << endl;
	}

	return 0;
}
