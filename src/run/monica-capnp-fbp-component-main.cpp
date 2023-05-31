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
#include <kj/main.h>

#include <capnp/any.h>

#include "tools/helper.h"
#include "tools/debug.h"
#include "common/rpc-connection-manager.h"
#include "common/common.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "fbp.capnp.h"

#define KJ_MVCAP(var) var = kj::mv(var)

using namespace std;
using namespace monica;
using namespace Tools;

class FBPMain
{
public:
  explicit FBPMain(kj::ProcessContext &context)
  : ioContext(kj::setupAsyncIo()), conMan(ioContext), context(context) {}

  kj::MainBuilder::Validity setName(kj::StringPtr n) { name = str(n); return true; }

  kj::MainBuilder::Validity setFromAttr(kj::StringPtr name) { fromAttr = kj::str(name); return true; }

  kj::MainBuilder::Validity setToAttr(kj::StringPtr name) { toAttr = kj::str(name); return true; }

  kj::MainBuilder::Validity setInSr(kj::StringPtr name) { inSr = kj::str(name); return true; }

  kj::MainBuilder::Validity setOutSr(kj::StringPtr name) { outSr = kj::str(name); return true; }

  kj::MainBuilder::Validity startChannel()
  {
    bool startedServerInDebugMode = false;

    debug() << "MONICA: starting MONICA Cap'n Proto FBP component" << endl;   
    typedef mas::schema::fbp::IP IP;
    typedef mas::schema::fbp::Channel<IP> Channel;
    typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText, mas::schema::common::StructuredText> MonicaEnvInstance;
    typedef mas::schema::model::Env<mas::schema::common::StructuredText> Env;

    auto inp = conMan.tryConnectB(inSr.cStr()).castAs<Channel::ChanReader>();
    auto outp = conMan.tryConnectB(outSr.cStr()).castAs<Channel::ChanWriter>();
    //auto runMonicaClient = conMan.tryConnectB("capnp://insecure@10.10.24.218:9999/monica_sr").castAs<MonicaEnvInstance>();

    auto runMonica = kj::heap<RunMonica>(startedServerInDebugMode);
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
          auto attr = mas::infrastructure::common::getIPAttr(inIp, fromAttr);
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
            if (kj::size(toAttr) == 0) outIp.initContent().setAs<capnp::Text>(resJsonStr);
            // copy attributes, if any and set result as attribute, if requested
            auto toAttrBuilder = mas::infrastructure::common::copyAndSetIPAttrs(inIp, outIp, toAttr);//, capnp::toAny(resJsonStr));
            KJ_IF_MAYBE(builder, toAttrBuilder) builder->setAs<capnp::Text>(resJsonStr);

            wreq.send().wait(ioContext.waitScope);
          }
        }

      }

      auto wreq = outp.writeRequest();
      wreq.setDone();
      wreq.send().wait(ioContext.waitScope);
    } 
    catch(const kj::Exception& e)
    {
      std::cerr << "Exception: " << e.getDescription().cStr() << endl;
    }

    return true;
  }

  kj::MainFunc getMain()
  {
    return kj::MainBuilder(context, "MONICA FBP Component v0.1", "Offers a MONICA service.")
      .addOptionWithArg({'n', "name"}, KJ_BIND_METHOD(*this, setName),
                        "<component-name>", "Give this component a name.")
      .addOptionWithArg({'f', "from_attr"}, KJ_BIND_METHOD(*this, setFromAttr),
                        "<attr>", "Which attribute to read the MONICA env from.")
      .addOptionWithArg({'t', "to_attr"}, KJ_BIND_METHOD(*this, setToAttr),
                        "<attr>", "Which attribute to write the MONICA result to.")
      .addOptionWithArg({'i', "in_sr"}, KJ_BIND_METHOD(*this, setInSr),
                        "<sturdy_ref>", "Sturdy ref to input channel.")
      .addOptionWithArg({'o', "out_sr"}, KJ_BIND_METHOD(*this, setOutSr),
                        "<sturdy_ref>", "Sturdy ref to output channel.")      
      .callAfterParsing(KJ_BIND_METHOD(*this, startChannel))
      .build();
  }

private:
  kj::AsyncIoContext ioContext;
  mas::infrastructure::common::ConnectionManager conMan;
  kj::String name;
  kj::ProcessContext &context;
  kj::String inSr;
  kj::String outSr;
  kj::String fromAttr;
  kj::String toAttr;
};

KJ_MAIN(FBPMain)

