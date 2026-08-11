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

#include <functional>

#include "json11/json11.hpp"

#include "../io/output.h"
#include "common/dll-exports.h"
#include "json11/json11-helper.h"
#include "tools/date.h"

namespace monica {
class MonicaModel;
struct Workstep;

struct DLL_API SetValueData {
  OId oid;
  json11::Json value;
  std::function<json11::Json(const monica::MonicaModel *)> getValue;
};

namespace workstep {

DLL_API Tools::Errors merge(SetValueData *s, json11::Json j);
DLL_API json11::Json to_json(const SetValueData *s, const Workstep *ws);
DLL_API bool apply(SetValueData *s, Workstep *ws, MonicaModel *model);

} // namespace workstep

DLL_API Workstep makeSetValueWorkstep(json11::Json object);
DLL_API Workstep makeSetValueWorkstep(const Tools::Date &at, OId oid, json11::Json value);

} // namespace monica
