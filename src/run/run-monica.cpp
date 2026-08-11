/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/
#include "run-monica.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <tuple>

#include "common/rpc-connection-manager.h"
#include "model/monica/monica_state.capnp.h"
#include <capnp/compat/json.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/filesystem.h>
#include <kj/string.h>

#include "../core/crop-module.h"
#include "../io/build-output.h"
#include "climate/climate-common.h"
#include "json11/json11-helper.h"
#include "tools/algorithms.h"
#include "tools/debug.h"

using namespace monica;
using namespace std;
using namespace Climate;
using namespace Tools;
using namespace Soil;
using namespace json11;

namespace { // private

Errors extractAndStore(const Json &jv, std::vector<CultivationMethod> &vec) {
  Errors es;
  vec.clear();
  for (const Json &cmj : jv.array_items()) {
    CultivationMethod v;
    es.append(cultivationmethod::merge(&v, cmj));
    vec.push_back(v);
  }
  return es;
}

Errors extractAndStoreCropRotations(const Json &jv,
                                    std::vector<CropRotation> &vec) {
  Errors es;
  vec.clear();
  for (const Json &cmj : jv.array_items()) {
    CropRotation v;
    es.append(crop_rotation_merge(&v, cmj));
    vec.push_back(v);
  }
  return es;
}

} // namespace

CropRotation
monica::makeCropRotation(Tools::Date start, Tools::Date end,
                         std::vector<CultivationMethod> cropRotation) {
  CropRotation cr;
  cr.start = start;
  cr.end = end;
  cr.cropRotation = cropRotation;
  return cr;
}

Errors monica::crop_rotation_merge(CropRotation *cr, json11::Json j) {
  Errors es;

  set_iso_date_value(cr->start, j, "start");
  set_iso_date_value(cr->end, j, "end");
  es.append(::extractAndStore(j["cropRotation"], cr->cropRotation));

  return es;
}

json11::Json monica::crop_rotation_to_json(const CropRotation *cr) {
  J11Array cra;
  for (const auto &c : cr->cropRotation)
    cra.push_back(cultivationmethod::to_json(&c));

  return json11::Json::object{{"type", "CropRotation"},
                              {"start", cr->start.toIsoDateString()},
                              {"end", cr->end.toIsoDateString()},
                              {"cropRotation", cra}};
}

Env monica::makeEnv(CentralParameterProvider &&cpp) {
  Env env;
  env.params = cpp;
  return env;
}

Errors monica::env_merge(Env *env, json11::Json j) {
  Errors es;

  es.append(centralparameterprovider::merge(&env->params, j["params"]));

  es.append(env->climateData.merge(j["climateData"]));

  env->events = j["events"];
  env->events2 = j["events2"];
  env->outputs = j["outputs"];

  es.append(::extractAndStore(j["cropRotation"], env->cropRotation));
  es.append(
      ::extractAndStoreCropRotations(j["cropRotations"], env->cropRotations));
  es.append(::extractAndStore(j["cropRotation2"], env->cropRotation2));
  es.append(
      ::extractAndStoreCropRotations(j["cropRotations2"], env->cropRotations2));

  set_bool_value(env->debugMode, j, "debugMode");

  set_string_value(env->climateCSV, j, "climateCSV");

  // move pathToClimateCSV whatever it is into a vector as we support multiple
  // climate files (merging)
  if (j["pathToClimateCSV"].is_string() &&
      !j["pathToClimateCSV"].string_value().empty()) {
    env->pathsToClimateCSV.push_back(j["pathToClimateCSV"].string_value());
  } else if (j["pathToClimateCSV"].is_array()) {
    for (const auto &path :
         toStringVector(j["pathToClimateCSV"].array_items())) {
      if (!path.empty())
        env->pathsToClimateCSV.push_back(path);
    }
  }
  env->csvViaHeaderOptions = j["csvViaHeaderOptions"];

  env->customId = j["customId"];
  set_string_value(env->sharedId, j, "sharedId");

  return es;
}

json11::Json monica::env_to_json(const Env *env) {
  J11Array cr;
  for (const auto &cm : env->cropRotation)
    cr.push_back(cultivationmethod::to_json(&cm));
  J11Array cr2;
  for (const auto &cm : env->cropRotation2)
    cr2.push_back(cultivationmethod::to_json(&cm));

  J11Array crs;
  for (const auto &c : env->cropRotations) {
    J11Array cr;
    for (const auto &cm : c.cropRotation)
      cr.push_back(cultivationmethod::to_json(&cm));

    auto cro = J11Object{{"from", c.start.toIsoDateString()},
                         {"end", c.end.toIsoDateString()},
                         {"cropRotation", cr}};
    crs.push_back(cro);
  }
  J11Array crs2;
  for (const auto &c : env->cropRotations2) {
    J11Array cr;
    for (const auto &cm : c.cropRotation)
      cr.push_back(cultivationmethod::to_json(&cm));

    auto cro = J11Object{{"from", c.start.toIsoDateString()},
                         {"end", c.end.toIsoDateString()},
                         {"cropRotation", cr}};
    crs2.push_back(cro);
  }

  return J11Object{
      {"type", "Env"},
      {"params", centralparameterprovider::to_json(&env->params)},
      {"cropRotation", cr},
      {"cropRotation2", cr2},
      {"cropRotations", crs},
      {"cropRotations2", crs2},
      {"climateData", env->climateData.to_json()},
      {"debugMode", env->debugMode},
      {"climateCSV", env->climateCSV},
      {"pathsToClimateCSV", toPrimJsonArray(env->pathsToClimateCSV)},
      {"csvViaHeaderOptions", env->csvViaHeaderOptions},
      {"customId", env->customId},
      {"sharedId", env->sharedId},
      {"events", env->events},
      {"events2", env->events2},
      {"outputs", env->outputs}};
}

bool monica::env_return_obj_outputs(const Env *env) {
  return env->outputs["obj-outputs?"].bool_value();
}

string monica::env_to_string(const Env *env) {
  ostringstream s;
  s << " noOfLayers: " << env->params.simulationParameters.p_NumberOfLayers
    << " layerThickness: " << env->params.simulationParameters.p_LayerThickness
    << endl;
  s << "ClimateData: from: " << env->climateData.startDate().toString()
    << " to: " << env->climateData.endDate().toString() << endl;
  s << "Fruchtfolge: " << endl;
  for (const CultivationMethod &cm : env->cropRotation)
    s << cultivationmethod::toString(&cm) << endl;
  s << "customId: " << env->customId.dump();
  return s.str();
}

/**
 * Interface method for python wrapping, so climate module
 * does not need to be wrapped by python.
 *
 * @param acd
 * @param data
 */
/*
void
Env::addOrReplaceClimateData(std::string name, const std::vector<double>& data)
{
  cout << "addOrReplaceClimsteData " << name.c_str() << endl;
  int acd = 0;
  if (name == "tmin")
    acd = tmin;
  else if (name == "tmax")
    acd = tmax;
  else if (name == "tavg")
    acd = tavg;
  else if (name == "precip")
    acd = precip;
  else if (name == "globrad")
    acd = globrad;
  else if (name == "wind")
    acd = wind;
  else if (name == "sunhours")
    acd = sunhours;
  else if (name == "relhumid")
    acd = relhumid;
  else if (name == "co2")
    acd = co2;
  else if (name == "et0")
    acd = et0;

  climateData.addOrReplaceClimateData(AvailableClimateData(acd), data);
}
*/

/*
pair<Date, map<Climate::ACD, double>>
climateDataForStep(const Climate::DataAccessor& da,
                   size_t stepNo,
                   double latitude)
{
  Date startDate = da.startDate();
  Date currentDate = startDate + stepNo;

  // test if data for relhumid are available; if not, value is set to -1.0
  double relhumid = da.hasAvailableClimateData(Climate::relhumid)
    ? da.dataForTimestep(Climate::relhumid, stepNo)
    : -1.0;

  double globrad = da.hasAvailableClimateData(Climate::globrad)
    ? da.dataForTimestep(Climate::globrad, stepNo)
    : (da.hasAvailableClimateData(Climate::sunhours)
       ? Tools::sunshine2globalRadiation(currentDate.julianDay(),
                                         da.dataForTimestep(Climate::sunhours,
stepNo), latitude, true) : -1.0);

  map<Climate::ACD, double> m
  {{ Climate::tmin, da.dataForTimestep(Climate::tmin, stepNo)}
  ,{ Climate::tavg, da.dataForTimestep(Climate::tavg, stepNo)}
  ,{ Climate::tmax, da.dataForTimestep(Climate::tmax, stepNo)}
  ,{ Climate::precip, da.dataForTimestep(Climate::precip, stepNo)}
  ,{ Climate::wind, da.dataForTimestep(Climate::wind, stepNo)}
  ,{ Climate::globrad, globrad}
  ,{ Climate::relhumid, relhumid }
  };
  return make_pair(currentDate, m);
}
*/

void writeDebugInputs(const Env &env, string fileName = "inputs.json") {
  ofstream pout;
  string path = Tools::fixSystemSeparator(
      centralparameterprovider::pathToOutputDir(&env.params));
  if (Tools::ensureDirExists(path)) {
    string pathToFile = path + "/" + fileName;
    pout.open(pathToFile);
    if (pout.fail()) {
      cerr << "Error couldn't open file: '" << pathToFile << "'." << endl;
      return;
    }
    pout << env_to_json(&env).dump() << endl;
    pout.flush();
    pout.close();
  } else
    cerr << "Error failed to create path: '" << path << "'." << endl;
}

template <typename T = int> Maybe<T> parseInt(const string &s) {
  Maybe<T> res;
  if (s.empty() || (s.size() > 1 && s.substr(0, 2) == "xx"))
    return res;
  else {
    try {
      res = stoi(s);
    } catch (exception e) {
    }
  }
  return res;
}

Spec monica::makeSpec(json11::Json j) {
  Spec spec;
  spec_merge(&spec, j);
  return spec;
}

Tools::Errors monica::spec_merge(Spec *spec, json11::Json j) {
  // init(start, j, "start");
  // init(end, j, "end");
  // init(at, j, "at");
  // init(from, j, "from");
  // init(to, j, "to");
  // Maybe<DMY> dummy;
  // init(dummy, j, "while");
  spec->startf = spec_create_expression_func(j["start"]);
  spec->endf = spec_create_expression_func(j["end"]);
  spec->atf = spec_create_expression_func(j["at"]);
  spec->fromf = spec_create_expression_func(j["from"]);
  spec->tof = spec_create_expression_func(j["to"]);
  spec->whilef = spec_create_expression_func(j["while"]);

  return {};
}

json11::Json monica::spec_to_json(const Spec *spec) { return spec->origSpec; }

std::function<bool(const MonicaModel &)>
monica::spec_create_expression_func(Json j) {
  // is an expression event
  if (j.is_array()) {
    if (auto f = buildCompareExpression(j.array_items()))
      return f;
  } else if (j.is_string()) {
    auto jts = j.string_value();
    if (!jts.empty()) {
      auto s = splitString(jts, "-");
      // is date event
      if (jts.size() == 10 && s.size() == 3 && s[0].size() == 4 &&
          s[1].size() == 2 && s[2].size() == 2) {
        auto year = parseInt<uint16_t>(s[0]);
        auto month = parseInt<uint8_t>(s[1]);
        auto day = parseInt<uint8_t>(s[2]);

        auto f = [day, month, year](const MonicaModel &monica) {
          auto cd = monica.currentStepDate;

          // build date to compare against
          // apply min() for day, to allow for matching of the last day for each
          // month by choosing 31st
          Date date(day.isNothing() ? cd.day()
                                    : min(day.value(), cd.daysInMonth()),
                    month.isNothing() ? cd.month() : month.value(),
                    year.isNothing() ? cd.year() : year.value(), false, true);

          return date == cd;
        };
        return f;
      } else { // treat all other strings as potential workstep event
        return [jts](const MonicaModel &monica) {
          const auto &currentEvents = monica.currentEvents;
          return currentEvents.find(jts) != currentEvents.end();
        };
      }
    }
  }

  return {};
}

void storeResults(const vector<OId> &outputIds, vector<J11Array> &results,
                  const MonicaModel &monica) {
  const auto &ofs = buildOutputTable().ofs;

  size_t i = 0;
  results.resize(outputIds.size());
  for (auto oid : outputIds) {
    auto ofi = ofs.find(oid.id);
    if (ofi != ofs.end())
      results[i].push_back(ofi->second(monica, oid));
    ++i;
  }
};

void storeResultsObj(const vector<OId> &outputIds, vector<J11Object> &results,
                     const MonicaModel &monica) {
  const auto &ofs = buildOutputTable().ofs;

  J11Object result;
  for (auto oid : outputIds) {
    auto ofi = ofs.find(oid.id);
    if (ofi != ofs.end())
      result[::monica::oid::outputName(&oid)] = ofi->second(monica, oid);
  }
  results.push_back(result);
};

void monica::store_data_aggregate_results(StoreData *sd) {
  if (!sd->intermediateResults.empty()) {
    if (sd->results.size() < sd->intermediateResults.size()) {
      sd->results.resize(sd->intermediateResults.size());
    }

    assert(sd->intermediateResults.size() == sd->outputIds.size());

    size_t i = 0;
    for (auto oid : sd->outputIds) {
      auto &ivs = sd->intermediateResults.at(i);
      if (!ivs.empty()) {
        if (ivs.front().is_string()) {
          switch (oid.timeAggOp) {
          case OId::FIRST:
            sd->results[i].push_back(ivs.front());
            break;
          case OId::LAST:
            sd->results[i].push_back(ivs.back());
            break;
          default:
            sd->results[i].push_back(ivs.front());
          }
        } else
          sd->results[i].push_back(applyOIdOP(oid.timeAggOp, ivs));

        sd->intermediateResults[i].clear();
      }
      ++i;
    }
  }
}

void monica::store_data_aggregate_results_obj(StoreData *sd) {
  if (!sd->intermediateResults.empty()) {
    assert(sd->intermediateResults.size() == sd->outputIds.size());

    J11Object result;
    size_t i = 0;
    for (auto oid : sd->outputIds) {
      auto &ivs = sd->intermediateResults.at(i);
      if (!ivs.empty()) {
        if (ivs.front().is_string()) {
          switch (oid.timeAggOp) {
          case OId::FIRST:
            result[monica::oid::outputName(&oid)] = ivs.front();
            break;
          case OId::LAST:
            result[monica::oid::outputName(&oid)] = ivs.back();
            break;
          default:
            result[monica::oid::outputName(&oid)] = ivs.front();
          }
        } else
          result[monica::oid::outputName(&oid)] =
              applyOIdOP(oid.timeAggOp, ivs);

        sd->intermediateResults[i].clear();
      }
      ++i;
    }
    sd->resultsObj.push_back(result);
  }
}

void monica::store_data_store_results_if_spec_applies(StoreData *sd,
                                                      const MonicaModel &monica,
                                                      bool storeObjOutputs) {
  auto &spec = sd->spec;
  auto &withinEventStartEndRange = sd->withinEventStartEndRange;
  auto &withinEventFromToRange = sd->withinEventFromToRange;
  string os = spec.origSpec.dump();
  bool isCurrentlyEndEvent = false;

  // check for possible start event (if one exists at all and just enter in that
  // case if it is false)
  if (withinEventStartEndRange.isNothing() ||
      !withinEventStartEndRange.value()) {
    if (spec.startf)
      withinEventStartEndRange = spec.startf(monica);
  }

  // check for end event (doesn't need a start event, but if there was one at
  // all, it has to be true)
  if (withinEventStartEndRange.isNothing() ||
      withinEventStartEndRange.isValue()) {
    if (spec.endf)
      isCurrentlyEndEvent = spec.endf(monica);
  }

  // do something if we are in start/end range or nothing is set at all (means
  // do it always)
  if (withinEventStartEndRange.isNothing() ||
      withinEventStartEndRange.value()) {
    // check for at event
    if (spec.atf && spec.atf(monica)) {
      if (storeObjOutputs)
        storeResultsObj(sd->outputIds, sd->resultsObj, monica);
      else
        storeResults(sd->outputIds, sd->results, monica);
    } else if (spec.fromf && spec.tof) { // or from/to range event
      bool isCurrentlyToEvent = false;
      if (withinEventFromToRange.isNothing() ||
          !withinEventFromToRange.value()) {
        withinEventFromToRange = spec.fromf(monica);
      } else if (withinEventFromToRange.isValue()) {
        isCurrentlyToEvent = spec.tof(monica);
      }

      if (withinEventFromToRange.value()) {
        // if while is specified together with a from/to range, store only if
        // the while is true but aggregate only if the range is left this means
        // the range specifies the extend of recording
        if (spec.whilef) {
          if (spec.whilef(monica))
            storeResults(sd->outputIds, sd->intermediateResults, monica);
        } else
          storeResults(sd->outputIds, sd->intermediateResults, monica);

        if (isCurrentlyToEvent) {
          if (storeObjOutputs)
            store_data_aggregate_results_obj(sd);
          else
            store_data_aggregate_results(sd);
          withinEventFromToRange = false;
        }
      }
    }
    // or a single while aggregating expression
    else if (spec.whilef) {
      if (spec.whilef(monica))
        storeResults(sd->outputIds, sd->intermediateResults, monica);
      else if (!sd->intermediateResults.empty() &&
               !sd->intermediateResults.front().empty()) {
        // if while event was not successful but we got intermediate results,
        // they should be aggregated
        if (storeObjOutputs)
          store_data_aggregate_results_obj(sd);
        else
          store_data_aggregate_results(sd);
      }
    }
  }

  if (isCurrentlyEndEvent)
    withinEventStartEndRange = false;
}

vector<StoreData> monica::setupStorage(const json11::Json &event2oids,
                                       const Date &startDate,
                                       const Date &endDate) {
  map<string, Json> shortcuts = {
      {"daily", J11Object{{"at", "xxxx-xx-xx"}}},
      {"monthly", J11Object{{"from", "xxxx-xx-01"}, {"to", "xxxx-xx-31"}}},
      {"yearly", J11Object{{"from", "xxxx-01-01"}, {"to", "xxxx-12-31"}}},
      {"run", J11Object{{"from", startDate.toIsoDateString()},
                        {"to", endDate.toIsoDateString()}}},
      {"crop", J11Object{{"from", "Sowing"}, {"to", "Harvest"}}}};

  vector<StoreData> storeData;

  const auto &e2os = event2oids.array_items();
  for (size_t i = 0, size = e2os.size(); i < size; i += 2) {
    StoreData sd;
    sd.spec.origSpec = e2os[i];
    Json spec = sd.spec.origSpec;
    // find shortcut for string or store string as 'at' pattern
    if (spec.is_string()) {
      auto ss = spec.string_value();
      auto ci = shortcuts.find(ss);
      if (ci != shortcuts.end())
        spec = ci->second;
      else
        spec = J11Object{{"at", ss}};
    } else if (spec.is_array() && spec.array_items().size() == 4 &&
               spec[0].is_string() &&
               (spec[0].string_value() == "while" ||
                spec[0].string_value() ==
                    "at")) { // an array means it's an expression pattern to be
                             // stored at 'at'
      auto sa = spec.array_items();
      spec = J11Object{
          {spec[0].string_value(), J11Array(sa.begin() + 1, sa.end())}};
    } else if (spec.is_array())
      spec = J11Object{{"at", spec}};
    // everything else (number, bool, null) we ignore
    // object is the default we assume
    else if (!spec.is_object())
      continue;

    // if "at" and "while" are missing, add by default an "every day" "at"
    if (spec["at"].is_null() && spec["while"].is_null() &&
        spec["from"].is_null() && spec["to"].is_null()) {
      auto o = spec.object_items();
      o["at"] = "xxxx-xx-xx";
      spec = o;
    }

    spec_merge(&sd.spec, spec);
    sd.outputIds = parseOutputIds(e2os[i + 1].array_items());

    storeData.push_back(sd);
  }

  return storeData;
}

struct DFSRes {
  kj::Own<MonicaModel> monica;
  uint16_t critPos{0};
  uint16_t cmitPos{0};
};

DFSRes deserializeFullState(kj::Own<const kj::ReadableFile> file,
                            bool serializedMonicaStateIsJson) {
  DFSRes res;
  auto allBytes = file->readAllBytes();
  if (serializedMonicaStateIsJson) {
    const capnp::JsonCodec json;
    capnp::MallocMessageBuilder msg;
    auto runtimeStateBuilder =
        msg.initRoot<mas::schema::model::monica::RuntimeState>();
    json.decode(allBytes.asChars(), runtimeStateBuilder);
    auto runtimeState = runtimeStateBuilder.asReader();
    res.monica = nullptr;
    res.monica = makeMonicaModel(runtimeState.getModelState());
  } else {
    kj::ArrayInputStream ais(allBytes);
    capnp::InputStreamMessageReader message(ais);
    auto runtimeState =
        message.getRoot<mas::schema::model::monica::RuntimeState>();
    res.monica = nullptr;
    res.monica = makeMonicaModel(runtimeState.getModelState());
  }
  return res;
}

std::pair<Output, Output> monica::runMonicaIC(Env env, bool isIC) {
  Output out, out2;
  bool returnObjOutputs = env_return_obj_outputs(&env);
  out.customId = env.customId;
  out2.customId = env.customId;

  activateDebug = env.debugMode;
  if (activateDebug)
    writeDebugInputs(env, "inputs.json");

  // prefer multiple crop rotations, but use a single rotation if there
  if (env.cropRotations.empty() && !env.cropRotation.empty()) {
    env.cropRotations.push_back(makeCropRotation(env.climateData.startDate(),
                                                 env.climateData.endDate(),
                                                 env.cropRotation));
  }
  if (isIC && env.cropRotations2.empty() && !env.cropRotation2.empty()) {
    env.cropRotations2.push_back(makeCropRotation(env.climateData.startDate(),
                                                  env.climateData.endDate(),
                                                  env.cropRotation2));
  }

  debug() << "starting Monica" << endl;
  debug() << "-----" << endl;
  kj::Own<MonicaModel> monica, monica2;

  if (env.params.simulationParameters.loadSerializedMonicaStateAtStart) {
    auto pathToSerFile =
        kj::str(env.params.simulationParameters.pathToLoadSerializationFile);
    auto fs = kj::newDiskFilesystem();
    auto file =
        isAbsolutePath(pathToSerFile.cStr())
            ? fs->getRoot().openFile(fs->getCurrentPath().eval(pathToSerFile))
            : fs->getRoot().openFile(kj::Path::parse(pathToSerFile));

    auto dserRes = deserializeFullState(
        kj::mv(file),
        env.params.simulationParameters.deserializedMonicaStateFromJson);
    monica = kj::mv(dserRes.monica);
  } else {
    monica = makeMonicaModel(env.params);
    monica->simPs.startDate = env.climateData.startDate();
  }
  bool isSyncIC = false;
  if (isIC) {
    monica->intercropping = env.ic;
    isSyncIC = !monica->intercropping.isAsync();
    if (isSyncIC) {
      monica2 = makeMonicaModel(env.params);
      monica2->simPs.startDate = env.climateData.startDate();
    }
  }

  monica->simPs.endDate = env.climateData.endDate();
  monica->simPs.noOfPreviousDaysSerializedClimateData =
      env.params.simulationParameters.noOfPreviousDaysSerializedClimateData;
  if (isSyncIC) {
    monica2->simPs.endDate = env.climateData.endDate();
    monica2->simPs.noOfPreviousDaysSerializedClimateData =
        env.params.simulationParameters.noOfPreviousDaysSerializedClimateData;
  }

  debug() << "currentDate" << endl;
  Date currentDate = env.climateData.startDate();

  // create a way for worksteps to let the runtime calculate at a daily basis
  // things a workstep needs when being executed e.g. to actually accumulate
  // values from days before the workstep (for calculating a moving window of
  // past values)
  int dailyFuncId = 0, dailyFuncId2 = 0;
  map<int, vector<double>> dailyValues, dailyValues2;
  vector<function<void()>> applyDailyFuncs, applyDailyFuncs2;

  // iterate through all the worksteps in the croprotation(s) and check for
  // functions which have to run daily
  for (auto &cr : env.cropRotations) {
    for (auto &cm : cr.cropRotation) {
      for (auto wsptr : cm.allWorksteps) {
        auto df = workstep::registerDailyFunction(
            wsptr.get(), [&dailyValues, dailyFuncId]() -> vector<double> & {
              return dailyValues[dailyFuncId];
            });
        if (df) {
          applyDailyFuncs.push_back([&monica, df, dailyFuncId, &dailyValues] {
            dailyValues[dailyFuncId].push_back(df(monica.get()));
          });
        }
        dailyFuncId++;
      }
    }
  }
  if (isSyncIC) {
    for (auto &cr : env.cropRotations2) {
      for (auto &cm : cr.cropRotation) {
        for (auto wsptr : cm.allWorksteps) {
          auto df = workstep::registerDailyFunction(
              wsptr.get(), [&dailyValues2, dailyFuncId2]() -> vector<double> & {
                return dailyValues2[dailyFuncId2];
              });
          if (df) {
            applyDailyFuncs2.push_back(
                [&monica2, df, dailyFuncId2, &dailyValues2] {
                  dailyValues2[dailyFuncId2].push_back(df(monica2.get()));
                });
          }
          dailyFuncId2++;
        }
      }
    }
  }

  auto crit = env.cropRotations.begin();
  auto crit2 = env.cropRotations2.begin();
  // after loading deserialized state, move the iterator to the previous
  // position if possible
  //!!! attention doesn't check currently if the env is the same as when the
  //!state had been serialized !!!
  // while (critPos-- > 0 && crit + 1 != env.cropRotations.end())
  //   crit++;

  // cropRotation is a shadow of the env.cropRotation, which will hold pointers
  // to CMs in env.cropRotation, but might shrink if pure absolute CMs are
  // finished
  vector<CultivationMethod *> cropRotation, cropRotation2;

  auto checkAndInitShadowOfNextCropRotation_ =
      [](auto &envCropRotations, auto &crit, auto &cropRotation,
         Date currentDate) {
        if (crit != envCropRotations.end()) {
          // if current cropRotation is finished, try to move to next
          if (crit->end.isValid() && currentDate == crit->end + 1) {
            crit++;
            cropRotation.clear();
          }

          // check again, because we might have moved to next cropRotation
          if (crit != envCropRotations.end()) {
            // if a new cropRotation starts, copy the pointers to the CMs to the
            // shadow CR
            if (crit->start.isValid() && currentDate == crit->start) {
              for (auto &cm : crit->cropRotation)
                cropRotation.push_back(&cm);
              return true;
            }
          }
        }
        return false;
      };

  auto checkAndInitShadowOfNextCropRotation = [&](Date currentDate) {
    return checkAndInitShadowOfNextCropRotation_(env.cropRotations, crit,
                                                 cropRotation, currentDate);
  };
  auto checkAndInitShadowOfNextCropRotation2 = [&](Date currentDate) {
    return checkAndInitShadowOfNextCropRotation_(env.cropRotations2, crit2,
                                                 cropRotation2, currentDate);
  };

  // iterator through the crop rotation
  auto cmit = cropRotation.begin();
  auto cmit2 = cropRotation2.begin();
  // after loading deserialized state, move the iterator to the previous
  // position if possible
  //!!! attention doesn't check currently if the env is the same as when the
  //!state had been serialized !!!
  // while (cmitPos-- > 0 && cmit + 1 != cropRotation.end())
  //	cmit++;

  auto findNextCultivationMethod_ = [&](Date currentDate, auto &cropRotation,
                                        auto &cmit,
                                        bool advanceToNextCM = true) {
    CultivationMethod *currentCM = nullptr;
    Date nextAbsoluteCMApplicationDate;

    // it might be possible that the next cultivation method has to be skipped
    // (if cover/catch crop)
    bool notFoundNextCM = true;
    while (notFoundNextCM) {
      if (advanceToNextCM) {
        // delete fully cultivation methods with only absolute worksteps,
        // because they won't participate in a new run when wrapping the crop
        // rotation
        if (cultivationmethod::areOnlyAbsoluteWorksteps(*cmit) || !(*cmit)->repeat)
          cmit = cropRotation.erase(cmit);
        else
          cmit++;

        // start anew if we reached the end of the crop rotation
        if (cmit == cropRotation.end())
          cmit = cropRotation.begin();
      }

      // check if there's at least a cultivation method left in cropRotation
      if (cmit != cropRotation.end()) {
        advanceToNextCM = true;
        currentCM = *cmit;

        // addedYear tells that the start of the cultivation method was before
        // currentDate and thus the whole CM had to be moved into the next year
        // is possible for relative dates
        bool addedYear = cultivationmethod::reinit(currentCM, currentDate);
        if (addedYear) {
          // current CM is a cover crop, check if the latest sowing date would
          // have been before current date, if so, skip current CM
          if (currentCM->isCoverCrop) {
            // if current CM's latest sowing date is actually after current
            // date, we have to reinit current CM again, but this time prevent
            // shifting it to the next year
            if (!(notFoundNextCM = cultivationmethod::absLatestSowingDate(currentCM).withYear(
                                       currentDate.year()) < currentDate)) {
              cultivationmethod::reinit(currentCM, currentDate, true);
            }
          } else
            notFoundNextCM =
                currentCM->canBeSkipped; // if current CM was marked skipable,
                                           // skip it
        } else { // not added year or CM was had also absolute dates
          if (currentCM->isCoverCrop)
            notFoundNextCM = cultivationmethod::absLatestSowingDate(currentCM) < currentDate;
          else if (currentCM->canBeSkipped)
            notFoundNextCM = cultivationmethod::absStartDate(currentCM) < currentDate;
          else
            notFoundNextCM = false;
        }

        if (notFoundNextCM)
          nextAbsoluteCMApplicationDate = Date();
        else {
          nextAbsoluteCMApplicationDate = cultivationmethod::staticWorksteps(currentCM).empty()
                                              ? Date()
                                              : cultivationmethod::absStartDate(currentCM, false);
          debug() << "new valid next abs app-date: "
                  << nextAbsoluteCMApplicationDate.toString() << endl;
        }
      } else {
        currentCM = nullptr;
        nextAbsoluteCMApplicationDate = Date();
        notFoundNextCM = false;
      }
    }

    return make_pair(currentCM, nextAbsoluteCMApplicationDate);
  };

  auto findNextCultivationMethod = [&](Date currentDate,
                                       bool advanceToNextCM = true) {
    return findNextCultivationMethod_(currentDate, cropRotation, cmit,
                                      advanceToNextCM);
  };
  auto findNextCultivationMethod2 = [&](Date currentDate,
                                        bool advanceToNextCM = true) {
    return findNextCultivationMethod_(currentDate, cropRotation2, cmit2,
                                      advanceToNextCM);
  };

  // direct handle to current cultivation method
  CultivationMethod *currentCM{nullptr};
  CultivationMethod *currentCM2{nullptr};
  Date nextAbsoluteCMApplicationDate, nextAbsoluteCMApplicationDate2;
  tie(currentCM, nextAbsoluteCMApplicationDate) =
      findNextCultivationMethod(currentDate, false);
  if (isSyncIC)
    tie(currentCM2, nextAbsoluteCMApplicationDate2) =
        findNextCultivationMethod2(currentDate, false);

  // while (cmitPos-- > 0 && cmit + 1 != cropRotation.end())
  //	tie(currentCM, nextAbsoluteCMApplicationDate) =
  //findNextCultivationMethod(currentDate, true);;

  vector<StoreData> store = setupStorage(
      env.events, env.climateData.startDate(), env.climateData.endDate());
  vector<StoreData> store2;
  if (isSyncIC)
    store2 = setupStorage(env.events2, env.climateData.startDate(),
                          env.climateData.endDate());

  monica->currentEvents.insert("run-started");
  if (isSyncIC)
    monica2->currentEvents.insert("run-started");
  for (size_t d = 0, nods = env.climateData.noOfStepsPossible(); d < nods;
       ++d, ++currentDate) {
    debug() << "currentDate: " << currentDate.toString() << endl;

    if (checkAndInitShadowOfNextCropRotation(currentDate)) {
      // cmit = cropRotation.empty() ? cropRotation.end() :
      // cropRotation.begin();
      cmit = cropRotation.begin();
      tie(currentCM, nextAbsoluteCMApplicationDate) =
          findNextCultivationMethod(currentDate, false);
    }
    if (isSyncIC && checkAndInitShadowOfNextCropRotation2(currentDate)) {
      // cmit = cropRotation.empty() ? cropRotation.end() :
      // cropRotation.begin();
      cmit2 = cropRotation2.begin();
      tie(currentCM2, nextAbsoluteCMApplicationDate2) =
          findNextCultivationMethod2(currentDate, false);
    }

    monicamodel::dailyReset(monica.get());
    if (isSyncIC)
      monicamodel::dailyReset(monica2.get());

    // set the soil moisture of the monica1's soil column to monica2's soil
    // column (from previous day)
    if (isSyncIC) {
      const auto &cps = monica->cropPs;
      if (cps.twoWaySync && cps.sequentialWaterUse) {
        auto nols = monica->soilColumn->size();
        KJ_ASSERT((nols == monica2->soilColumn->size()));
        for (size_t i = 0; i < nols; i++) {
          (*(monica->soilColumn))[i].vs_SoilMoisture_m3 =
              (*(monica2->soilColumn))[i].vs_SoilMoisture_m3;
        }
      }
    }

    monica->currentStepDate = currentDate;
    if (isSyncIC)
      monica2->currentStepDate = currentDate;
    monica->climateData.push_back(env.climateData.allDataForStep(
        d, env.params.siteParameters.vs_Latitude));
    if (isSyncIC) {
      auto cd = env.climateData.allDataForStep(
          d, env.params.siteParameters.vs_Latitude);
      // in case of sequential water use activated, set the precipitation for
      // the second monica to 0
      if (monica->cropPs.sequentialWaterUse)
        cd[Climate::precip] = 0;
      monica2->climateData.push_back(cd);
    }

    // test if monica's crop has been dying in previous step
    // if yes, it will be incorporated into soil
    if (monica->currentCropModule && monica->currentCropModule->dyingOut) {
      monicamodel::incorporateCurrentCrop(monica.get());
    }
    if (isSyncIC && monica2->currentCropModule &&
        monica2->currentCropModule->dyingOut) {
      monicamodel::incorporateCurrentCrop(monica2.get());
    }

    // try to apply dynamic worksteps marked to run before everything else that
    // day
    if (currentCM)
      cultivationmethod::apply(currentCM, monica.get(), true);
    if (isSyncIC && currentCM2)
      cultivationmethod::apply(currentCM2, monica2.get(), true);

    // apply worksteps and cycle through crop rotation
    if (currentCM && nextAbsoluteCMApplicationDate == currentDate) {
      debug() << "MONICA 1: applying absolute-at: "
              << nextAbsoluteCMApplicationDate.toString() << endl;
      cultivationmethod::absApply(currentCM, nextAbsoluteCMApplicationDate, monica.get());

      nextAbsoluteCMApplicationDate =
          cultivationmethod::nextAbsDate(currentCM, nextAbsoluteCMApplicationDate);

      debug() << "MONICA 1: next abs app-date: "
              << nextAbsoluteCMApplicationDate.toString() << endl;
    }
    if (isSyncIC && currentCM2 &&
        nextAbsoluteCMApplicationDate2 == currentDate) {
      debug() << "MONICA 2: applying absolute-at: "
              << nextAbsoluteCMApplicationDate2.toString() << endl;
      cultivationmethod::absApply(currentCM2, nextAbsoluteCMApplicationDate2, monica2.get());

      nextAbsoluteCMApplicationDate2 =
          cultivationmethod::nextAbsDate(currentCM2, nextAbsoluteCMApplicationDate2);

      debug() << "MONICA 2: next abs app-date: "
              << nextAbsoluteCMApplicationDate2.toString() << endl;

      // calculate phRedux if set to automatic
      if (monica2->cropPs.pc_intercropping_autoPhRedux &&
          monica2->currentEvents.find("Sowing") !=
              monica2->currentEvents.end() &&
          monica->currentCropModule.get() != nullptr) {
        auto cg1 = monica->currentCropModule.get();
        // crop 1 (wheat) has not yet reached anthesis, use first part of curve
        auto anthesisStage = kj::get<1>(cropmodule::anthesisBetweenStages(cg1));
        auto dvsPhr = monica2->cropPs.pc_intercropping_dvs_phr;
        if (cg1->vc_DevelopmentalStage < anthesisStage) {
          monica->cropPs.pc_intercropping_phRedux =
              monica2->cropPs.pc_intercropping_phRedux =
                  log10(cropmodule::getCurrentTotalTemperatureSum(cg1)) /
                  dvsPhr;
        } else {
          auto tempSumAtAnthesis =
              cropmodule::sumStageTemperatureSums(cg1, 0, anthesisStage);
          auto totTempSum = cropmodule::sumStageTemperatureSums(cg1, 0, -1);
          monica->cropPs.pc_intercropping_phRedux =
              monica2->cropPs.pc_intercropping_phRedux =
                  (log10(tempSumAtAnthesis) / dvsPhr) -
                  (log10(tempSumAtAnthesis) / dvsPhr /
                   (totTempSum - tempSumAtAnthesis) *
                   (cropmodule::getCurrentTotalTemperatureSum(cg1) -
                    tempSumAtAnthesis));
        }
      }
    }

    // monica main stepping method
    if (isSyncIC) {
      if (monica2->currentCropModule) {
        monicamodel::setOtherCropHeightAndLAIt(
            monica.get(), monica2->currentCropModule->vc_CropHeight,
            monica2->currentCropModule->vc_LeafAreaIndex);
      } else {
        monicamodel::setOtherCropHeightAndLAIt(monica.get(), -1, -1);
      }
    }

    if (isSyncIC)
      debug() << "MONICA 1: ";
    monicamodel::step(monica.get());
    if (isSyncIC) {
      if (monica->currentCropModule) {
        monicamodel::setOtherCropHeightAndLAIt(
            monica2.get(), monica->currentCropModule->vc_CropHeight,
            monica->currentCropModule->vc_LeafAreaIndex);
      } else {
        monicamodel::setOtherCropHeightAndLAIt(monica2.get(), -1, -1);
      }
      debug() << "MONICA 2: ";
      // set the soil moisture of monica2's soil column to monica1's soil column
      // (after running for current day)
      if (monica->cropPs.sequentialWaterUse) {
        auto nols = monica->soilColumn->size();
        KJ_ASSERT((nols == monica2->soilColumn->size()));
        for (size_t i = 0; i < nols; i++) {
          (*(monica2->soilColumn))[i].vs_SoilMoisture_m3 =
              (*(monica->soilColumn))[i].vs_SoilMoisture_m3;
        }
      }
      monicamodel::step(monica2.get());
    }
    debug() << std::endl;

    // call all daily functions, assuming it's better to do this after the
    // steps, than before so the daily monica calculations will be taken into
    // account but means also that a workstep which gets executed before the
    // steps, can't take the values into account by applying a daily function
    for (auto &f : applyDailyFuncs)
      f();
    if (isSyncIC)
      for (auto &f : applyDailyFuncs2)
        f();

    // try to apply dynamic worksteps marked to run AFTER everything else that
    // day
    if (currentCM)
      cultivationmethod::apply(currentCM, monica.get(), false);
    if (isSyncIC && currentCM2)
      cultivationmethod::apply(currentCM2, monica2.get(), false);

    // store results
    for (auto &s : store)
      store_data_store_results_if_spec_applies(&s, *monica, returnObjOutputs);
    if (isSyncIC)
      for (auto &s : store2)
        store_data_store_results_if_spec_applies(&s, *monica2,
                                                 returnObjOutputs);

    // if the next application date is not valid, we're at the end
    // of the application list of this cultivation method
    // and go to the next one in the crop rotation
    if (currentCM && cultivationmethod::allDynamicWorkstepsFinished(currentCM) &&
        !nextAbsoluteCMApplicationDate.isValid()) {
      // to count the applied fertiliser for the next production process
      monicamodel::resetFertiliserCounter(monica.get());
      tie(currentCM, nextAbsoluteCMApplicationDate) =
          findNextCultivationMethod(currentDate + 1);
    }
    if (isSyncIC && currentCM2 && cultivationmethod::allDynamicWorkstepsFinished(currentCM2) &&
        !nextAbsoluteCMApplicationDate2.isValid()) {
      // to count the applied fertiliser for the next production process
      monicamodel::resetFertiliserCounter(monica2.get());
      tie(currentCM2, nextAbsoluteCMApplicationDate2) =
          findNextCultivationMethod2(currentDate + 1);
    }
  }

  if (env.params.simulationParameters.serializeMonicaStateAtEnd) {
    Workstep sms = makeSaveMonicaStateWorkstep(
        currentDate,
        env.params.simulationParameters.pathToSerializationAtEndFile,
        env.params.simulationParameters.serializeMonicaStateAtEndToJson,
        env.params.simulationParameters.noOfPreviousDaysSerializedClimateData);
    workstep::apply(&sms, monica.get());
  }
  // if (isSyncIC && env2.params.simulationParameters.serializeMonicaStateAtEnd)
  // { 	SaveMonicaState sms(currentDate,
  //env2.params.simulationParameters.pathToSerializationFile);
  //	sms.apply(monica2.get());
  // }

  for (auto &sd : store) {
    // aggregate results of while events or unfinished other from/to ranges
    // (where to event didn't happen yet)
    if (returnObjOutputs)
      store_data_aggregate_results_obj(&sd);
    else
      store_data_aggregate_results(&sd);
    out.data.push_back(
        {sd.spec.origSpec.dump(), sd.outputIds, sd.results, sd.resultsObj});
  }
  if (isSyncIC) {
    for (auto &sd : store2) {
      // aggregate results of while events or unfinished other from/to ranges
      // (where to event didn't happen yet)
      if (returnObjOutputs)
        store_data_aggregate_results_obj(&sd);
      else
        store_data_aggregate_results(&sd);
      out2.data.push_back(
          {sd.spec.origSpec.dump(), sd.outputIds, sd.results, sd.resultsObj});
    }
  }

  debug() << "returning from runMonica" << endl;

#ifdef TEST_HOURLY_OUTPUT
  tout(true);
#endif

  return make_pair(out, out2);
}

Output monica::runMonica(Env env) {
  return runMonicaIC(kj::mv(env), false).first;
}
