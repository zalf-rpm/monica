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
#include "json11/json11-helper.h"
#include "climate/climate-common.h"
#include "tools/date.h"
#include "../core/monica-model.h"
#include "output.h"

namespace monica
{
  struct OutputMetadata
  {
    int id;
    std::string name;
    std::string unit;
    std::string description;
  };

  //---------------------------------------------------------------------------

  double applyOIdOP(OId::OP op, const std::vector<double>& vs);

  json11::Json applyOIdOP(OId::OP op, const std::vector<json11::Json>& js);

  DLL_API std::vector<OId> parseOutputIds(const Tools::J11Array& oidArray);

  struct DLL_API BOTRes
  {
    std::map<int, std::function<json11::Json(const MonicaModel&, const OId&)>> ofs;
    std::map<int, std::function<void(MonicaModel&, const OId&, json11::Json)>> setfs;
    std::map<std::string, OutputMetadata> name2metadata;
  };
  DLL_API BOTRes& buildOutputTable();

  //----------------------------------------------------------------------------

  std::function<bool(double, double)> getCompareOp(std::string opStr);
  bool applyCompareOp(std::function<bool(double, double)> op, json11::Json lj, json11::Json rj);

  std::function<double(double, double)> getPrimitiveCalcOp(std::string opStr);
  json11::Json applyPrimitiveCalcOp(std::function<double(double, double)> op, json11::Json lj, json11::Json rj);

  
  template<typename OP_RT, typename APPLY_RT>
  std::function<APPLY_RT(const monica::MonicaModel&)>
    buildExpression(Tools::J11Array a,
                    std::function<std::function<OP_RT(double, double)>(std::string)> getOp,
                    std::function<APPLY_RT(std::function<OP_RT(double, double)>, json11::Json, json11::Json)> applyOp)
  {
    if(a.size() == 3
       && (a[0].is_number() || a[0].is_string() || a[0].is_array())
       && a[1].is_string()
       && (a[2].is_number() || a[2].is_string() || a[2].is_array()))
    {
      json11::Json leftj = a[0];
      std::string ops = a[1].string_value();
      json11::Json rightj = a[2];

      auto op = getOp(ops);

      const auto& ofs = buildOutputTable().ofs;
      typename decltype(buildOutputTable().ofs)::mapped_type lf, rf;
      monica::OId loid, roid;
      if(!leftj.is_number())
      {
        auto loids = parseOutputIds({leftj});
        if(!loids.empty())
        {
          loid = loids.front();
          auto ofi = ofs.find(loid.id);
          if(ofi != ofs.end())
            lf = ofi->second;
        }
      }
      if(!rightj.is_number())
      {
        auto roids = parseOutputIds({rightj});
        if(!roids.empty())
        {
          roid = roids.front();
          auto ofi = ofs.find(roid.id);
          if(ofi != ofs.end())
            rf = ofi->second;
        }
      }

      if(lf && rf && op)
      {
        return [=](const monica::MonicaModel& m)
        {
          return applyOp(op, lf(m, loid), rf(m, roid));
        };
      }
      else if(lf && rightj.is_number() && op)
      {
        return [=](const monica::MonicaModel& m)
        {
          return applyOp(op, lf(m, loid), rightj);
        };
      }
      else if(leftj.is_number() && rf && op)
      {
        return [=](const monica::MonicaModel& m)
        {
          return applyOp(op, leftj, rf(m, roid));
        };
      }
    }

    return std::function<APPLY_RT(const monica::MonicaModel&)>();
  }

  inline std::function<bool(const monica::MonicaModel&)> buildCompareExpression(Tools::J11Array a)
  {
    return buildExpression<bool, bool>(a, getCompareOp, applyCompareOp);
  }

  inline std::function<json11::Json(const monica::MonicaModel&)> buildPrimitiveCalcExpression(Tools::J11Array a)
  {
    return buildExpression<double, json11::Json>(a, getPrimitiveCalcOp, applyPrimitiveCalcOp);
  }

} // namespace monica
