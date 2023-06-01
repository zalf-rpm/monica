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

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/main.h>
#include <capnp/ez-rpc.h>

#include "common/restorable-service-main.h"

#include "run-monica-capnp.h"
#include "model.capnp.h"
#include "common.capnp.h"

#define KJ_MVCAP(var) var = kj::mv(var)

namespace monica {

using RSM = mas::infrastructure::common::RestorableServiceMain;

class MonicaCapnpServerMain : public RSM
{
public:
  explicit MonicaCapnpServerMain(kj::ProcessContext& context)
  : RSM(context, "MONICA Cap'n Proto Server v0.1", "Offers a MONICA as a Cap'n Proto service.")
  {}

  kj::MainBuilder::Validity setDebug() { startedServerInDebugMode = true; return true; }

  kj::MainBuilder::Validity startService()
  {
    typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText, mas::schema::common::StructuredText> MonicaEnvInstance;

    KJ_LOG(INFO, "Starting Cap'n Proto MONICA service");

    auto ownedRunMonica = kj::heap<RunMonica>(startedServerInDebugMode);
    auto runMonica = ownedRunMonica.get();
    if (name.size() > 0) runMonica->setName(name);
    MonicaEnvInstance::Client runMonicaClient = kj::mv(ownedRunMonica);
    runMonica->setClient(runMonicaClient);
    KJ_LOG(INFO, "created MONICA service");
    
    startRestorerSetup(runMonicaClient);
    runMonica->setRestorer(restorer);

    auto monicaSR = restorer->saveStr(runMonicaClient, nullptr, nullptr, false).wait(ioContext.waitScope).sturdyRef;
    if(outputSturdyRefs && monicaSR.size() > 0) std::cout << "monicaSR=" << monicaSR.cStr() << std::endl;

    // Run forever, accepting connections and handling requests.
    kj::NEVER_DONE.wait(ioContext.waitScope);

    KJ_LOG(INFO, "stopped Cap'n Proto MONICA server");
    return true;
  }

  kj::MainFunc getMain()
  {
    return addRestorableServiceOptions()
      .addOption({'d', "debug"}, KJ_BIND_METHOD(*this, setDebug), "Activate debug output.")      
      .callAfterParsing(KJ_BIND_METHOD(*this, startService))
      .build();
  }

private:
  bool startedServerInDebugMode{false};
};

}

KJ_MAIN(monica::MonicaCapnpServerMain)
