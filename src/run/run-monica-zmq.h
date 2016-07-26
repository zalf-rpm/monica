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

#ifndef RUN_MONICA_ZMQ_H_
#define RUN_MONICA_ZMQ_H_

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
#include "run-monica.h"

namespace Monica
{
  void startZeroMQMonica(zmq::context_t* zmqContext, 
												 std::string inputSocketAddress, 
												 std::string outputSocketAddress, 
												 bool isInProcess = false);

	void startZeroMQMonicaFull(zmq::context_t* zmqContext, 
														 std::string socketAddress, 
														 bool useZmqProxy);

	json11::Json runZeroMQMonicaFull(zmq::context_t* zmqContext, 
																	 std::string socketAddress, 
																	 Env env);
}

#endif
