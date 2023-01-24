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
#include <kj/string.h>
#include <kj/thread.h>

#include "common/common.h"
#include "common/restorer.h"

#include "model.capnp.h"
#include "common.capnp.h"
#include "persistence.capnp.h"

namespace monica
{

typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText, mas::schema::common::StructuredText> MonicaEnvInstance;

class RunMonica final : public MonicaEnvInstance::Server
{
public:
  RunMonica(bool startedServerInDebugMode = false, mas::infrastructure::common::Restorer* restorer = nullptr); 

  kj::StringPtr getId() const { return _id; }
  void setId(kj::StringPtr id) { _id = kj::str(id); }

  kj::StringPtr getName() const { return _name; }
  void setName(kj::StringPtr name) { _name = kj::str(name); }

  kj::StringPtr getDescription() const { return _description; }
  void setDescription(kj::StringPtr description) { _description = kj::str(description); }

  MonicaEnvInstance::Client getClient() { return _client; }
  void setClient(MonicaEnvInstance::Client c) { _client = c; }

  mas::schema::common::Action::Client getUnregisterAction() { return _unregisterAction; }
  void setUnregisterAction(mas::schema::common::Action::Client unreg) { _unregisterAction = unreg; }

  kj::Promise<void> info(InfoContext context) override;

  kj::Promise<void> run(RunContext context) override;

  //kj::Promise<void> stop(StopContext context) override;

  //save @0 () -> (sturdyRef :Text, unsaveSR :Text);
  kj::Promise<void> save(SaveContext context) override;

  void setRestorer(mas::infrastructure::common::Restorer* restorer){  _restorer = restorer; }

private:
  // Implementation of the Model::Instance Cap'n Proto interface
  bool _startedServerInDebugMode{ false };
  kj::String _id, _name, _description;
  mas::schema::common::Action::Client _unregisterAction{ nullptr };
  mas::infrastructure::common::Restorer* _restorer{nullptr};
  MonicaEnvInstance::Client _client{nullptr};
};

} // namespace monica
