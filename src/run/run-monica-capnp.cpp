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

#include "json11/json11.hpp"

#include "tools/helper.h"

#include "db/abstract-db-connections.h"
#include "tools/debug.h"
#include "run-monica.h"
#include "../io/database-io.h"
#include "../core/monica-model.h"
#include "json11/json11-helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "env-from-json-config.h"
#include "tools/algorithms.h"
#include "../io/csv-format.h"
#include "climate/climate-common.h"

#include "model.capnp.h"
#include "common.capnp.h"

#include "run-monica-capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Climate;
using namespace zalf::capnp;

//std::map<std::string, DataAccessor> daCache;

DataAccessor fromCapnpData(
  const Date& startDate,
  const Date& endDate,
  capnp::List<rpc::Climate::Element>::Reader header,
  capnp::List<capnp::List<float>>::Reader data) {
  typedef rpc::Climate::Element E;

  if (data.size() == 0)
    return DataAccessor();

  DataAccessor da(startDate, endDate);
  //vector<double> d(data[0].size());
  for (int i = 0; i < header.size(); i++) {
    auto vs = data[i];
    vector<double> d(data[0].size());
    //transform(vs.begin(), vs.end(), d.begin(), [](float f) { return f; });
    for (int k = 0; k < vs.size(); k++)
      d[k] = vs[k];
    switch (header[i]) {
      case E::TMIN: da.addClimateData(ACD::tmin, std::move(d)); break;
      case E::TAVG: da.addClimateData(ACD::tavg, std::move(d)); break;
      case E::TMAX: da.addClimateData(ACD::tmax, std::move(d)); break;
      case E::PRECIP: da.addClimateData(ACD::precip, std::move(d)); break;
      case E::RELHUMID: da.addClimateData(ACD::relhumid, std::move(d)); break;
      case E::WIND: da.addClimateData(ACD::wind, std::move(d)); break;
      case E::GLOBRAD: da.addClimateData(ACD::globrad, std::move(d)); break;
      default:;
    }
  }

  return da;
}

kj::Promise<void> RunMonicaImpl::info(InfoContext context) //override
{
  auto rs = context.getResults();
  rs.initInfo();
  rs.getInfo().setId("some monica_id");
  rs.getInfo().setName("Monica capnp server");
  rs.getInfo().setDescription("some description");
  return kj::READY_NOW;
}

kj::Promise<void> RunMonicaImpl::run(RunContext context) //override
{
  debug() << ".";

  auto envR = context.getParams().getEnv();

  auto runMonica = [context, envR, this](DataAccessor da = DataAccessor()) mutable {
    string err;
    auto rest = envR.getRest();
    if (!rest.getStructure().isJson()) {
      return Monica::Output(string("Error: 'rest' field is not valid JSON!"));
    }

    const Json& envJson = Json::parse(rest.getValue().cStr(), err);
    //cout << "runMonica: " << envJson["customId"].dump() << endl;

    Env env;
    auto errors = env.merge(envJson);

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

    Monica::Output out;
    if (eda.success()) {
      env.climateData = eda.result;

      env.debugMode = _startedServerInDebugMode && env.debugMode;

      env.params.userSoilMoistureParameters.getCapillaryRiseRate =
        [](string soilTexture, int distance) {
        return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
      };

      out = Monica::runMonica(env);
    }

    out.errors = eda.errors;
    out.warnings = eda.warnings;

    return out;
  };

  if (envR.hasTimeSeries()) {
    auto ts = envR.getTimeSeries();
    auto rangeProm = ts.rangeRequest().send();
    auto headerProm = ts.headerRequest().send();
    auto dataTProm = ts.dataTRequest().send();

    return rangeProm
      .then([KJ_MVCAP(headerProm), KJ_MVCAP(dataTProm)](auto&& rangeResponse) mutable {
      return headerProm
        .then([KJ_MVCAP(rangeResponse), KJ_MVCAP(dataTProm)](auto&& headerResponse) mutable {
        return dataTProm
          .then([KJ_MVCAP(rangeResponse), KJ_MVCAP(headerResponse)](auto&& dataTResponse) mutable {
          auto sd = rangeResponse.getStartDate();
          auto ed = rangeResponse.getEndDate();
          return fromCapnpData(
            Date(sd.getDay(), sd.getMonth(), sd.getYear()),
            Date(ed.getDay(), ed.getMonth(), ed.getYear()),
            headerResponse.getHeader(), dataTResponse.getData());
        });
      });
    }).then([context, runMonica](DataAccessor da) mutable {
      auto out = runMonica(da);
      auto rs = context.getResults();
      rs.initResult();
      rs.getResult().setValue(out.toString());
            });
  } else {
    auto out = runMonica();
    auto rs = context.getResults();
    rs.initResult();
    rs.getResult().setValue(out.toString());
    return kj::READY_NOW;
  }
}
