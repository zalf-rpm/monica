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

//#include "db/abstract-db-connections.h"
#include "tools/debug.h"
#include "run-monica.h"
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

#include "common/sole.hpp"

//using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Climate;
using namespace mas;

//std::map<std::string, DataAccessor> daCache;

DataAccessor fromCapnpData(
  const Tools::Date& startDate,
  const Tools::Date& endDate,
  capnp::List<mas::schema::climate::Element>::Reader header,
  capnp::List<capnp::List<float>>::Reader data) {
  typedef mas::schema::climate::Element E;

  if (data.size() == 0)
    return DataAccessor();

  DataAccessor da(startDate, endDate);
  //vector<double> d(data[0].size());
  for (uint i = 0; i < header.size(); i++) {
    auto vs = data[i];
    std::vector<double> d(data[0].size());
    //transform(vs.begin(), vs.end(), d.begin(), [](float f) { return f; });
    for (uint k = 0; k < vs.size(); k++)
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

J11Array fromCapnpSoilProfile(mas::schema::soil::Profile::Reader profile) {
  J11Array ls;
  for (const auto& layer : profile.getLayers()) {
    J11Object l;
    l["Thickness"] = layer.getSize();
    for (const auto& prop : layer.getProperties()) {
      switch (prop.getName()) {
      case mas::schema::soil::PropertyName::SAND: l["Sand"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::CLAY: l["Clay"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::SILT: l["Silt"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::ORGANIC_CARBON: l["SoilOrganicCarbon"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::ORGANIC_MATTER: l["SoilOrganicMatter"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::BULK_DENSITY: l["SoilBulkDensity"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::RAW_DENSITY: l["SoilRawDensity"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::P_H: l["pH"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::SOIL_TYPE: l["KA5TextureClass"] = prop.getType().cStr(); break;
      case mas::schema::soil::PropertyName::PERMANENT_WILTING_POINT: l["PermanentWiltingPoint"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::FIELD_CAPACITY: l["FieldCapacity"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::SATURATION: l["PoreVolume"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::SOIL_WATER_CONDUCTIVITY_COEFFICIENT: l["Lambda"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::SCELETON: l["Sceleton"] = prop.getF32Value() / 100.0; break;
      case mas::schema::soil::PropertyName::AMMONIUM: l["SoilAmmonium"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::NITRATE: l["SoilNitrate"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::CN_RATIO: l["CN"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::SOIL_MOISTURE: l["SoilMoisturePercentFC"] = prop.getF32Value(); break;
      case mas::schema::soil::PropertyName::IN_GROUNDWATER: l["is_in_groundwater"] = prop.getBValue(); break;
      case mas::schema::soil::PropertyName::IMPENETRABLE: l["is_impenetrable"] = prop.getBValue(); break;
      }
    }
    ls.push_back(l);
  }
  return ls;
}

RunMonica::RunMonica(mas::infrastructure::common::Restorer* restorer, bool startedServerInDebugMode) 
  : _restorer(restorer)
  ,  _startedServerInDebugMode(startedServerInDebugMode) 
{
  _id = sole::uuid4().str();
  _name = "Monica capnp server";
  _description = "";
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

kj::Promise<void> RunMonica::run(RunContext context) //override
{
  auto envR = context.getParams().getEnv();

  auto runMonica = [context, envR, this](DataAccessor da = DataAccessor()) mutable {
    std::string err;
    auto rest = envR.getRest();
    if (!rest.getStructure().isJson()) {
      return Monica::Output(std::string("Error: 'rest' field is not valid JSON!"));
    }

    const Json& envJson = Json::parse(rest.getValue().cStr(), err);
    //cout << "runMonica: " << envJson["customId"].dump() << endl;

    Env env;
    auto errors = env.merge(envJson);

    //if we got a capnproto soil profile in the message, overwrite an existing profile by that one
    if (envR.hasSoilProfile()) {
      auto layers = fromCapnpSoilProfile(envR.getSoilProfile());
      env.params.siteParameters.merge(J11Object{ {"SoilProfileParameters", layers} });
    }

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
        [](std::string soilTexture, size_t distance) {
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
            Tools::Date(sd.getDay(), sd.getMonth(), sd.getYear()),
            Tools::Date(ed.getDay(), ed.getMonth(), ed.getYear()),
            headerResponse.getHeader(), dataTResponse.getData());
        });
      });
    }).then([context, runMonica](DataAccessor da) mutable {
      auto out = runMonica(da);
      auto rs = context.getResults();
      auto res = rs.initResult();
      res.initStructure().setJson();
      res.setValue(out.toString());
      });
  } else {
    auto out = runMonica();
    auto rs = context.getResults();
    auto res = rs.initResult();
    res.initStructure().setJson();
    res.setValue(out.toString());
    return kj::READY_NOW;
  }
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
  if(_restorer)
  {
    _restorer->save(_client, context.getResults().initSturdyRef(), context.getResults().initUnsaveSR());
  }
  return kj::READY_NOW;
}

