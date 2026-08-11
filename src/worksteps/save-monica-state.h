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

#include "tools/date.h"
#include "tools/helper.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct SaveMonicaStateData {
  std::string pathToFile;
  bool toJson{false};
  int noOfPreviousDaysSerializedClimateData{-1};
};

namespace workstep {

// note: merge needs ws - the original re-parses "runAtStartOfDay" with an
// explicit false default, overriding what mergeCommon already set on the common
// field (SaveMonicaState defaults to running at the *end* of the day, unlike
// every other subtype).
Tools::Errors merge(SaveMonicaStateData *sms, Workstep *ws,
                    json11::Json j);
json11::Json to_json(const SaveMonicaStateData *sms,
                     const Workstep *ws);
bool apply(SaveMonicaStateData *sms, Workstep *ws, MonicaModel *model);

} // namespace workstep

Workstep makeSaveMonicaStateWorkstep(json11::Json object);
Workstep makeSaveMonicaStateWorkstep(
    const Tools::Date &at, std::string pathToSerializedStateFile,
    bool serializeAsJson = false,
    int noOfPreviousDaysSerializedClimateData = -1);

} // namespace monica
