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

#include "tools/debug.h"
#include "tools/json11-helper.h"

using namespace std;
using namespace Tools;
using namespace json11;

string appName = "monica-zmq-control-send";
string version = "2.0.0-beta";

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	zmq::context_t context(1);

	bool debug = false, debugSet = false;
	string address = "localhost";
	int port = 6666;
	string command = "";
	int count = 1;

	auto printHelp = [=]()
	{
		cout
			<< appName << endl
			<< " [-d | --debug] ... show debug outputs" << endl
			<< " [--use-zmq-proxy] ... connect MONICA process to a ZeroMQ proxy" << endl
			<< " [--zmq-client] ... run in client mode communicating to a MONICA ZeroMQ server" << endl
			<< " [--zmq-server] ... run in server mode communicating with MONICA ZeroMQ clients" << endl
			<< " [[-a | --address] CONTROL-ADDRESS (default: " << address << ")] ... address of control node" << endl
			<< " [[-p | --port] CONTROL-PORT (default: " << port << ")] ... port of control node" << endl
			<< " [[-n | --start-new] COUNT] ... start COUNT new MONICA nodes" << endl
			<< " [[-m | --start-max] COUNT] ... start maximum COUNT MONICA nodes" << endl
			<< " [[-s | --stop] COUNT] ... stop COUNT MONICA nodes" << endl
			<< " [send PARAMETERS] ... send command to MONICA ZeroMQ control node via calling 'monica-zmq-control-send' with PARMETERS" << endl
			<< " [-h | --help] ... this help output" << endl
			<< " [-v | --version] ... outputs MONICA version" << endl;
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
			{
				command = "start-new";
				count = atoi(argv[++i]);
			}
			else if((arg == "-m" || arg == "--start-max")
							&& i + 1 < argc)
			{
				command = "start-max";
				count = atoi(argv[++i]);
			}
			else if((arg == "-s" || arg == "--stop")
							&& i + 1 < argc)
			{
				command = "stop";
				count = atoi(argv[++i]);
			}
			else if(arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if(arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		if(!command.empty())
		{
			zmq::socket_t socket(context, ZMQ_REQ);

			string address = string("tcp://") + address + ":" + to_string(port);
			try
			{
				socket.connect(address);
				//cout << "Bound " << appName << " zeromq reply socket to address: " << address << "!" << endl;

				J11Object resultMsg;
				resultMsg["type"] = command;
				resultMsg["count"] = count;
				try
				{
					s_send(socket, Json(resultMsg).dump());

					try
					{
						auto msg = receiveMsg(socket);
						cout << "Received ack: " << msg.type() << endl;
					}
					catch(zmq::error_t e)
					{
						cerr
							<< "Exception on trying to receive 'ack' reply message on zmq socket with address: "
							<< address << "! Error: [" << e.what() << "]" << endl;
					}
				}
				catch(zmq::error_t e)
				{
					cerr
						<< "Exception on trying to send request message: " << Json(resultMsg).dump() << " on zmq socket with address: "
						<< address << "! Error: [" << e.what() << "]" << endl;
				}
			}
			catch(zmq::error_t e)
			{
				cerr << "Couldn't connect socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
			}
		}
	}

	return 0;
}
