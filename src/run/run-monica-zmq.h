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

#ifndef RUN_ZMQ_MONICA_H_
#define RUN_ZMQ_MONICA_H_

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <sstream>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

#ifndef NO_ZMQ
#include "zmq.hpp"
#endif

#include "run-monica.h"

namespace Monica
{
#ifndef NO_ZMQ
  void startZeroMQMonica(zmq::context_t* zmqContext, std::string inputSocketAddress, std::string outputSocketAddress, bool isInProcess = false);

	void startZeroMQMonicaFull(zmq::context_t* zmqContext, std::string replySocketAddress, bool debugMode = false);

	void runZeroMQMonicaFull(zmq::context_t* zmqContext, std::string socketAddress, Env env);
#endif
}

#endif
