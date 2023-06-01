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

#include "run-monica-capnp.h"

#include <string>
#include <vector>

#include <kj/debug.h>
#include <kj/common.h>

#include "json11/json11.hpp"

#include "tools/helper.h"
#include "run-monica.h"
#include "climate/climate-file-io.h"
#include "capnp-helper.h"
#include "common/sole.hpp"

#include "common.capnp.h"

#define KJ_MVCAP(var) var = kj::mv(var)

using namespace monica;
using namespace Tools;
using namespace json11;
using namespace Climate;
using namespace mas;

//std::map<std::string, DataAccessor> daCache;

RunMonica::RunMonica(bool startedServerInDebugMode, mas::infrastructure::common::Restorer *restorer)
    : _restorer(restorer), _startedServerInDebugMode(startedServerInDebugMode) {
  _id = kj::str(sole::uuid4().str());
  _name = kj::str("Monica capnp server");
}


kj::Promise<void> RunMonica::info(InfoContext context) //override
{
  KJ_LOG(INFO, "info message received");
  auto rs = context.getResults();
  rs.setId(_id);
  rs.setName(_name);
  rs.setDescription(_description);
  return kj::READY_NOW;
}

kj::Promise<void> RunMonica::run(RunContext context)
{
  auto envR = context.getParams().getEnv();

  auto runMonica =
      [envR, this](const DataAccessor& da = DataAccessor(), J11Array soilLayers = J11Array()) mutable {
    std::string err;
    auto rest = envR.getRest();
    if (!rest.getStructure().isJson()) {
      return monica::Output(std::string("Error: 'rest' field is not valid JSON!"));
    }

    const Json &envJson = Json::parse(rest.getValue().cStr(), err);
    //cout << "runMonica: " << envJson["customId"].dump() << endl;

    Env env;
    auto errors = env.merge(envJson);

    if (!soilLayers.empty()) env.params.siteParameters.merge(J11Object{{"SoilProfileParameters", soilLayers}});

    EResult<DataAccessor> eda;
    if (da.isValid()) {
      eda.result = da;
    } else if (!env.climateData.isValid()) {
      if (!env.climateCSV.empty()) {
        eda = readClimateDataFromCSVStringViaHeaders(env.climateCSV, env.csvViaHeaderOptions);
      } else if (!env.pathsToClimateCSV.empty()) {
        eda = readClimateDataFromCSVFilesViaHeaders(env.pathsToClimateCSV, env.csvViaHeaderOptions);
      }
    }

    monica::Output out;
    if (eda.success()) {
      env.climateData = eda.result;

      env.debugMode = _startedServerInDebugMode && env.debugMode;

      env.params.userSoilMoistureParameters.getCapillaryRiseRate =
          [](std::string soilTexture, size_t distance) {
            return Soil::readCapillaryRiseRates().getRate(kj::mv(soilTexture), distance);
          };

      out = monica::runMonica(env);
    }

    out.errors = eda.errors;
    out.warnings = eda.warnings;
    return out;
  };

  auto proms = kj::heapArrayBuilder<kj::Promise<void>>(2);
  DataAccessor da;
  J11Array soilLayers;

  if (envR.hasTimeSeries()) {
    auto ts = envR.getTimeSeries();
    proms.add(dataAccessorFromTimeSeries(ts).then([&da](const DataAccessor &da2) { da = da2; }));
  } else {
    proms.add(kj::READY_NOW);
  }

  if (envR.hasSoilProfile()) {
    proms.add(fromCapnpSoilProfile(envR.getSoilProfile()).then([&soilLayers](auto &&layers) {
      soilLayers = layers;
    }));
  } else {
    proms.add(kj::READY_NOW);
  }

  return kj::joinPromises(proms.finish()).then([context, runMonica, da, soilLayers]() mutable {
    auto out = runMonica(da, soilLayers);
    auto rs = context.getResults();
    auto res = rs.initResult();
    res.initStructure().setJson();
    res.setValue(out.toString());
  });
}

/*
kj::Promise<void> RunMonica::stop(StopContext context) //override
{
  std::cout << "Stop received. Exiting. cout" << std::endl;
  KJ_LOG(INFO, "Stop received. Exiting.");
  return unregister.doRequest().send().then([](auto&&) { 
    std::cout << "exit(0)" << std::endl;
    exit(0); 
  });
}
*/

// struct SaveParams {
//     sealFor @0 :SturdyRef.Owner;
// }
// struct SaveResults {
//   sturdyRef @0 :SturdyRef;
//   unsaveSR  @1 :SturdyRef;
// }
// save @0 SaveParams -> SaveResults;
kj::Promise<void> RunMonica::save(SaveContext context) {
  KJ_LOG(INFO, "save message received");
  if (_restorer) {
    _restorer->save(_client, context.getResults().initSturdyRef(), context.getResults().initUnsaveSR());
  }
  return kj::READY_NOW;
}

