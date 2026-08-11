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

#include "set-value.h"

#include "../core/monica-model.h"
#include "../io/build-output.h"
#include "../run/workstep.h"
#include "json11/json11-helper.h"

using namespace std;
using namespace monica;
using namespace Tools;

Workstep monica::makeSetValueWorkstep(const Tools::Date &at, OId oid,
                                      json11::Json value) {
  Workstep ws;
  ws.date = at;
  SetValueData s;
  s.oid = oid;
  s.value = value;
  ws.data = s;
  return ws;
}

Workstep monica::makeSetValueWorkstep(json11::Json j) {
  Workstep ws;
  ws.data = SetValueData{};
  Errors res = workstep::mergeCommon(&ws, j);
  res.append(workstep::merge(&std::get<SetValueData>(ws.data), j));
  ws.errors = res;
  return ws;
}

Errors workstep::merge(SetValueData *s, json11::Json j) {
  Errors res;

  auto oids = parseOutputIds({j["var"]});
  if (!oids.empty())
    s->oid = oids[0];
  else
    return res;

  s->value = j["value"];
  if (s->value.is_array()) {
    auto jva = s->value.array_items();
    if (!jva.empty()) {
      // is an expression
      if (jva[0] == "=" && jva.size() == 4) {
        auto f =
            buildPrimitiveCalcExpression(J11Array(jva.begin() + 1, jva.end()));
        s->getValue = [f](const MonicaModel *mm) { return f(*mm); };
      } else {
        auto oids2 = parseOutputIds({s->value});
        if (!oids2.empty()) {
          auto oid = oids2[0];
          const auto &ofs = buildOutputTable().ofs;
          auto ofi = ofs.find(oid.id);
          if (ofi != ofs.end()) {
            auto f = ofi->second;
            s->getValue = [f, oid](const MonicaModel *mm) {
              return f(*mm, oid);
            };
          }
        }
      }
    }
  } else
    // NOTE: captures a copy of the value, not `s` itself - unlike the original
    // class-based code (where `this` was always a stable heap address via
    // shared_ptr, so `[=]` capturing `this` and reading `this->_value` live was
    // safe), `s` here points into a Workstep that is still a local/
    // about-to-be-returned-by-value object at this point in
    // makeSetValueWorkstep, not yet at its final stable (e.g. shared_ptr-owned)
    // address - capturing the pointer would risk it dangling after a move. A
    // value copy is behaviorally identical here since `value` is never
    // reassigned again after this merge() call for the object's lifetime.
    s->getValue = [value = s->value](const MonicaModel *) { return value; };

  return res;
}

json11::Json workstep::to_json(const SetValueData *s, const Workstep *ws) {
  return json11::Json::object{{"type", "SetValue"},
                              {"date", ws->date.toIsoDateString()},
                              {"var", s->oid.jsonInput},
                              {"value", s->value}};
}

bool workstep::apply(SetValueData *s, Workstep *ws, MonicaModel *model) {
  workstep::applyCommon(ws, model);

  if (!s->getValue)
    return true;

  const auto &setfs = buildOutputTable().setfs;
  auto ci = setfs.find(s->oid.id);
  if (ci != setfs.end()) {
    auto v = s->getValue(model);
    ci->second(*model, s->oid, v);
  }

  model->currentEvents.insert("SetValue");

  return true;
}
