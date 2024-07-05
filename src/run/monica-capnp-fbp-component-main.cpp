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

#include <kj/debug.h>
#include <kj/common.h>
#include <kj/main.h>

#include <capnp/any.h>

#include "tools/debug.h"
#include "common/rpc-connection-manager.h"
#include "common/common.h"
#include "resource/version.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "fbp.capnp.h"

#define KJ_MVCAP(var) var = kj::mv(var)

using namespace std;
using namespace monica;
using namespace Tools;

class FBPMain {
public:
  explicit FBPMain(kj::ProcessContext &context)
      : ioContext(kj::setupAsyncIo()), conMan(ioContext), context(context) {}

  kj::MainBuilder::Validity setName(kj::StringPtr n) {
    name = str(n);
    return true;
  }

  kj::MainBuilder::Validity setFromAttr(kj::StringPtr name) {
    fromAttr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setToAttr(kj::StringPtr name) {
    toAttr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setInSr(kj::StringPtr name) {
    inSr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setOutSr(kj::StringPtr name) {
    outSr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setMonicaSr(kj::StringPtr name) {
    monicaSr = kj::str(name);
    return true;
  }


  kj::MainBuilder::Validity startComponent() {
    bool startedServerInDebugMode = false;

    debug() << "MONICA: starting MONICA Cap'n Proto FBP component" << endl;
    typedef mas::schema::fbp::IP IP;
    typedef mas::schema::fbp::Channel<IP> Channel;
    typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText, mas::schema::common::StructuredText> MonicaEnvInstance;
    typedef mas::schema::model::Env<mas::schema::common::StructuredText> Env;

    //std::cout << "inSr: " << inSr.cStr() << std::endl;
    //std::cout << "outSr: " << outSr.cStr() << std::endl;
    auto inp = conMan.tryConnectB(inSr.cStr()).castAs<Channel::ChanReader>();
    auto outp = conMan.tryConnectB(outSr.cStr()).castAs<Channel::ChanWriter>();

    MonicaEnvInstance::Client runMonicaClient(nullptr);
    if (monicaSr.size() > 0) {
      runMonicaClient = conMan.tryConnectB(monicaSr.cStr()).castAs<MonicaEnvInstance>();
    } else {
      runMonicaClient = kj::heap<RunMonica>(startedServerInDebugMode);
    }
    try {
      while (true) {
        KJ_LOG(INFO, "trying to read from IN port");
        auto msg = inp.readRequest().send().wait(ioContext.waitScope);
        KJ_LOG(INFO, "received msg from IN port");
        // check for end of data from in port
        if (msg.isDone()) {
          KJ_LOG(INFO, "received done -> exiting main loop");
          break;
        } else {
          auto inIp = msg.getValue();
          auto attr = mas::infrastructure::common::getIPAttr(inIp, fromAttr);
          auto env = attr.orDefault(inIp.getContent()).getAs<Env>();
          KJ_LOG(INFO, "received env -> running MONICA");
          auto rreq = runMonicaClient.runRequest();
          rreq.setEnv(env);
          auto res = rreq.send().wait(ioContext.waitScope);
          KJ_LOG(INFO, "received MONICA result");
          if (res.hasResult() && res.getResult().hasValue()) {
            KJ_LOG(INFO, "result is not empty");
            auto resJsonStr = res.getResult().getValue();
            auto wreq = outp.writeRequest();
            auto outIp = wreq.initValue();

            // set content if not to be set as attribute
            if (kj::size(toAttr) == 0) outIp.initContent().setAs<capnp::Text>(resJsonStr);
            // copy attributes, if any and set result as attribute, if requested
            auto toAttrBuilder = mas::infrastructure::common::copyAndSetIPAttrs(inIp, outIp,
                                                                                toAttr);//, capnp::toAny(resJsonStr));
            KJ_IF_MAYBE(builder, toAttrBuilder) builder->setAs<capnp::Text>(resJsonStr);
            KJ_LOG(INFO, "trying to send result on OUT port");
            wreq.send().wait(ioContext.waitScope);
            KJ_LOG(INFO, "sent result on OUT port");
          }
        }
      }
      KJ_LOG(INFO, "closing OUT port");
      outp.closeRequest().send().wait(ioContext.waitScope);
    }
    catch (const kj::Exception &e) {
      KJ_LOG(INFO, "Exception: ", e.getDescription());
      std::cerr << "Exception: " << e.getDescription().cStr() << endl;
    }

    return true;
  }

  kj::MainFunc getMain() {
    return kj::MainBuilder(context, kj::str("MONICA FBP Component v", VER_FILE_VERSION_STR), "Offers a MONICA service.")
        .addOptionWithArg({'n', "name"}, KJ_BIND_METHOD(*this, setName),
                          "<component-name>", "Give this component a name.")
        .addOptionWithArg({'f', "from_attr"}, KJ_BIND_METHOD(*this, setFromAttr),
                          "<attr>", "Which attribute to read the MONICA env from.")
        .addOptionWithArg({'t', "to_attr"}, KJ_BIND_METHOD(*this, setToAttr),
                          "<attr>", "Which attribute to write the MONICA result to.")
        .addOptionWithArg({'i', "env_in_sr"}, KJ_BIND_METHOD(*this, setInSr),
                          "<sturdy_ref>", "Sturdy ref to input channel.")
        .addOptionWithArg({'o', "result_out_sr"}, KJ_BIND_METHOD(*this, setOutSr),
                          "<sturdy_ref>", "Sturdy ref to output channel.")
        .addOptionWithArg({'m', "monica_sr"}, KJ_BIND_METHOD(*this, setMonicaSr),
                          "<sturdy_ref>", "Sturdy ref to MONICA instance.")
        .callAfterParsing(KJ_BIND_METHOD(*this, startComponent))
        .build();
  }

private:
  kj::AsyncIoContext ioContext;
  mas::infrastructure::common::ConnectionManager conMan;
  kj::String name;
  kj::ProcessContext &context;
  kj::String inSr;
  kj::String outSr;
  kj::String monicaSr;
  kj::String fromAttr;
  kj::String toAttr;
};

KJ_MAIN(FBPMain)

