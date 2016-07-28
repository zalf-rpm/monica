/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <thread>
#include <tuple>

#include "zmq.hpp"
#include "zhelpers.hpp"

#include "json11/json11.hpp"

#include "run-monica-zmq.h"
#include "cultivation-method.h"
#include "tools/debug.h"
#include "../io/database-io.h"
#include "tools/zmq-helper.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Soil;

//-----------------------------------------------------------------------------

Json Monica::sendZmqRequestMonicaFull(zmq::context_t* zmqContext, 
																			string socketAddress,
																			Json envJson)
{
	Json res;

	zmq::socket_t socket(*zmqContext, ZMQ_REQ);
	debug() << "MONICA: connecting monica zeromq request socket to address: " << socketAddress << endl;
	try
	{
		socket.connect(socketAddress);
		debug() << "MONICA: connected monica zeromq request socket to address: " << socketAddress << endl;

		//string s = envJson.dump();

		try
		{
			if(s_send(socket, envJson.dump()))
			{
				try
				{
					auto msg = receiveMsg(socket);
					res = msg.json;
				}
				catch(zmq::error_t e)
				{
					cerr
						<< "Exception on trying to receive reply message on zmq socket with address: "
						<< socketAddress << "! Error: [" << e.what() << "]" << endl;
				}
			}
		}
		catch(zmq::error_t e)
		{
			cerr
				<< "Exception on trying to request MONICA run with message on zmq socket with address: "
				<< socketAddress << "! Error: [" << e.what() << "]" << endl;
		}
	}
	catch(zmq::error_t e)
	{
		cerr << "Coulnd't connect socket to address: " << socketAddress << "! Error: " << e.what() << endl;
	}

	debug() << "exiting sendZmqRequestMonicaFull" << endl;
	return res;
}

//-----------------------------------------------------------------------------


