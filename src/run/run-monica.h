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

#pragma once

#include <vector>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "../core/monica-model.h"
#include "cultivation-method.h"
#include "climate/climate-common.h"
#include "../io/output.h"

namespace monica
{
struct CropRotation {
  Tools::Date start, end;
  std::vector<CultivationMethod> cropRotation;
};

CropRotation makeCropRotation(Tools::Date start, Tools::Date end, std::vector<CultivationMethod> cropRotation);

Tools::Errors crop_rotation_merge(CropRotation* cr, json11::Json j);

json11::Json crop_rotation_to_json(const CropRotation* cr);

struct DLL_API Env {
  //! object holding the climate data
  Climate::DataAccessor climateData;
  // 1. priority, object holding the climate data

  std::string climateCSV;
  // 2nd priority, if climateData not valid, try to read climate data from csv string

  std::vector<std::string> pathsToClimateCSV;
  // 3. priority, if climateCSV is empty, try to load climate data from vector of file paths

  json11::Json csvViaHeaderOptions;
  // the csv options for reading/parsing csv data

  std::vector<CultivationMethod> cropRotation, cropRotation2;
  // vector of elements holding the data of the single crops in the rotation

  std::vector<CropRotation> cropRotations, cropRotations2;
  // optionally

  json11::Json events, events2;
  // the events section defining the requested outputs

  json11::Json outputs;
  // the whole outputs section

  json11::Json customId;
  // id identifiying this particular run

  std::string sharedId;
  // shared id between runs belonging together

  CentralParameterProvider params;

  std::string berestRequestAddress;

  bool debugMode{false};

  Intercropping ic;
};

DLL_API Env makeEnv(CentralParameterProvider&& cpp);

DLL_API Tools::Errors env_merge(Env* env, json11::Json j);
// merge a json file into Env

DLL_API json11::Json env_to_json(const Env* env);
// serialize to json

DLL_API std::string env_to_string(const Env* env);

//! is the output as a list (e.g. days) of an object (holding all the requested data)
DLL_API bool env_return_obj_outputs(const Env* env);


struct Spec {
  json11::Json origSpec;

  std::function<bool(const MonicaModel&)> startf;
  std::function<bool(const MonicaModel&)> endf;
  std::function<bool(const MonicaModel&)> fromf;
  std::function<bool(const MonicaModel&)> tof;
  std::function<bool(const MonicaModel&)> atf;
  std::function<bool(const MonicaModel&)> whilef;
};

Spec makeSpec(json11::Json j);

Tools::Errors spec_merge(Spec* spec, json11::Json j);

std::function<bool(const MonicaModel&)> spec_create_expression_func(json11::Json j);

json11::Json spec_to_json(const Spec* spec);

struct StoreData {
  Tools::Maybe<bool> withinEventStartEndRange;
  Tools::Maybe<bool> withinEventFromToRange;
  Spec spec;
  std::vector<OId> outputIds;
  std::vector<Tools::J11Array> intermediateResults;
  std::vector<Tools::J11Array> results;
  std::vector<Tools::J11Object> resultsObj;
};

void store_data_aggregate_results(StoreData* sd);

void store_data_aggregate_results_obj(StoreData* sd);

void store_data_store_results_if_spec_applies(StoreData* sd, const MonicaModel& monica, bool storeObjOutputs = false);

std::vector<StoreData> setupStorage(const json11::Json& event2oids, const Tools::Date& startDate, const Tools::Date& endDate);

//! main function for running monica under a given Env(ironment)
//! @param env the environment completely defining what the model needs and gets
//! @return a structure with all the Monica results
DLL_API std::pair<Output, Output> runMonicaIC(Env env, bool isIntercropping = true);
DLL_API Output runMonica(Env env);

} // namespace monica
