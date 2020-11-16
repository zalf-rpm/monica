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

#include "zeromq/zmq-helper.h"
#include "json11/json11.hpp"
#include "json11/json11-helper.h"
#include "tools/debug.h"
#include "monica-zmq-defaults.h"

using namespace std;
using namespace Tools;
using namespace json11;
using namespace Monica;

string appName = "monica-zmq-control-send";
string version = "2.0.0-beta";

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	zmq::context_t context(1);

	bool debug = false, debugSet = false;
	string address = defaultControlAddress;
	int port = defaultControlPort;
	string command = "";
	int count = 1;

	bool connectToZmqProxy = false;
	string proxyAddress = defaultProxyAddress;
	int proxyFrontendPort = defaultProxyFrontendPort;
	int proxyBackendPort = defaultProxyBackendPort;
	bool usePipeline = false;
	string inputAddress = defaultInputAddress;
	int inputPort = defaultInputPort;
	string outputAddress = defaultOutputAddress;
	int outputPort = defaultOutputPort;
	string pubControlAddress = defaultPublisherControlAddress;
	int pubControlPort = defaultPublisherControlPort;
	
	auto printHelp = [=]()
	{
		cout
			<< appName << "[commands/options]" << endl
			<< endl
			<< "commands/options:" << endl
			<< endl
			<< " -h   | --help ... this help output" << endl
			<< " -v   | --version ... outputs " << appName << " version" << endl
			<< endl
			<< " -d   | --debug ... show debug outputs" << endl
			<< " -a   | --address CONTROL-ADDRESS (default: " << address << ") ... address of control node" << endl
			<< " -p   | --port CONTROL-PORT (default: " << port << ") ... port of control node" << endl
			<< " -n   | --start-new COUNT] ... start COUNT new MONICA nodes" << endl
			<< " -m   | --start-max COUNT ... start maximum COUNT MONICA nodes" << endl
			<< " -s   | --stop] COUNT ... stop COUNT MONICA nodes" << endl
			<< " -c   | --connect-to-proxy ... connect MONICA service to a ZeroMQ proxy and use proxy address/port defaults" << endl
			<< " -pa  | --proxy-address ADDRESS (default: " << inputAddress << ") ... proxy address to connect MONICA service to" << endl
			<< " -pfp | --proxy-frontend-port PORT (default: " << inputPort << ") ... proxy client side port of proxy to be used by MONICA service" << endl
			<< " -pbp | --proxy-backend-port PORT (default: " << inputPort << ") ... proxy service side port of proxy to be used by MONICA service" << endl
			<< " -ia  | --input-address ADDRESS (default: " << inputAddress << ") ... address to get inputs from for MONICA service" << endl
			<< " -ip  | --input-port PORT (default: " << inputPort << ") ... port to get inputs from for MONICA service" << endl
			<< " -od  | --output-defaults ... use MONICA service in a pipeline, but use output address/port defaults" << endl
			<< " -oa  | --output-address ADDRESS (default: " << outputAddress << ") ... address for send outputs of MONICA service to" << endl
			<< " -op  | --output-port PORT (default: " << outputPort << ") ... port to send outputs of MONICA service to" << endl
			<< " -pca | --publisher-control-address ADDRESS (default: " << pubControlAddress << ") ... address of a publisher where MONICA service will listen for control messages" << endl
			<< " -pcp | --publisher-control-port PORT (default: " << pubControlPort << ") ... port of a publisher where MONICA service will listen for control messages" << endl;
			
	};

	if(argc >= 1)
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
			else if((arg == "-n" || arg == "--start-new")
							&& i + 1 < argc)
				command = "start-new", count = atoi(argv[++i]);
			else if((arg == "-m" || arg == "--start-max")
							&& i + 1 < argc)
				command = "start-max", count = atoi(argv[++i]);
			else if((arg == "-s" || arg == "--stop")
							&& i + 1 < argc)
				command = "stop", count = atoi(argv[++i]);
			else if((arg == "-c" || arg == "--connect-to-proxy"))
				connectToZmqProxy = true;
			else if((arg == "-pa" || arg == "--proxy-address")
							&& i + 1 < argc)
				proxyAddress = argv[++i];
			else if((arg == "-pfp" || arg == "--proxy-frontend-port")
							&& i + 1 < argc)
				proxyFrontendPort = stoi(argv[++i]);
			else if((arg == "-pbp" || arg == "--proxy-backend-port")
							&& i + 1 < argc)
				proxyBackendPort = stoi(argv[++i]);
			else if((arg == "-ia" || arg == "--input-address")
							&& i + 1 < argc)
				inputAddress = argv[++i];
			else if((arg == "-ip" || arg == "--input-port")
							&& i + 1 < argc)
				inputPort = stoi(argv[++i]);
			if(arg == "-od" || arg == "--output-defaults")
				usePipeline = true;
			else if((arg == "-oa" || arg == "--output-address")
							&& i + 1 < argc)
				outputAddress = argv[++i], usePipeline = true;
			else if((arg == "-op" || arg == "--output-port")
							&& i + 1 < argc)
				outputPort = stoi(argv[++i]), usePipeline = true;
			else if((arg == "-pca" || arg == "--publisher-control-address")
							&& i + 1 < argc)
				pubControlAddress = argv[++i];
			else if((arg == "-pcp" || arg == "--publisher-control-port")
							&& i + 1 < argc)
				pubControlPort = stoi(argv[++i]);
			else if(arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if(arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		if(!command.empty())
		{
			zmq::socket_t socket(context, ZMQ_REQ);

			string address_ = string("tcp://") + address + ":" + to_string(port);
			try
			{
				socket.connect(address_);
				//cout << "Bound " << appName << " zeromq reply socket to address: " << address << "!" << endl;

				J11Object resultMsg;
				resultMsg["type"] = command;
				resultMsg["count"] = count;
				resultMsg["control-address"] = pubControlAddress;
				resultMsg["control-port"] = pubControlPort;
				if(usePipeline)
				{
					resultMsg["input-address"] = inputAddress;
					resultMsg["input-port"] = inputPort;
					resultMsg["output-address"] = outputAddress;
					resultMsg["output-port"] = outputPort;
				}
				else if(connectToZmqProxy)
				{
					resultMsg["proxy-address"] = proxyAddress;
					resultMsg["proxy-frontend-port"] = proxyFrontendPort; 
					resultMsg["proxy-backend-port"] = proxyBackendPort;
				}
				else
				{
					//resultMsg["service-address"] = inputAddress;
					resultMsg["service-port"] = inputPort;
				}
				
				try
				{
					s_send(socket, Json(resultMsg).dump());

					try
					{
						auto msg = receiveMsg(socket);
						if(msg.json["type"].string_value() == "result")
						{
							if(command == "start-new")
								cout << "OK: successfully started " << msg.json["started"].int_value() << " MONICA instances" << endl;
							else if(command == "start-max")
								cout 
								<< "OK: successfully started/stopped " 
								<< msg.json["started"].int_value() << "/" << msg.json["stopped"].int_value() 
								<< " MONICA instances" << endl;
							else if(command == "stop")
								cout << "OK: successfully stopped " << msg.json["stopped"].int_value() << " MONICA instances" << endl;
						}
					}
					catch(zmq::error_t e)
					{
						cerr
							<< "Exception on trying to receive 'ack' reply message on zmq socket with address: "
							<< address_ << "! Error: [" << e.what() << "]" << endl;
					}
				}
				catch(zmq::error_t e)
				{
					cerr
						<< "Exception on trying to send request message: " << Json(resultMsg).dump() << " on zmq socket with address: "
						<< address_ << "! Error: [" << e.what() << "]" << endl;
				}
			}
			catch(zmq::error_t e)
			{
				cerr << "Couldn't connect socket to address: " << address_ << "! Error: [" << e.what() << "]" << endl;
			}
		}
	}

	return 0;
}
