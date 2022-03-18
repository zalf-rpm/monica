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

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/rpc-twoparty.h>
#include <kj/thread.h>

#include "tools/helper.h"
#include "tools/debug.h"
#include "../mas-infrastructure/src/cpp/common/rpc-connections.h"
#include "../mas-infrastructure/src/cpp/common/common.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "cluster_admin_service.capnp.h"
#include "registry.capnp.h"
#include "climate_data.capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
//using namespace json11;
//using namespace Climate;
using namespace mas;

string appName = "monica-capnp-server";
string version = "1.0.0-beta";

typedef schema::model::EnvInstance<schema::common::StructuredText, schema::common::StructuredText> MonicaEnvInstance;
typedef schema::model::EnvInstanceProxy<schema::common::StructuredText, schema::common::StructuredText> MonicaEnvInstanceProxy;

int main(int argc, const char* argv[]) {

  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  infrastructure::common::ConnectionManager _conMan;
  auto ioContext = kj::setupAsyncIo();
  string proxyAddress = "localhost";
  int proxyPort = 6666;
  bool connectToProxy = false;

  string registrarSR;

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
      << " -cp | --connect-to-proxy "
      "... connect to proxy at -pa and -pp" << endl
      << " -pa | --proxy-address "
      "... ADDRESS (default: " << proxyAddress << ") "
      "... connects server to proxy running at given address" << endl
      << " -pp | --proxy-port ... PORT (default: " << proxyPort << ") "
      "... connects server to proxy running on given port." << endl
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
      } else if (arg == "-cp" || arg == "--connect-to-proxy") {
        connectToProxy = true;
      } else if (arg == "-pa" || arg == "--proxy-address") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          proxyAddress = argv[++i];
      } else if (arg == "-pp" || arg == "--proxy-port") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          proxyPort = stoi(argv[++i]);
      } else if (arg == "-rsr" || arg == "--registrar-sturdy-ref") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          registrarSR = argv[++i];
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

    if (connectToProxy) {
      /*
      //create client connection to proxy
      try {
        capnp::EzRpcClient client(proxyAddress, proxyPort);

        auto& cWaitScope = client.getWaitScope();

        // Request the bootstrap capability from the server.
        MonicaEnvInstanceProxy::Client cap = client.getMain<MonicaEnvInstanceProxy>();

        // Make a call to the capability.
        auto request = cap.registerEnvInstanceRequest();
        request.setInstance(runMonicaImplClient);
        auto response = request.send().wait(cWaitScope);
        unregister = response.getUnregister();
        runMonicaImpl.setUnregister(unregister);

        //if (hideServer)
        kj::NEVER_DONE.wait(cWaitScope);
      } catch (exception e) {
        cerr << "Couldn't connect to proxy at address: " << proxyAddress << ":" << proxyPort << endl
          << "Exception: " << e.what() << endl;
      }
      */
    } else {
      try {
        debug() << "monica: trying to bind to host: " << address << " port: " << port << endl;
        auto proms = _conMan.bind(ioContext, restorerClient, address, port);
        auto addrPromise = proms.first.fork().addBranch();
        auto addrStr = addrPromise.wait(ioContext.waitScope);
        restorerRef.setHost("10.10.24.210");//addrStr);
        auto portPromise = proms.second.fork().addBranch();
        auto port = portPromise.wait(ioContext.waitScope);
        restorerRef.setPort(port);
        cout << "monica: bound to host: " << address << " port: " << port << endl;

        auto restorerSR = restorerRef.sturdyRef();
        auto monicaSRs = restorerRef.save(runMonicaClient);
        cout << "monica: monica_sr: " << monicaSRs.first << endl;
        cout << "monica: restorer_sr: " << restorerSR << endl;

        if (port == 0) {
          // The address format "unix:/path/to/socket" opens a unix domain socket,
          // in which case the port will be zero.
          std::cout << "Listening on Unix socket..." << std::endl;
        }
        else {
          std::cout << "Listening on port " << port << "..." << std::endl;
        }

        kj::Promise<void> regProm(nullptr);

        if(!registrarSR.empty())
        {
          debug() << "monica: trying to register at registrar: " << registrarSR << endl;          
          regProm = _conMan.tryConnect(ioContext, registrarSR).then([&](capnp::Capability::Client&& client){
            registrar = client.castAs<schema::registry::Registrar>();
            auto request = registrar.registerRequest();
            request.setCap(runMonicaClient);
            request.setRegName("monica");
            request.setCategoryId("monica");
            return request.send();
          }).then([&](auto&& response){
            if(response.hasUnreg()) { 
              unregister = response.getUnreg();
              runMonicaRef.setUnregister(unregister);
            }
            if(response.hasReregSR()) reregSR = response.getReregSR().cStr();
            debug() << "monica: registered at registrar: " << registrarSR << endl;
          });
        }

        /*
        if(!registrarSR.empty())
        {
          debug() << "monica: trying to register at registrar: " << registrarSR << endl;          
          //auto registrar = _conMan.tryConnect(ioContext, registrarSR).wait(ioContext.waitScope).castAs<schema::registry::Registrar>();
          registrar = _conMan.tryConnectB(ioContext, registrarSR).castAs<schema::registry::Registrar>();
          auto request = registrar.registerRequest();
          request.setCap(runMonicaClient);
          request.setRegName("monica");
          request.setCategoryId("monica");
          auto response = request.send().wait(ioContext.waitScope);
          kj::NEVER_DONE.wait(ioContext.waitScope);
          if(response.hasUnreg()) { 
            unregister = response.getUnreg();
            runMonicaRef.setUnregister(unregister);
          }
          if(response.hasReregSR()) reregSR = response.getReregSR().cStr();
          debug() << "monica: registered at registrar: " << registrarSR << endl;
        }
        */

        regProm.wait(ioContext.waitScope);
        // Run forever, accepting connections and handling requests.
        kj::NEVER_DONE.wait(ioContext.waitScope);
      } catch (exception e) {
        cerr << "Exception: " << e.what() << endl;
      }
    } 
    /*
    else {
      capnp::EzRpcServer server(runMonicaImplClient, address + (port < 0 ? "" : string(":") + to_string(port)));

      // Write the port number to stdout, in case it was chosen automatically.
      auto& waitScope = server.getWaitScope();
      port = server.getPort().wait(waitScope);
      if (port == 0) {
        // The address format "unix:/path/to/socket" opens a unix domain socket,
        // in which case the port will be zero.
        std::cout << "Listening on Unix socket..." << std::endl;
      } else {
        std::cout << "Listening on port " << port << "..." << std::endl;
      }

      // Run forever, accepting connections and handling requests.
      kj::NEVER_DONE.wait(waitScope);
    }
    */

    debug() << "stopped Cap'n Proto MONICA server" << endl;
  }

  return 0;
}
