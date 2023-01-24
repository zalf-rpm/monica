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

#pragma once

#include <string>

namespace monica
{
  static const char* defaultProxyAddress = "localhost";
  static const int defaultProxyFrontendPort = 5555;
  static const int defaultProxyBackendPort = 5566;
  static const char* defaultControlAddress = "localhost";
  static const int defaultControlPort = 8888;
  static const char* defaultPublisherControlAddress = "localhost";
  static const int defaultPublisherControlPort = 8899;
  static const char* defaultInputAddress = "localhost";
  static const int defaultInputPort = 6666;
  static const char* defaultOutputAddress = "localhost";
  static const int defaultOutputPort = 7777;

  static const char* defProxyFrontendAddress = "tcp://localhost:5555";
  static const char* defProxyBackendAddress = "tcp://localhost:5566";
  static const char* defControlAddress = "tcp://localhost:8888";
  static const char* defPublisherControlAddress = "tcp://localhost:8899";
  static const char* defInputAddress = "tcp://localhost:6666";
  static const char* defOutputAddress = "tcp://localhost:7777";
  static const char* defServeAddress = "tcp://*:6666";

}
