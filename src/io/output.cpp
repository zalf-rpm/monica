/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg-Mohnicke <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "output.h"

#include <fstream>
#include <algorithm>
#include <mutex>

#include "json11/json11-helper.h"
#include "tools/debug.h"
#include "tools/helper.h"
#include "tools/algorithms.h"

using namespace monica;
using namespace Tools;
using namespace std;
using namespace json11;


OId monica::makeOId(int id) {
  OId oid;
  oid.id = id;
  return oid;
}

OId monica::makeOId(int id, OId::ORGAN organ) {
  OId oid;
  oid.id = id;
  oid.organ = organ;
  return oid;
}

OId monica::makeOId(int id, OId::OP layerAgg) {
  OId oid;
  oid.id = id;
  oid.layerAggOp = layerAgg;
  oid.fromLayer = 0;
  oid.toLayer = 20;
  return oid;
}

OId monica::makeOId(int id, OId::OP layerAgg, OId::OP timeAgg) {
  OId oid;
  oid.id = id;
  oid.layerAggOp = layerAgg;
  oid.timeAggOp = timeAgg;
  oid.fromLayer = 0;
  oid.toLayer = 20;
  return oid;
}

OId monica::makeOId(int id, int from, int to, OId::OP layerAgg) {
  OId oid;
  oid.id = id;
  oid.layerAggOp = layerAgg;
  oid.fromLayer = from;
  oid.toLayer = to;
  return oid;
}

OId monica::makeOId(int id, int from, int to, OId::OP layerAgg, OId::OP timeAgg) {
  OId oid;
  oid.id = id;
  oid.layerAggOp = layerAgg;
  oid.timeAggOp = timeAgg;
  oid.fromLayer = from;
  oid.toLayer = to;
  return oid;
}

OId monica::makeOId(json11::Json object) {
  OId oid;
  oid::merge(&oid, object);
  return oid;
}

Errors oid::merge(OId* oid, json11::Json j)
{
  set_int_value(oid->id, j, "id");
  set_string_value(oid->name, j, "name");
  set_string_value(oid->displayName, j, "displayName");
  set_string_value(oid->unit, j, "unit");
  set_string_value(oid->jsonInput, j, "jsonInput");

  oid->layerAggOp = OId::OP(int_valueD(j, "layerAggOp", OId::NONE));
  oid->timeAggOp = OId::OP(int_valueD(j, "timeAggOp", OId::AVG));

  oid->organ = OId::ORGAN(int_valueD(j, "organ", OId::_UNDEFINED_ORGAN_));

  set_int_value(oid->fromLayer, j, "fromLayer");
  set_int_value(oid->toLayer, j, "toLayer");

  return{};
}

json11::Json oid::to_json(const OId* oid)
{
  return json11::Json::object
  {{"type", "OId"}
  ,{"id", oid->id}
  ,{"name", oid->name}
  ,{"displayName", oid->displayName}
  ,{"unit", oid->unit}
  ,{"jsonInput", oid->jsonInput}
  ,{"layerAggOp", int(oid->layerAggOp)}
  ,{"timeAggOp", int(oid->timeAggOp)}
  ,{"organ", int(oid->organ)}
  ,{"fromLayer", oid->fromLayer}
  ,{"toLayer", oid->toLayer}
  };
}


std::string oid::toString(const OId* oid, bool includeTimeAgg)
{
  ostringstream oss;
  oss << "[";
  oss << oid->name;
  if(isOrgan(oid))
    oss << ", " << toString(oid, oid->organ);
  else if(isRange(oid))
    oss << ", [" << (oid->fromLayer + 1) << ", " << (oid->toLayer + 1)
    << (oid->layerAggOp != OId::NONE ? string(", ") + toString(oid, oid->layerAggOp) : "")
    << "]";
  else if(oid->fromLayer >= 0)
    oss << ", " << (oid->fromLayer + 1);
  if(includeTimeAgg)
    oss << ", " << toString(oid, oid->timeAggOp);
  oss << "]";

  return oss.str();
}

std::string oid::toString(const OId* oid, OId::OP op)
{
  string res("undef");
  switch(op)
  {
  case OId::AVG: res = "AVG"; break;
  case OId::MEDIAN: res = "MEDIAN"; break;
  case OId::SUM: res = "SUM"; break;
  case OId::MIN: res = "MIN"; break;
  case OId::MAX: res = "MAX"; break;
  case OId::FIRST: res = "FIRST"; break;
  case OId::LAST: res = "LAST"; break;
  case OId::NONE: res = "NONE"; break;
  case OId::_UNDEFINED_OP_:
  default:;
  }
  return res;
}

std::string oid::toString(const OId* oid, OId::ORGAN organ)
{
  string res("undef");
  switch(organ)
  {
  case OId::ROOT: res = "Root"; break;
  case OId::LEAF: res = "Leaf"; break;
  case OId::SHOOT: res = "Shoot"; break;
  case OId::FRUIT: res = "Fruit"; break;
  case OId::STRUCT: res = "Struct"; break;
  case OId::SUGAR: res = "Sugar"; break;
  case OId::_UNDEFINED_ORGAN_:
  default:;
  }
  return res;
}

std::string oid::outputName(const OId* oid)
{
  string outName = oid->name;
  if(isOrgan(oid)) outName = outName + "/" + toString(oid, oid->organ);
  if(!oid->displayName.empty())
    outName = oid->displayName;
  return outName;
}

Output monica::makeOutput(std::string error) {
  Output output;
  output.errors.push_back(error);
  return output;
}

Output monica::makeOutput(json11::Json object) {
  Output output;
  output::merge(&output, object);
  return output;
}

Errors output::merge(Output* output, json11::Json j)
{
  Errors es;

  output->customId = j["customId"];// .string_value();

  for(const auto& d : j["data"].array_items())
  {
    vector<J11Array> vs;
    vector<J11Object> os;
    for(auto& j : d["results"].array_items())
    {
      if(j.is_array())
        vs.push_back(j.array_items());
      else if(j.is_object())
        os.push_back(j.object_items());
    }
    vector<OId> outputIds;
    for(Json oidj : d["outputIds"].array_items())
    {
      OId o;
      es.append(oid::merge(&o, oidj));
      outputIds.push_back(o);
    }
    output->data.push_back({d["origSpec"].string_value(), outputIds, vs, os});
  }

  output->errors = toStringVector(j["errors"]);
  output->warnings = toStringVector(j["warnings"]);

  return es;
}

json11::Json output::to_json(const Output* output)
{
  J11Array ds;
  for(const auto& d : output->data)
  {
    J11Array rs;
    if(!d.results.empty())
      for(auto r : d.results)
        rs.push_back(r);
    else if(!d.resultsObj.empty())
      for(auto o : d.resultsObj)
        rs.push_back(o);
    J11Array outputIds;
    for(const auto& o : d.outputIds) outputIds.push_back(oid::to_json(&o));
    ds.push_back(J11Object
    {{"origSpec", d.origSpec}
    ,{"outputIds", outputIds}
    ,{"results", rs}
    });
  }

  return json11::Json::object
  {{"type", "Output"}
  ,{"customId", output->customId}
  ,{"data", ds}
  ,{"errors", toPrimJsonArray(output->errors)}
  ,{"warnings", toPrimJsonArray(output->warnings)}
  };
}
