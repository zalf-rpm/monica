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
#include "monica-zmq-defaults.h"

using namespace std;
using namespace Tools;
using namespace json11;
using namespace Monica;

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
		for (int i = 0; i < count; i++)
		{
			J11Object resultMsg;
			resultMsg["type"] = "finish";
			try
			{
				if (s_send(socket, Json(resultMsg).dump()))
				{
					debug() << "Send 'finish' message to a MONICA process" << endl;

					try
					{
						auto msg = receiveMsg(socket);
						if (msg.valid)
						{
							stopped++;
							debug() << "Received ack: " << msg.type() << endl;
						}
					}
					catch (zmq::error_t e)
					{
						cerr
							<< "Exception on trying to receive 'ack' reply message on zmq socket with address: "
							<< address << "! Error: [" << e.what() << "]" << endl;
					}
				}
			}
			catch (zmq::error_t e)
			{
				cerr
					<< "Exception on trying to send 'finish' message on zmq socket with address: " << address
					<< "! Error: [" << e.what() << "]" << endl;
			}
		}
	}
	catch (zmq::error_t e)
	{
		cerr << "Couldn't connect socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
	}

	return stopped;
}

int main(int argc,
	char** argv)
{
	int commPort = defaultControlPort;
	string proxyAddress = defaultProxyAddress;
	int frontendProxyPort = defaultProxyFrontendPort;
	int backendProxyPort = defaultProxyBackendPort;
	bool prs = false;

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
			<< " -prs | --pull-router-sockets ... use pull-router-sockets for pipeline proxy" << endl
			<< " -c | --port COMM-PORT (default: " << commPort << ") ... run " << appName << " with given control port" << endl
			<< " -a | --proxy-address PROXY-ADDRESS (default: " << proxyAddress << ") ... connect client to give IP address" << endl
			<< " -f | --frontend-proxy-port PROXY-PORT (default: " << frontendProxyPort << ") ... communicate with started MONICA ZeroMQ servers via given frontend proxy port" << endl
			<< " -b | --backend-proxy-port PROXY-PORT (default: " << backendProxyPort << ") ... connect started MONICA ZeroMQ servers to given backend proxy port" << endl
			<< " -d | --debug ... enable debug outputs" << endl;
	};

	for (auto i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if ((arg == "-c" || arg == "--comm-port")
			&& i + 1 < argc)
			commPort = stoi(argv[++i]);
		else if ((arg == "-a" || arg == "--proxy-address")
			&& i + 1 < argc)
			proxyAddress = argv[++i];
		else if ((arg == "-f" || arg == "--frontend-proxy-port")
			&& i + 1 < argc)
			frontendProxyPort = stoi(argv[++i]);
		else if ((arg == "-b" || arg == "--backend-proxy-port")
			&& i + 1 < argc)
			backendProxyPort = stoi(argv[++i]);
		else if (arg == "-prs" || arg == "--pull-router-sockets")
			prs = true;
		else if (arg == "-d" || arg == "--debug")
			activateDebug = true;
		else if (arg == "-h" || arg == "--help")
			printHelp(), exit(0);
		else if (arg == "-v" || arg == "--version")
			cout << appName << " version " << version << endl, exit(0);
	}

	zmq::context_t context(1);

	// setup a reply socket for managing (e.g. starting/stopping) MONICA processes
	zmq::socket_t socket(context, ZMQ_REP);

	string address = string("tcp://*:") + to_string(commPort);
	try
	{
		socket.bind(address);

		map<string, int> started;

		//loop until receive finish message
		while (true)
		{
			try
			{
				auto msg = receiveMsg(socket);
				if (!msg.valid)
					continue;

				debug() << "Received message: " << msg.toString() << endl;

				string msgType = msg.type();
				if (msgType == "finish")
				{
					J11Object resultMsg;
					resultMsg["type"] = "ack";
					try
					{
						s_send(socket, Json(resultMsg).dump());
					}
					catch (zmq::error_t e)
					{
						cerr
							<< "Exception on trying to reply to 'finish' request with 'ack' message on zmq socket with address: "
							<< address << "! Still will finish " << appName << "! Error: [" << e.what() << "]" << endl;
					}
					socket.setsockopt(ZMQ_LINGER, 0);
					socket.close();

					break;
				}
				else if (msgType == "start-new"
					|| msgType == "start-max"
					|| msgType == "stop")
				{
					Json& fmsg = msg.json;

					int count = fmsg["count"].int_value();

					string proxyAddress = fmsg["proxy-address"].string_value();
					int proxyFrontendPort = fmsg["proxy-frontend-port"].int_value();
					int proxyBackendPort = fmsg["proxy-backend-port"].int_value();

					//string serviceAddress = fmsg["service-address"].string_value();
					bool isService = !fmsg["service-port"].is_null();
					int servicePort = fmsg["service-port"].int_value();

					string controlAddresses = fmsg["control-addresses"].string_value();
					string inputAddresses = fmsg["input-addresses"].string_value();
					string outputAddresses = fmsg["output-addresses"].string_value();

					string addresses;
					string stopAddress;
					int stopPort;
					if (!proxyAddress.empty())
					{
						stopAddress = proxyAddress, stopPort = proxyFrontendPort;
						addresses = string(" -p tcp://") + proxyAddress + ":" + to_string(proxyBackendPort);
						if (!controlAddresses.empty())
							addresses += " -c " + controlAddresses;
					}
					else if (isService)
					{
						count = max(count, 1);
						//stopAddress = "127.0.0.1", stopPort = servicePort;
						stopAddress = "localhost", stopPort = servicePort;
						addresses = string(" -s tcp://*:") + to_string(servicePort);
						if (!controlAddresses.empty())
							addresses += " -c " + controlAddresses;
					}
					else if (!outputAddresses.empty() && !inputAddresses.empty())
					{
						addresses = string(" -i ") + inputAddresses + " -o " + outputAddresses;
						if (!controlAddresses.empty())
							addresses += " -c " + controlAddresses;
					}

#ifdef WIN32
					string cmd = string("start /b monica-zmq-server ") + addresses;
#else
					string cmd = string("monica-zmq-server") + addresses + " &";
#endif
					//cout << "addresses: " << addresses << endl;

					bool isStartMax = msgType == "startMax";
					bool isStop = msgType == "stop";
					int& started_ = started[addresses];
					int successfullyStarted = 0;
					int i = isStartMax ? started_ : 0;
					int stop = 0;
					if (isStartMax)
						stop = max(0, started_ - count);
					else if (isStop)
						stop = max(0, min(count, started_));

					if (!isStop)
					{
						for (; i < count; i++)
						{
							int res = system(cmd.c_str());
							Tools::debug() << "result of running '" << cmd << "': " << res << endl;
							started[addresses]++, successfullyStarted++;
						}
					}

					if ((isStartMax || isStop)
						&& !stopAddress.empty())
					{
						auto stopped = stopMonicaProcesses(context, stopAddress, stopPort, stop);
						if (isStop)
							started_ -= stopped;
					}

					J11Object resultMsg;
					resultMsg["type"] = "result";
					resultMsg["started"] = successfullyStarted;
					if (isStartMax || isStop)
						resultMsg["stopped"] = stop;
					try
					{
						s_send(socket, Json(resultMsg).dump());
					}
					catch (zmq::error_t e)
					{
						cerr
							<< "Exception on trying to reply with result message: " << Json(resultMsg).dump()
							<< " on zmq socket with address: " << address
							<< "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
					}
				}
				else if (msgType == "start-pipeline-proxies"
					|| msgType == "stop-pipeline-proxies")
				{
					Json& fmsg = msg.json;

					int inFrontendPort = fmsg["input-frontend-port"].int_value();
					int inBackendPort = fmsg["input-backend-port"].int_value();
					int outFrontendPort = fmsg["output-frontend-port"].int_value();
					int outBackendPort = fmsg["output-backend-port"].int_value();

					string inArgs = string(" -f ") + to_string(inFrontendPort) + " -b " + to_string(inBackendPort);
					string outArgs = string(" -f ") + to_string(outFrontendPort) + " -b " + to_string(outBackendPort);

#ifdef WIN32
					string inCmd = string("start /b monica-zmq-proxy -p ") + inArgs;
					string outCmd = string("start /b monica-zmq-proxy -p") + (prs ? "rs " : " ") + outArgs;
#else
					string inCmd = string("monica-zmq-proxy -p ") + inArgs + " &";
					string outCmd = string("monica-zmq-proxy -p") + (prs ? "rs " : " ") + outArgs + " &";
#endif
					//cout << "inCmd: " << inCmd << " outCmd: " << outCmd << endl;

					int res = system(inCmd.c_str());
					Tools::debug() << "result of running '" << inCmd << "': " << res << endl;

					res = system(outCmd.c_str());
					Tools::debug() << "result of running '" << outCmd << "': " << res << endl;

					J11Object resultMsg;
					resultMsg["type"] = "result";
					resultMsg["ok"] = true;
					try
					{
						s_send(socket, Json(resultMsg).dump());
					}
					catch (zmq::error_t e)
					{
						cerr
							<< "Exception on trying to reply with result message: " << Json(resultMsg).dump()
							<< " on zmq socket with address: " << address
							<< "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
					}
				}
			}
			catch (zmq::error_t e)
			{
				cerr
					<< "Exception on trying to receive request message on zmq socket with address: "
					<< address << "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
			}
		}
	}
	catch (zmq::error_t e)
	{
		cerr << "Couldn't bind socket to address: " << address << "! Error: [" << e.what() << "]" << endl;
	}
	Tools::debug() << "Bound " << appName << " zeromq reply socket to address: " << address << "!" << endl;

	Tools::debug() << "exiting monica-zmq-control" << endl;
	return 0;
}
