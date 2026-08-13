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

#include <iostream>
#include <string>

#include "monica-zmq-defaults.h"
#include "resource/version.h"
#include "serve-monica-zmq.h"
#include "tools/algorithms.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

string appName = "monica-zmq-server";
string version = VER_FILE_VERSION_STR;
;

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  string serveAddress = defServeAddress;
  string proxyAddress = defProxyBackendAddress;
  bool connectToZmqProxy = false;
  string inputAddress = defInputAddress;
  string outputAddress = defOutputAddress;
  bool usePipeline = false;
  bool useRouterOutputSocket = false;
  string controlAddress = defControlAddress;

  SocketOp inputOp = monica::connect;
  SocketOp outputOp = monica::connect;

  int major, minor, patch;
  zmq::version(&major, &minor, &patch);

  auto printHelp = [=]() {
    cout << appName << "[options]" << endl
         << endl
         << "options:" << endl
         << endl
         << " -h | --help ... this help output" << endl
         << " -v | --version ... outputs " << appName << " version and ZeroMQ version being used"
         << endl
         << endl
         << " -d | --debug ... show debug outputs" << endl
         << " -s | --serve-address [ADDRESS] (default: " << serveAddress
         << ")] ... serve MONICA on given address" << endl
         << " -p | --proxy-address [(PROXY-)ADDRESS1[,ADDRESS2,...]] (default: " << inputAddress
         << ")] ... receive work via proxy from given address(es)" << endl
         << " -bi | --bind-input ... bind the input port" << endl
         << " -ci | --connect-input (default) ... connect the input port" << endl
         << " -i | --input-address [bind|connect]|[ADDRESS1[,ADDRESS2,...]] "
            "(default: "
         << inputAddress << ")] ... receive work from given address(es)" << endl
         << " -bo | --bind-output ... bind the output port" << endl
         << " -co | --connect-output (default) ... connect the output port" << endl
         << " -o | --output-address [ADDRESS1[,ADDRESS2,...]] (default: " << outputAddress
         << ")] ... send results to this address(es)" << endl
         << " -or | --router-output-address [ADDRESS1[,ADDRESS2,...]] (default: " << outputAddress
         << ")] ... send results to this address(es) but use a router socket" << endl
         << " -c | --control-address [ADDRESS] (default: " << controlAddress
         << ")] ... connect MONICA server to this address for control messages" << endl;
  };

  zmq::context_t context(1);

  if (argc >= 1) {
    for (auto i = 1; i < argc; i++) {
      string arg = argv[i];
      if (arg == "-d" || arg == "--debug")
        activateDebug = true;
      else if (arg == "-s" || arg == "--serve-address") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          serveAddress = argv[++i];
      } else if (arg == "-p" || arg == "--proxy-address") {
        connectToZmqProxy = true;
        if (i + 1 < argc && argv[i + 1][0] != '-')
          proxyAddress = argv[++i];
      }
      if (arg == "-bi" || arg == "--bind-input")
        inputOp = monica::bind;
      if (arg == "-ci" || arg == "--connect-input")
        inputOp = monica::connect;
      else if (arg == "-i" || arg == "--input-address") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          inputAddress = argv[++i];
      }
      if (arg == "-bo" || arg == "--bind-output")
        outputOp = monica::bind;
      if (arg == "-co" || arg == "--connect-output")
        outputOp = monica::connect;
      else if (arg == "-o" || arg == "--output-address") {
        usePipeline = true;
        if (i + 1 < argc && argv[i + 1][0] != '-')
          outputAddress = argv[++i];
      } else if (arg == "-or" || arg == "--router-output-address") {
        usePipeline = true;
        useRouterOutputSocket = true;
        if (i + 1 < argc && argv[i + 1][0] != '-')
          outputAddress = argv[++i];
      } else if (arg == "-c" || arg == "--control-address") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          controlAddress = argv[++i];
      } else if (arg == "-h" || arg == "--help")
        printHelp(), exit(0);
      else if (arg == "-v" || arg == "--version")
        cout << appName << " version " << version << " ZeroMQ version: " << major << "." << minor
             << "." << patch << endl,
            exit(0);
    }

    debug() << "starting ZeroMQ MONICA server" << endl;

    map<SocketRole, SocketConfig> addresses;
    if (usePipeline) {
      addresses[ReceiveJob] = {Pull, splitString(inputAddress, ","), inputOp};
      addresses[SendResult] = {useRouterOutputSocket ? Router : Push,
                               splitString(outputAddress, ","), outputOp};
    } else if (connectToZmqProxy)
      addresses[ReceiveJob] = {ProxyReply, splitString(proxyAddress, ","), monica::connect};
    else
      addresses[ReceiveJob] = {Reply, splitString(serveAddress, ","), monica::bind};

    addresses[Control] = {Subscribe, vector<string>{controlAddress}, monica::connect};

    serveZmqMonicaFull(&context, addresses);

    debug() << "stopped ZeroMQ MONICA server" << endl;
  }

  return 0;
}
