/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "save-monica-state.h"

#include "../core/monica-model.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"
#include "model/monica/monica_state.capnp.h"
#include <capnp/compat/json.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/filesystem.h>
#include <kj/string.h>

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeSaveMonicaStateWorkstep(
    const Tools::Date &at, std::string pathToSerializedStateFile,
    bool serializeAsJson, int noOfPreviousDaysSerializedClimateData) {
  Workstep ws;
  ws.date = at;
  ws.runAtStartOfDay = false; // by default run at the end of the day
  SaveMonicaStateData sms;
  sms.pathToFile = std::move(pathToSerializedStateFile);
  sms.toJson = serializeAsJson;
  sms.noOfPreviousDaysSerializedClimateData =
      noOfPreviousDaysSerializedClimateData;
  ws.data = sms;
  return ws;
}

Workstep monica::makeSaveMonicaStateWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = SaveMonicaStateData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<SaveMonicaStateData>(ws.data), &ws, j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(SaveMonicaStateData *sms, Workstep *ws, json11::Json j) {
  Errors res;
  set_bool_valueD(ws->runAtStartOfDay, j, "runAtStartOfDay", false);
  set_string_value(sms->pathToFile, j, "path");
  set_bool_value(sms->toJson, j, "toJson");
  set_int_valueD(sms->noOfPreviousDaysSerializedClimateData, j,
                 "noOfPreviousDaysSerializedClimateData", -1);
  return res;
}

json11::Json workstep::to_json(const SaveMonicaStateData *sms,
                               const Workstep *ws) {
  return json11::Json::object{{"type", "SaveMonicaState"},
                              {"path", sms->pathToFile},
                              {"toJson", sms->toJson},
                              {"noOfPreviousDaysSerializedClimateData",
                               sms->noOfPreviousDaysSerializedClimateData},
                              {"runAtStartOfDay", ws->runAtStartOfDay}};
}

bool workstep::apply(SaveMonicaStateData *sms, Workstep *ws,
                     MonicaModel *model) {
  workstep::applyCommon(ws, model);

  int prevVal = -1;
  if (sms->noOfPreviousDaysSerializedClimateData > -1) {
    prevVal = model->simPs.noOfPreviousDaysSerializedClimateData;
    model->simPs.noOfPreviousDaysSerializedClimateData =
        sms->noOfPreviousDaysSerializedClimateData;
  }

  const auto pathToSerFile = kj::str(sms->pathToFile);
  auto fs = kj::newDiskFilesystem();
  auto file = isAbsolutePath(pathToSerFile.cStr())
                  ? fs->getRoot().openFile(
                        fs->getCurrentPath().eval(pathToSerFile),
                        kj::WriteMode::CREATE | kj::WriteMode::MODIFY)
                  : fs->getRoot().openFile(kj::Path::parse(pathToSerFile),
                                           kj::WriteMode::CREATE |
                                               kj::WriteMode::MODIFY);

  capnp::MallocMessageBuilder message;
  auto runtimeState =
      message.initRoot<mas::schema::model::monica::RuntimeState>();
  const auto modelState = runtimeState.initModelState();
  monicamodel::serialize(model, modelState);

  if (sms->toJson) {
    const capnp::JsonCodec json;
    const auto jStr = json.encode(runtimeState);
    file->writeAll(jStr);
  } else {
    auto flatArray = capnp::messageToFlatArray(message.getSegmentsForOutput());
    file->writeAll(flatArray.asBytes());
  }

  if (prevVal > -1)
    model->simPs.noOfPreviousDaysSerializedClimateData = prevVal;
  model->currentEvents.insert("SaveMonicaState");
  return true;
}
