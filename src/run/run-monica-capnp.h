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

#include <kj/debug.h>
#include <kj/common.h>

#include <capnp/rpc-twoparty.h>
#include <kj/thread.h>

#include "common/common.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "persistence.capnp.h"

namespace Monica
{

  typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText, mas::schema::common::StructuredText> MonicaEnvInstance;

  class RunMonica final : public MonicaEnvInstance::Server
  {
  public:
    RunMonica(mas::infrastructure::common::Restorer* restorer, bool startedServerInDebugMode = false); 

    void setClient(MonicaEnvInstance::Client c) { _client = c; }

    void setUnregister(mas::schema::common::Action::Client unreg) { unregister = unreg; }

    kj::Promise<void> info(InfoContext context) override;

    kj::Promise<void> run(RunContext context) override;

    //kj::Promise<void> stop(StopContext context) override;

    //save @0 () -> (sturdyRef :Text, unsaveSR :Text);
    kj::Promise<void> save(SaveContext context) override;

  private:
    // Implementation of the Model::Instance Cap'n Proto interface
    bool _startedServerInDebugMode{ false };
    std::string _id, _name, _description;
    mas::schema::common::Action::Client unregister{ nullptr };
    mas::infrastructure::common::Restorer* _restorer{nullptr};
    MonicaEnvInstance::Client _client{nullptr};
  };

}
