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

#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include <string>

#include "zhelpers.hpp"
#include "json11/json11.hpp"

#include "tools/zmq-helper.h"
#include "tools/json11-helper.h"
#include "tools/helper.h"
#include "tools/debug.h"

using namespace std;
using namespace Tools;
using namespace json11;

string appName = "monica-zmq-control";
string version = "0.0.1";

int stopMonicaProcesses(zmq::context_t& context, 
												string proxyAddress,
												int frontendProxyPort,
												int count)
{
	int stopped = 0;

	// setup a reply socket for managing (e.g. starting/stopping) MONICA processes
	zmq::socket_t socket(context, ZMQ_REQ);

	string address = string("tcp://") + proxyAddress + ":" + to_string(frontendProxyPort);
	try
	{
		socket.setsockopt(ZMQ_RCVTIMEO, 5000);
		socket.setsockopt(ZMQ_SNDTIMEO, 5000);
		socket.connect(address);
		debug() << "Bound " << appName << " zeromq request socket to address: " << address << "!" << endl;

		//send finish message and wait for reply from MONICA server process
		//might take too long in practice, then we have to shut down a MONICA process in a better way
		debug() << "Trying to finish " << count << " MONICA processes" << endl;
		for(int i = 0; i < count; i++)
		{
			J11Object resultMsg;
			resultMsg["type"] = "finish";
			try
			{
				if(s_send(socket, Json(resultMsg).dump()))
				{
					debug() << "Send 'finish' message to a MONICA process" << endl;

					try
					{
						auto msg = receiveMsg(socket);
						if(msg.valid)
						{
							stopped++;
							debug() << "Received ack: " << msg.type() << endl;
						}
					}
					catch(zmq::error_t e)
					{
						cerr
							<< "Exception on trying to receive 'ack' reply message on zmq socket with address: "
							<< address << "! Error: [" << e.what() << "]" << endl;
					}
				}
			}
			catch(zmq::error_t e)
			{
				cerr 
					<< "Exception on trying to send 'finish' message on zmq socket with address: " << address
					<< "! Error: [" << e.what() << "]" << endl;
			}
		}
	}
	catch(zmq::error_t e)
	{
		cerr << "Couldn't connect socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
	}

	return stopped;
}

int main (int argc, 
					char** argv)
{
	int commPort = 6666;
	int frontendProxyPort = 5555;
	int backendProxyPort = 5556;
	string proxyAddress = "localhost";

	auto printHelp = [=]()
	{
		cout
			<< appName << " " << endl
			<< " [[-c | --port] COMM-PORT (default: " << commPort << ")] ... run " << appName << " with given control port" << endl
			<< " [[-a | --proxy-address] PROXY-ADDRESS (default: " << proxyAddress << ")] ... connect client to give IP address" << endl
			<< " [[-f | --frontend-proxy-port] PROXY-PORT (default: " << frontendProxyPort << ")] ... communicate with started MONICA ZeroMQ servers via given frontend proxy port" << endl
			<< " [[-b | --backend-proxy-port] PROXY-PORT (default: " << backendProxyPort << ")] ... connect started MONICA ZeroMQ servers to given backend proxy port" << endl
			<< " [-h | --debug] ... enable debug outputs" << endl
			<< " [-h | --help] ... this help output" << endl
			<< " [-v | --version] ... outputs " << appName << " version" << endl;
	};
	
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
		else if(arg == "-d" || arg == "--debug")
			activateDebug = true;
		else if(arg == "-h" || arg == "--help")
			printHelp(), exit(0);
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

		int started = 0;

		//loop until receive finish message
		while(true)
		{
			try
			{
				auto msg = receiveMsg(socket);
				if(!msg.valid)
					continue;

				debug() << "Received message: " << msg.toString() << endl;
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
					try
					{
						s_send(socket, Json(resultMsg).dump());
					}
					catch(zmq::error_t e)
					{
						cerr
							<< "Exception on trying to reply to 'finish' request with 'ack' message on zmq socket with address: "
							<< address << "! Still will finish " << appName << "! Error: [" << e.what() << "]" << endl;
					}
					break;
				}
				else if(msgType == "start-new")
				{
					Json& fmsg = msg.json;

					int count = fmsg["count"].int_value();

#ifdef WIN32
					string cmd = string("start /b monica --use-zmq-proxy --zmq-server --port ") + to_string(backendProxyPort);
#else
					string cmd = string("monica --use-zmq-proxy --zmq-server --port ") + to_string(backendProxyPort) + " &";
#endif
					
					int successfullyStarted = 0;
					for(int i = 0; i < count; i++)
					{
						int res = system(cmd.c_str());
						debug() << "result of running '" << cmd << "': " << res << endl;
						started++, successfullyStarted++;
					}

					J11Object resultMsg;
					resultMsg["type"] = "result";
					resultMsg["started"] = successfullyStarted;
					try
					{
						s_send(socket, Json(resultMsg).dump());
					}
					catch(zmq::error_t e)
					{
						cerr
							<< "Exception on trying to reply with result message: " << Json(resultMsg).dump() 
							<< " on zmq socket with address: " << address 
							<< "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
					}
				}
				else if(msgType == "start-max")
				{
					Json& fmsg = msg.json;

					int count = fmsg["count"].int_value();
					int stop = max(0, started - count);

#ifdef WIN32
					string cmd = string("start /b monica --use-zmq-proxy --zmq-server --port ") + to_string(backendProxyPort);
#else
					string cmd = string("monica --use-zmq-proxy --zmq-server --port ") + to_string(backendProxyPort) + " &";
#endif

					int additionallyStarted = 0;
					for(int i = started; i < count; i++)
					{
						int res = system(cmd.c_str());
						debug() << "result of running '" << cmd << "': " << res << endl;
						started++, additionallyStarted++;
					}

					stopMonicaProcesses(context, proxyAddress, backendProxyPort, stop);

					J11Object resultMsg;
					resultMsg["type"] = "result";
					resultMsg["started"] = additionallyStarted;
					resultMsg["shut-down"] = stop;
					try
					{
						s_send(socket, Json(resultMsg).dump());
					}
					catch(zmq::error_t e)
					{
						cerr
							<< "Exception on trying to reply with result message: " << Json(resultMsg).dump()
							<< " on zmq socket with address: " << address
							<< "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
					}
				}
				else if(msgType == "stop")
				{
					Json& fmsg = msg.json;

					int count = fmsg["count"].int_value();
					int stop = max(0, min(count, started));
					started -= stopMonicaProcesses(context, proxyAddress, frontendProxyPort, stop);

					J11Object resultMsg;
					resultMsg["type"] = "result";
					resultMsg["shut-down"] = stop;
					try
					{
						s_send(socket, Json(resultMsg).dump());
					}
					catch(zmq::error_t e)
					{
						cerr
							<< "Exception on trying to reply with result message: " << Json(resultMsg).dump()
							<< " on zmq socket with address: " << address
							<< "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
					}
				}
			}
			catch(zmq::error_t e)
			{
				cerr
					<< "Exception on trying to receive request message on zmq socket with address: "
					<< address << "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
			}
		}
	}
	catch(zmq::error_t e)
	{
		cerr << "Coulnd't bind socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
	}
	debug() << "Bound " << appName << " zeromq reply socket to address: " << address << "!" << endl;

	debug() << "exiting monica-zmq-control" << endl;
	return 0;
}
