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

#ifndef SERVE_MONICA_ZMQ_H_
#define SERVE_MONICA_ZMQ_H_

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

#include "zmq.hpp"

#include "json11/json11.hpp"
#include "tools/json11-helper.h"

namespace Monica
{
	namespace ZmqServer
	{
		void startZeroMQMonica(zmq::context_t* zmqContext,
													 std::string inputSocketAddress,
													 std::string outputSocketAddress,
													 bool isInProcess = false);

		enum SocketType { Reply, ProxyReply, Pull, Push, Subscribe };
		enum SocketRole { ReceiveJob, SendResult, Control };
		enum SocketOp { bind, connect };
		struct SocketConfig
		{
			SocketType type;
			std::vector<std::string> addresses;
			SocketOp op;
		};
		void serveZmqMonicaFull(zmq::context_t* zmqContext,
														std::map<SocketRole, SocketConfig> socketAddresses);
	}
}

#endif
