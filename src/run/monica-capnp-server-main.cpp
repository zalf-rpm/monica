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
#include <fstream>
#include <string>
#include <tuple>
#include <vector>
#include <algorithm>

#include <kj/debug.h>
#include <kj/common.h>
#define KJ_MVCAP(var) var = kj::mv(var)
#include <kj/main.h>

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/rpc-twoparty.h>
#include <kj/thread.h>

#include "tools/helper.h"
#include "tools/debug.h"
#include "common/rpc-connections.h"
#include "common/common.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "cluster_admin_service.capnp.h"
#include "registry.capnp.h"
#include "climate_data.capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace mas;

string appName = "monica-capnp-server";
string version = "1.0.0-beta";

typedef schema::model::EnvInstance<schema::common::StructuredText, schema::common::StructuredText> MonicaEnvInstance;

int main(int argc, const char* argv[]) {

  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  infrastructure::common::ConnectionManager _conMan;
  auto ioContext = kj::setupAsyncIo();
  string proxyAddress = "localhost";
  int proxyPort = 6666;
  bool connectToProxy = false;

  string registrarSR;
  string sr;

  string address = "*";
  int port = 0;

  bool startedServerInDebugMode = false;

  auto printHelp = [=]() {
    cout
      << appName << "[options]" << endl
      << endl
      << "options:" << endl
      << endl
      << " -h | --help "
      "... this help output" << endl
      << " -v | --version "
      "... outputs " << appName << " version and ZeroMQ version being used" << endl
      << endl
      << " -d | --debug "
      "... show debug outputs" << endl
      << " -a | --address ... ADDRESS (default: " << address << ") "
      "... runs server bound to given address, may be '*' to bind to all local addresses" << endl
      << " -p | --port ... PORT (default: none) "
      "... runs the server bound to the port, PORT may be ommited to choose port automatically." << endl
      << " -rsr | ----registrar-sturdy-ref ... REGISTAR_STURDY_REF "
      "... register MONICA at the registrar" << endl;
  };

  if (argc >= 1) {
    for (auto i = 1; i < argc; i++) {
      string arg = argv[i];
      if (arg == "-d" || arg == "--debug") {
        activateDebug = true;
        startedServerInDebugMode = true;
      }
      else if (arg == "-a" || arg == "--address") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          address = argv[++i];
      } else if (arg == "-p" || arg == "--port") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          port = stoi(argv[++i]);
      } else if (arg == "-rsr" || arg == "--registrar-sturdy-ref") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          registrarSR = argv[++i];
      } else if (arg == "-sr" || arg == "--sturdy-ref") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          sr = argv[++i];
      } else if (arg == "-h" || arg == "--help")
        printHelp(), exit(0);
      else if (arg == "-v" || arg == "--version")
        cout << appName << " version " << version << endl, exit(0);
    }

    debug() << "starting Cap'n Proto MONICA server" << endl;

    auto restorer = kj::heap<rpc::common::Restorer>();
    auto& restorerRef = *restorer;
    schema::persistence::Restorer::Client restorerClient = kj::mv(restorer);
    auto runMonica = kj::heap<RunMonica>(&restorerRef, startedServerInDebugMode);
    auto& runMonicaRef = *runMonica;
    MonicaEnvInstance::Client runMonicaClient = kj::mv(runMonica);
    runMonicaRef.setClient(runMonicaClient);
    debug() << "created monica" << endl;

    schema::common::Action::Client unregister(nullptr);
    string reregSR;
    schema::registry::Registrar::Client registrar(nullptr);

    debug() << "monica: trying to bind to host: " << address << " port: " << port << endl;
    auto proms = _conMan.bind(ioContext, restorerClient, address, port);
    auto addrPromise = proms.first.fork().addBranch();
    auto addrStr = addrPromise.wait(ioContext.waitScope);
    restorerRef.setHost(address);//addrStr);
    auto portPromise = proms.second.fork().addBranch();
    auto port = portPromise.wait(ioContext.waitScope);
    restorerRef.setPort(port);
    cout << "monica: bound to host: " << address << " port: " << port << endl;

    auto restorerSR = restorerRef.sturdyRef();
    auto monicaSRs = restorerRef.save(runMonicaClient, sr);
    cout << "monica: monica_sr: " << monicaSRs.first << endl;
    cout << "monica: restorer_sr: " << restorerSR << endl;

    /*
    if (port == 0) {
      // The address format "unix:/path/to/socket" opens a unix domain socket,
      // in which case the port will be zero.
      std::cout << "Listening on Unix socket..." << std::endl;
    }
    else {
      std::cout << "Listening on port " << port << "..." << std::endl;
    }
    */

    if(!registrarSR.empty())
    {
      debug() << "monica: trying to register at registrar: " << registrarSR << endl;          
      registrar = _conMan.tryConnectB(ioContext, registrarSR).castAs<schema::registry::Registrar>();
      auto request = registrar.registerRequest();
      request.setCap(runMonicaClient);
      request.setRegName("monica");
      request.setCategoryId("monica");
      try {
        auto response = request.send().wait(ioContext.waitScope);
        if(response.hasUnreg()) { 
          unregister = response.getUnreg();
          runMonicaRef.setUnregister(unregister);
        }
        if(response.hasReregSR()) reregSR = response.getReregSR().cStr();
        debug() << "monica: registered at registrar: " << registrarSR << endl;
      } catch(kj::Exception e) {
        cout << "monica-capnp-server-main.cpp: Error sending register message to Registrar! Error description: " << e.getDescription().cStr() << endl;
      }
    }

    // Run forever, accepting connections and handling requests.
    kj::NEVER_DONE.wait(ioContext.waitScope);
  }

  debug() << "stopped Cap'n Proto MONICA server" << endl;
  return 0;
}
