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
#include "kj/common.h"
#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "json11/json11-helper.h"
#include "climate/climate-common.h"

namespace monica {
  struct DLL_API OId : public Tools::Json11Serializable {
    enum OP { AVG, MEDIAN, SUM, MIN, MAX, FIRST, LAST, NONE, UNDEFINED_OP_ };

    enum ORGAN { ROOT = 0, LEAF, SHOOT, FRUIT, STRUCT, SUGAR, UNDEFINED_ORGAN_ };

    OId() = default;

    explicit OId(int id)
      : id(id)
    {}

    OId(int id, ORGAN organ)
      : id(id), organ(organ)
    {}

    OId(int id, OP layerAgg)
      : id(id), layerAggOp(layerAgg), fromLayer(0), toLayer(20)
    {}

    //! id, layer aggregation and time aggregation, shortcut for aggregating all layers in non daily setting
    OId(int id, OP layerAgg, OP timeAgg)
      : id(id), layerAggOp(layerAgg), timeAggOp(timeAgg), fromLayer(0), toLayer(20)
    {}

    //! id, layer aggregation of from to (incl) to layers
    OId(int id, int from, int to, OP layerAgg)
      : id(id), layerAggOp(layerAgg), fromLayer(from), toLayer(to)
    {}

    //! aggregate layers from to (incl) to in a non daily setting
    OId(int id, int from, int to, OP layerAgg, OP timeAgg)
      : id(id), layerAggOp(layerAgg), timeAggOp(timeAgg), fromLayer(from), toLayer(to)
    {}

    explicit OId(json11::Json object);

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    bool isRange() const { return fromLayer >= 0 && toLayer >= 0; }// && fromLayer < toLayer; }

    bool isOrgan() const { return organ != UNDEFINED_ORGAN_; }

    virtual std::string toString(bool includeTimeAgg = false) const;

    std::string toString(OId::OP op) const;
    std::string toString(OId::ORGAN organ) const;

    std::string outputName() const;

    int id{-1};
    std::string name;
    std::string displayName;
    std::string unit;
    std::string jsonInput;
    OP layerAggOp{NONE}; //! aggregate values on potentially daily basis (e.g. soil layers)
    OP timeAggOp{AVG}; //! aggregate values in a second time range (e.g. monthly)
    ORGAN organ{UNDEFINED_ORGAN_};
    int fromLayer{-1}, toLayer{-1};
    kj::Maybe<int> roundToDigits;
    kj::Maybe<std::string> cropId;
  };

  struct DLL_API Output : public Tools::Json11Serializable
  {
    Output() = default;

    explicit Output(std::string error) { errors.push_back(error); }

    explicit Output(json11::Json object);

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;
    
    //std::string customId;
    json11::Json customId;

    struct Data
    {
      std::string origSpec;
      std::vector<OId> outputIds;
      std::vector<Tools::J11Array> results;
      std::vector<Tools::J11Object> resultsObj;
    };
    std::vector<Data> data;

    std::vector<std::string> errors;
    std::vector<std::string> warnings;
  };

}  // namespace monica

