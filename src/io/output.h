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

#include "json11/json11-helper.h"

namespace monica {
struct OId {
  enum OP { AVG, MEDIAN, SUM, MIN, MAX, FIRST, LAST, NONE, _UNDEFINED_OP_ };

  enum ORGAN { ROOT = 0, LEAF, SHOOT, FRUIT, STRUCT, SUGAR, _UNDEFINED_ORGAN_ };

  int id{-1};
  std::string name;
  std::string displayName;
  std::string unit;
  std::string jsonInput;
  OP layerAggOp{NONE}; //! aggregate values on potentially daily basis (e.g. soil layers)
  OP timeAggOp{AVG};   //! aggregate values in a second time range (e.g. monthly)
  ORGAN organ{_UNDEFINED_ORGAN_};
  int fromLayer{-1}, toLayer{-1};
};

//! just name
OId makeOId(int id);
//! id and organ
OId makeOId(int id, OId::ORGAN organ);
//! id and layer aggregation
OId makeOId(int id, OId::OP layerAgg);
//! id, layer aggregation and time aggregation, shortcut for aggregating all
//! layers in non daily setting
OId makeOId(int id, OId::OP layerAgg, OId::OP timeAgg);
//! id, layer aggregation of from to (incl) to layers
OId makeOId(int id, int from, int to, OId::OP layerAgg);
//! aggregate layers from to (incl) to in a non daily setting
OId makeOId(int id, int from, int to, OId::OP layerAgg, OId::OP timeAgg);
OId makeOId(json11::Json object);

namespace oid {

Tools::Errors merge(OId *oid, json11::Json j);
json11::Json to_json(const OId *oid);

inline bool isRange(const OId *oid) {
  return oid->fromLayer >= 0 && oid->toLayer >= 0;
} // && fromLayer < toLayer; }

inline bool isOrgan(const OId *oid) { return oid->organ != OId::_UNDEFINED_ORGAN_; }

std::string toString(const OId *oid, bool includeTimeAgg = false);

std::string toString(const OId *oid, OId::OP op);
std::string toString(const OId *oid, OId::ORGAN organ);

std::string outputName(const OId *oid);

} // namespace oid

struct Output {
  // std::string customId;
  json11::Json customId;

  struct Data {
    std::string origSpec;
    std::vector<OId> outputIds;
    std::vector<Tools::J11Array> results;
    std::vector<Tools::J11Object> resultsObj;
  };
  std::vector<Data> data;

  std::vector<std::string> errors;
  std::vector<std::string> warnings;
};

Output makeOutput(std::string error);
Output makeOutput(json11::Json object);

namespace output {

Tools::Errors merge(Output *output, json11::Json j);
json11::Json to_json(const Output *output);

} // namespace output

} // namespace monica
