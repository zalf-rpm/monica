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

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/main.h>
#include <kj/thread.h>
#define KJ_MVCAP(var) var = kj::mv(var)

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/rpc-twoparty.h>

#include "tools/helper.h"
#include "common/rpc-connections.h"
#include "common/common.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "registry.capnp.h"


namespace monica {

class MonicaCapnpServerMain
{
public:
  MonicaCapnpServerMain(kj::ProcessContext& context) 
  : restorer(kj::heap<mas::infrastructure::common::Restorer>())
  , conMan(restorer.get())
  , context(context)
  , ioContext(kj::setupAsyncIo()) 
  {}

  kj::MainBuilder::Validity setName(kj::StringPtr n) { name = kj::str(n); return true; }

  kj::MainBuilder::Validity setPort(kj::StringPtr portStr) { port = portStr.parseAs<int>(); return true; }

  kj::MainBuilder::Validity setHost(kj::StringPtr h) { host = kj::str(h); return true; }

  kj::MainBuilder::Validity setLocalHost(kj::StringPtr h) { localHost = kj::str(h); return true; }

  kj::MainBuilder::Validity setCheckPort(kj::StringPtr portStr) { checkPort = portStr.parseAs<int>(); return true; }

  kj::MainBuilder::Validity setCheckIP(kj::StringPtr ip) { checkIP = kj::str(ip); return true; }

  kj::MainBuilder::Validity setRegistrarSR(kj::StringPtr sr) { registrarSR = kj::str(sr); return true; }

  kj::MainBuilder::Validity setRegCategory(kj::StringPtr cat) { regCategory = kj::str(cat); return true; }

  kj::MainBuilder::Validity setDebug() { startedServerInDebugMode = true; return true; }

  kj::MainBuilder::Validity startService()
  {
    typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText, mas::schema::common::StructuredText> MonicaEnvInstance;

    KJ_LOG(INFO, "starting Cap'n Proto MONICA service");

    auto& restorerRef = *restorer;
    mas::schema::persistence::Restorer::Client restorerClient = kj::mv(restorer);
    auto runMonica = kj::heap<RunMonica>(&restorerRef, startedServerInDebugMode);
    auto& runMonicaRef = *runMonica;
    if (name.size() > 0) runMonicaRef.setName(name);
    MonicaEnvInstance::Client runMonicaClient = kj::mv(runMonica);
    runMonicaRef.setClient(runMonicaClient);
    KJ_LOG(INFO, "created monica");

    

    KJ_LOG(INFO, "trying to bind to", host, port);
    auto portPromise = conMan.bind(ioContext, restorerClient, host, port);
    auto succAndIP = mas::infrastructure::common::getLocalIP(checkIP, checkPort);
    if(kj::get<0>(succAndIP)) restorerRef.setHost(kj::get<1>(succAndIP));
    else restorerRef.setHost(localHost);
    auto port = portPromise.wait(ioContext.waitScope);
    restorerRef.setPort(port);
    KJ_LOG(INFO, "bound to", host, port);

    auto restorerSR = restorerRef.sturdyRefStr();
    auto monicaSR = kj::get<0>(restorerRef.saveStr(runMonicaClient));
    KJ_LOG(INFO, monicaSR);
    KJ_LOG(INFO, restorerSR);

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

    mas::schema::common::Action::Client unregister(nullptr);
    //mas::schema::persistence::SturdyRef::Reader reregSR(nullptr);
    mas::schema::registry::Registrar::Client registrar(nullptr);
    if(registrarSR.size() > 0)
    {
      KJ_LOG(INFO, "trying to register at", registrarSR);
      registrar = conMan.tryConnectB(ioContext, registrarSR).castAs<mas::schema::registry::Registrar>();
      auto request = registrar.registerRequest();
      request.setCap(runMonicaClient);
      request.setRegName(name.size() == 0 ? kj::str(runMonicaRef.getName(), "(", runMonicaRef.getId(), ")") : name.asPtr());
      request.setCategoryId(regCategory);
      auto xd = request.initXDomain();
      restorerRef.setVatId(xd.initVatId());
      xd.setRestorer(restorerClient);
      try {
        auto response = request.send().wait(ioContext.waitScope);
        if(response.hasUnreg()) { 
          unregister = response.getUnreg();
          runMonicaRef.setUnregisterAction(unregister);
        }
        //if(response.hasReregSR()) reregSR = response.getReregSR();
        KJ_LOG(INFO, "registered at", registrarSR);
      } catch(kj::Exception e) {
        KJ_LOG(ERROR, "Error sending register message to Registrar! Error", e.getDescription().cStr());
      }
    }

    // Run forever, accepting connections and handling requests.
    kj::NEVER_DONE.wait(ioContext.waitScope);

    KJ_LOG(INFO, "stopped Cap'n Proto MONICA server");
    return true;
  }

  kj::MainFunc getMain()
  {
    return kj::MainBuilder(context, "MONICA Cap'n Proto Server v0.1", "Offers a MONICA as a Cap'n Proto service.")
      .addOption({'d', "debug"}, KJ_BIND_METHOD(*this, setDebug),
                  "Activate debug output.")      
      .addOptionWithArg({'n', "name"}, KJ_BIND_METHOD(*this, setName),
                        "<instance-name>", "Give this MONICA instance a name.")
      .addOptionWithArg({'p', "port"}, KJ_BIND_METHOD(*this, setPort),
                        "<port>", "Which port to listen on. If omitted, will assign a free port.")
      .addOptionWithArg({'h', "host"}, KJ_BIND_METHOD(*this, setHost),
                        "<host-address (default: *)>", "Which address to bind to. * binds to all network interfaces.")
      .addOptionWithArg({'r', "registrar_sr"}, KJ_BIND_METHOD(*this, setRegistrarSR),
                        "<sturdy_ref>", "Sturdy ref to registrar.")
      .addOptionWithArg({"reg_category"}, KJ_BIND_METHOD(*this, setRegCategory),
                        "<category (default: monica)>", "Name of the category to register at.")
      .addOptionWithArg({"local_host (default: localhost)"}, KJ_BIND_METHOD(*this, setLocalHost),
                        "<IP_or_host_address>", "Use this host for sturdy reference creation.")
      .addOptionWithArg({"check_IP"}, KJ_BIND_METHOD(*this, setCheckIP),
                        "<IPv4 (default: 8.8.8.8)>", "IP to connect to in order to find local outside IP.")
      .addOptionWithArg({"check_port"}, KJ_BIND_METHOD(*this, setCheckPort),
                        "<port (default: 53)>", "Port to connect to in order to find local outside IP.")
      .callAfterParsing(KJ_BIND_METHOD(*this, startService))
      .build();
  }

private:
  kj::Own<mas::infrastructure::common::Restorer> restorer;
  mas::infrastructure::common::ConnectionManager conMan;
  kj::ProcessContext& context;
  kj::AsyncIoContext ioContext;
  kj::String name;
  kj::String host{kj::str("*")};
  kj::String localHost{kj::str("localhost")};
  int port{0};
  kj::String checkIP;
  int checkPort{0};
  kj::String registrarSR;
  kj::String regCategory{kj::str("monica")};
  bool startedServerInDebugMode{false};
};

}

KJ_MAIN(monica::MonicaCapnpServerMain)
