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

#include <string>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API SaveMonicaStateData {
  std::string pathToFile;
  bool toJson{false};
  int noOfPreviousDaysSerializedClimateData{-1};
};

namespace workstep {

// note: merge needs ws - the original re-parses "runAtStartOfDay" with an
// explicit false default, overriding what mergeCommon already set on the common
// field (SaveMonicaState defaults to running at the *end* of the day, unlike
// every other subtype).
DLL_API Tools::Errors merge(SaveMonicaStateData *sms, Workstep *ws,
                            json11::Json j);
DLL_API json11::Json to_json(const SaveMonicaStateData *sms,
                             const Workstep *ws);
DLL_API bool apply(SaveMonicaStateData *sms, Workstep *ws, MonicaModel *model);

} // namespace workstep

DLL_API Workstep makeSaveMonicaStateWorkstep(json11::Json object);
DLL_API Workstep makeSaveMonicaStateWorkstep(
    const Tools::Date &at, std::string pathToSerializedStateFile,
    bool serializeAsJson = false,
    int noOfPreviousDaysSerializedClimateData = -1);

} // namespace monica
