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

#include "zhelpers.hpp"
#include "json11/json11.hpp"

#include "tools/zmq-helper.h"
#include "tools/json11-helper.h"
#include "tools/helper.h"

using namespace std;
using namespace Tools;
using namespace json11;

string appName = "monica-zmq-control";
string version = "0.0.1";

void stopMonicaProcesses(zmq::context_t& context, 
												 string proxyAddress, 
												 int backendProxyPort, 
												 int count)
{
	// setup a reply socket for managing (e.g. starting/stopping) MONICA processes
	zmq::socket_t socket(context, ZMQ_REP);

	string address = string("tcp://") + proxyAddress + ":" + to_string(backendProxyPort);
	try
	{
		socket.connect(address);
	}
	catch(zmq::error_t e)
	{
		cerr << "Coulnd't connect socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
	}
	//cout << "Bound " << appName << " zeromq reply socket to address: " << address << "!" << endl;

	//send finish message and wait for reply from MONICA server process
	//might take too long in practice, then we have to shut down a MONICA process in a better way
	for(int i = 0; i < count; i++)
	{
		J11Object resultMsg;
		resultMsg["type"] = "finish";
		s_send(socket, Json(resultMsg).dump());

		auto msg = receiveMsg(socket);
		cout << "Received ack: " << msg.type() << endl;
	}
}

int main (int argc, 
					char** argv)
{
	int commPort = 6666;
	int frontendProxyPort = 5555;
	int backendProxyPort = 5556;
	string proxyAddress = "localhost";

	for(auto i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if((arg == "-c" || arg == "--comm-port")
			 && i + 1 < argc)
			commPort = stoi(argv[++i]);
		else if((arg == "-a" || arg == "--proxy-address")
						&& i + 1 < argc)
			proxyAddress = argv[++i];
		else if((arg == "-f" || arg == "--frontend-proxy-port")
						&& i + 1 < argc)
			frontendProxyPort = stoi(argv[++i]);
		else if((arg == "-b" || arg == "--backend-proxy-port")
						&& i + 1 < argc)
			backendProxyPort = stoi(argv[++i]);
		else if(arg == "-h" || arg == "--help")
		{
			cout
				<< "./" << appName << " " << endl
				<< "\t [[-c | --port] COMM-PORT (default: " << commPort << ")]\t ... run " << appName << " with given control port" << endl
				<< "\t [[-a | --proxy-address] PROXY-ADDRESS (default: " << proxyAddress << ")]\t ... connect client to give IP address" << endl
				<< "\t [[-f | --frontend-proxy-port] PROXY-PORT (default: " << frontendProxyPort << ")]\t ... communicate with started MONICA ZeroMQ servers via given frontend proxy port" << endl
				<< "\t [[-b | --backend-proxy-port] PROXY-PORT (default: " << backendProxyPort << ")]\t ... connect started MONICA ZeroMQ servers to given backend proxy port" << endl
				<< "\t [-h | --help]\t\t\t ... this help output" << endl
				<< "\t [-v | --version]\t\t ... outputs " << appName << " version" << endl;
			exit(0);
		}
		else if(arg == "-v" || arg == "--version")
			cout << appName << " version " << version << endl, exit(0);
	}
	
	zmq::context_t context(1);

	// setup a reply socket for managing (e.g. starting/stopping) MONICA processes
	zmq::socket_t socket(context, ZMQ_REP);

	string address = string("tcp://*:") + to_string(commPort);
	try
	{
		socket.bind(address);
	}
	catch(zmq::error_t e)
	{
		cerr << "Coulnd't bind socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
	}
	cout << "Bound " << appName << " zeromq reply socket to address: " << address << "!" << endl;

	int started = 0;

	//loop until receive finish message
	while(true)
	{
		auto msg = receiveMsg(socket);
		cout << "Received message: " << msg.toString() << endl;
		//    if(!msg.valid)
		//    {
		//      this_thread::sleep_for(chrono::milliseconds(100));
		//      continue;
		//    }

		string msgType = msg.type();
		if(msgType == "finish")
		{
			J11Object resultMsg;
			resultMsg["type"] = "ack";
			s_send(socket, Json(resultMsg).dump());
			break;
		}
		else if(msgType == "start-new")
		{
			Json& fmsg = msg.json;

			int count = fmsg["count"].int_value();
			
			string cmd = string("monica --use-zmq-proxy --zmq-server --port ") + to_string(backendProxyPort);
			string fullCmd = fixSystemSeparator(cmd);

			int successfullyStarted = 0;
			for(int i = 0; i < count; i++)
			{
				int res = system(fullCmd.c_str());
				cout << "res: " << res << endl;
				if(res)
					started++, successfullyStarted++;
			}
					
			J11Object resultMsg;
			resultMsg["type"] = "result";
			resultMsg["started"] = successfullyStarted;
			s_send(socket, Json(resultMsg).dump());
		}
		else if(msgType == "start-max")
		{
			Json& fmsg = msg.json;

			int count = fmsg["count"].int_value();
			int stop = max(0, started - count);

			string cmd = string("monica --use-zmq-proxy --zmq-server --port ") + to_string(backendProxyPort);
			string fullCmd = fixSystemSeparator(cmd);

			int additionallyStarted = 0;
			for(int i = started; i < count; i++)
			{
				int res = system(fullCmd.c_str());
				cout << "res: " << res << endl;
				if(res)
					started++, additionallyStarted++;
			}

			if(stop > 0)
				stopMonicaProcesses(context, proxyAddress, backendProxyPort, stop);

			J11Object resultMsg;
			resultMsg["type"] = "result";
			resultMsg["started"] = additionallyStarted;
			resultMsg["shut-down"] = stop;
			s_send(socket, Json(resultMsg).dump());
		}
		else if(msgType == "stop")
		{
			Json& fmsg = msg.json;

			int count = fmsg["count"].int_value();
			int stop = started - count;
			if(stop > 0)
				stopMonicaProcesses(context, proxyAddress, backendProxyPort, stop);

			J11Object resultMsg;
			resultMsg["type"] = "result";
			resultMsg["shut-down"] = stop;
			s_send(socket, Json(resultMsg).dump());
		}
	}

	cout << "exiting monica-zmq-control" << endl;
	return 0;
}
