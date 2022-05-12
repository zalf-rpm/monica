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
#include <capnp/orphan.h>

#include "tools/helper.h"
#include "tools/debug.h"
#include "common/rpc-connections.h"
#include "common/common.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "climate_data.capnp.h"
#include "persistence.capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace mas;

string appName = "monica-capnp-fbp-component";
string version = "0.0.1-beta";


int main(int argc, const char* argv[]) {

  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  infrastructure::common::ConnectionManager _conMan;
  auto ioContext = kj::setupAsyncIo();

  string in_sr;
  string out_sr;
  string fromAttr;
  string toAttr;

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
      << " -i | --in_sr ... input sturdy ref "
      "... sturdy ref to input channel " << endl
      << " -o | --out_sr ... output sturdy ref "
      "... sturdy ref to ouput channel" << endl
      << " -fa | --from_attr ... name of attribute to get env from" << endl
      << " -ta | --to_attr ... name of attribute to set result to" << endl;
  };

  if (argc >= 1) {
    for (auto i = 1; i < argc; i++) {
      string arg = argv[i];
      if (arg == "-d" || arg == "--debug") {
        activateDebug = true;
        startedServerInDebugMode = true;
      }
      else if (arg == "-i" || arg == "--in_sr") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          in_sr = argv[++i];
      } else if (arg == "-o" || arg == "--out_sr") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          out_sr = argv[++i];
      } else if (arg == "-fa" || arg == "--from_attr") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          fromAttr = argv[++i];
      } else if (arg == "-ta" || arg == "--to_attr") {
        if (i + 1 < argc && argv[i + 1][0] != '-')
          toAttr = argv[++i];
      } else if (arg == "-h" || arg == "--help")
        printHelp(), exit(0);
      else if (arg == "-v" || arg == "--version")
        cout << appName << " version " << version << endl, exit(0);
    }

    if (in_sr.empty() || out_sr.empty()) return 0;

    debug() << "MONICA: starting MONICA Cap'n Proto FBP component" << endl;   
    typedef schema::common::IP IP;
    typedef schema::common::Channel<IP> Channel;
    typedef schema::model::EnvInstance<schema::common::StructuredText, schema::common::StructuredText> MonicaEnvInstance;
    typedef schema::model::Env<schema::common::StructuredText> Env;

    auto inp = _conMan.tryConnectB(ioContext, in_sr).castAs<Channel::ChanReader>();
    auto outp = _conMan.tryConnectB(ioContext, out_sr).castAs<Channel::ChanWriter>();

    auto runMonica = kj::heap<RunMonica>(nullptr, startedServerInDebugMode);
    MonicaEnvInstance::Client runMonicaClient = kj::mv(runMonica);

    try 
    {
      while(true)
      {
        auto msg = inp.readRequest().send().wait(ioContext.waitScope);
        // check for end of data from in port
        if (msg.isDone()) break;
        else 
        {
          auto inIp = msg.getValue();
          auto attr = rpc::common::getIPAttr(inIp, fromAttr);
          auto env = attr.orDefault(inIp.getContent()).getAs<Env>();
          auto rreq = runMonicaClient.runRequest();
          rreq.setEnv(env);
          auto res = rreq.send().wait(ioContext.waitScope);
          if (res.hasResult() && res.getResult().hasValue())
          {
            auto resJsonStr = res.getResult().getValue();
            auto wreq = outp.writeRequest();
            auto outIp = wreq.initValue();
            
            // set content if not to be set as attribute
            if (toAttr.empty()) outIp.initContent().setAs<capnp::Text>(resJsonStr);
            // copy attributes, if any and set result as attribute, if requested
            auto toAttrBuilder = rpc::common::copyAndSetIPAttrs(inIp, outIp, toAttr);
            KJ_IF_MAYBE(builder, toAttrBuilder) builder->setAs<capnp::Text>(resJsonStr);

            wreq.send().wait(ioContext.waitScope);
          }
        }

      }

      auto wreq = outp.writeRequest();
      wreq.setDone();
      wreq.send().wait(ioContext.waitScope);
    } 
    catch(kj::Exception e)
    {
      std::cerr << "Exception: " << e.getDescription().cStr() << endl;
    }
  }

  debug() << "MONICA: stopped MONICA Cap'n Proto FBP component" << endl;
  return 0;
}
